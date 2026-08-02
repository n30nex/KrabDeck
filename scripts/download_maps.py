#!/usr/bin/env python3
"""
KrabOS Map Downloader

Downloads NRCan WMS or owner-permitted XYZ tiles for a region and zoom range,
outputting PNG tiles in the directory structure expected by the firmware:
    tiles/{z}/{x}/{y}.png

Copy the resulting 'tiles/' directory to the root of the T-Deck SD card.

Usage examples:
    # Download a Canadian region from the built-in NRCan source
    python3 download_maps.py --city toronto --zoom 10 14

    # Download London, zooms 10-15 only
    python3 download_maps.py --name london --bbox "51.3,-0.5,51.7,0.3" --zoom 10 15

    # Use another source only when its terms permit offline bulk caching
    python3 download_maps.py --city toronto --server generic \
      --url-template 'https://tiles.example/{z}/{x}/{y}.png' \
      --attribution 'Example Maps'

Tile servers:
    nrcan     Natural Resources Canada CBMT WMS (default)
    generic   Explicit HTTPS XYZ template supplied by the operator

Dependencies: pip install requests
License: GPL-3.0-or-later
Copyright (C) 2025 Ben
"""

import argparse
import math
import os
import sys
import time
import json
import ipaddress
import struct
from urllib.parse import urlsplit
from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock

try:
    import requests
except ImportError:
    print("ERROR: 'requests' required. Install: pip install requests")
    sys.exit(1)

# ── Tile servers ────────────────────────────────────────────

TILE_SERVERS = {
    "nrcan": {
        "kind": "wms",
        "url": (
            "https://maps.geogratis.gc.ca/wms/CBMT?SERVICE=WMS&VERSION=1.1.1"
            "&REQUEST=GetMap&LAYERS=CBMT&STYLES=&SRS=EPSG:3857"
            "&BBOX={bbox}&WIDTH=256&HEIGHT=256&FORMAT=image/png"
        ),
        "attribution": "Natural Resources Canada, Canada Base Map Transportation",
        "max_zoom": 18,
        "user_agent": "KrabOS-MapDownloader/1.0",
    },
}

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
MAX_PNG_BYTES = 320 * 1024

CITIES = {
    "london": (51.3, -0.5, 51.7, 0.3),
    "toronto": (43.55, -79.65, 43.85, -79.15),
    "montreal": (45.4, -73.75, 45.65, -73.45),
    "vancouver": (49.15, -123.3, 49.35, -122.95),
    "ottawa": (45.25, -76.05, 45.55, -75.45),
    "new-york": (40.55, -74.15, 40.9, -73.7),
    "seattle": (47.45, -122.45, 47.8, -122.15),
    "chicago": (41.7, -88.0, 42.05, -87.5),
    "los-angeles": (33.85, -118.55, 34.2, -118.15),
    "san-francisco": (37.65, -122.55, 37.9, -122.3),
    "manchester": (53.35, -2.45, 53.55, -2.1),
    "birmingham": (52.35, -2.05, 52.55, -1.7),
    "leeds": (53.7, -1.7, 53.9, -1.4),
    "liverpool": (53.3, -3.1, 53.5, -2.8),
    "bristol": (51.35, -2.75, 51.55, -2.45),
    "edinburgh": (55.85, -3.35, 56.05, -3.05),
    "glasgow": (55.75, -4.4, 55.95, -4.1),
    "cardiff": (51.4, -3.3, 51.55, -3.05),
    "belfast": (54.5, -6.1, 54.7, -5.8),
    "teesside": (54.45, -1.45, 54.65, -1.05),
    "newcastle": (54.9, -1.75, 55.05, -1.45),
    "sheffield": (53.3, -1.6, 53.45, -1.35),
    "nottingham": (52.85, -1.3, 53.05, -1.05),
    "southampton": (50.85, -1.5, 50.95, -1.3),
    "brighton": (50.75, -0.25, 50.9, -0.05),
    "cambridge": (52.1, 0.05, 52.25, 0.2),
    "oxford": (51.7, -1.35, 51.8, -1.15),
}

# ── Tile math (Web Mercator) ────────────────────────────────

def latlon_to_tile(lat, lon, zoom):
    """Convert lat/lon to tile x/y at given zoom level."""
    n = 1 << zoom
    x = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    y = int((1.0 - math.log(math.tan(lat_rad) + 1.0 / math.cos(lat_rad)) / math.pi) / 2.0 * n)
    return x, y

def tile_to_latlon(x, y, zoom):
    """Convert tile x/y back to lat/lon (top-left corner)."""
    n = 1 << zoom
    lon = x / n * 360.0 - 180.0
    lat_rad = math.atan(math.sinh(math.pi * (1.0 - 2.0 * y / n)))
    lat = math.degrees(lat_rad)
    return lat, lon

def tiles_for_bbox(lat1, lon1, lat2, lon2, zoom):
    """Yield all (x, y, zoom) tile coordinates covering a bounding box."""
    # Normalize: lat1/lon1 = southwest, lat2/lon2 = northeast
    min_lat, max_lat = min(lat1, lat2), max(lat1, lat2)
    min_lon, max_lon = min(lon1, lon2), max(lon1, lon2)

    # Clamp to valid range
    min_lat = max(min_lat, -85.0511)
    max_lat = min(max_lat, 85.0511)
    min_lon = max(min_lon, -180.0)
    max_lon = min(max_lon, 180.0)

    tx1, ty1 = latlon_to_tile(max_lat, min_lon, zoom)  # top-left
    tx2, ty2 = latlon_to_tile(min_lat, max_lon, zoom)  # bottom-right

    n = 1 << zoom
    tx1 = max(0, tx1)
    ty1 = max(0, ty1)
    tx2 = min(n - 1, tx2)
    ty2 = min(n - 1, ty2)

    count = (tx2 - tx1 + 1) * (ty2 - ty1 + 1)
    for x in range(tx1, tx2 + 1):
        for y in range(ty1, ty2 + 1):
            yield x, y, zoom

    return count

# ── Download logic ──────────────────────────────────────────

stats_lock = Lock()
download_stats = {"done": 0, "failed": 0, "total": 0}
session = None

def init_session(server_config):
    global session
    session = requests.Session()
    session.headers.update({
        "User-Agent": server_config.get("user_agent", "KrabOS/1.0"),
        "Accept": "image/png",
    })
    return session

def mercator_bbox(x, y, zoom):
    """Return an EPSG:3857 WMS BBOX for one slippy-map tile."""
    origin = 20037508.342789244
    width = origin * 2 / (1 << zoom)
    min_x = -origin + x * width
    max_x = min_x + width
    max_y = origin - y * width
    min_y = max_y - width
    return f"{min_x:.3f},{min_y:.3f},{max_x:.3f},{max_y:.3f}"


def https_template_valid(template):
    """Reject malformed, local, credential-bearing, or incomplete templates."""
    if not template or any(token not in template for token in ("{z}", "{x}", "{y}")):
        return False
    expanded = template.replace("{z}", "0").replace("{x}", "0").replace("{y}", "0")
    if "{" in expanded or "}" in expanded or any(ord(char) <= 32 for char in expanded):
        return False
    parsed = urlsplit(expanded)
    if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
        return False
    if parsed.fragment or parsed.hostname == "localhost" or parsed.hostname.endswith(".local"):
        return False
    try:
        address = ipaddress.ip_address(parsed.hostname)
    except ValueError:
        return True
    return not (address.is_private or address.is_loopback or address.is_link_local or
                address.is_multicast or address.is_unspecified or address.is_reserved)


def build_tile_url(server_config, x, y, zoom):
    template = server_config["url"]
    if server_config.get("kind") == "wms":
        return template.replace("{bbox}", mercator_bbox(x, y, zoom))
    return template.replace("{z}", str(zoom)).replace("{x}", str(x)).replace("{y}", str(y))


def server_description_for_log(server_name, server_config):
    """Describe a source without exposing custom URL path/query credentials."""
    if server_name == "generic":
        return "generic (custom HTTPS source; URL redacted)"
    return f"{server_name} ({server_config['url']})"


def valid_png(data):
    """Match the firmware's bounded 256px PNG admission contract."""
    return (
        24 <= len(data) <= MAX_PNG_BYTES
        and data.startswith(PNG_SIGNATURE)
        and data[12:16] == b"IHDR"
        and struct.unpack(">II", data[16:24]) == (256, 256)
    )


def download_tile(args):
    """Download a single PNG tile. Returns (x, y, zoom, success)."""
    x, y, zoom, server_config, output_dir, resume = args
    global session, download_stats

    url = build_tile_url(server_config, x, y, zoom)

    # Output path
    out_dir = os.path.join(output_dir, str(zoom), str(x))
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f"{y}.png")

    # Resume keeps a prior non-empty tile. The default refreshes it.
    if resume and os.path.exists(out_path):
        with open(out_path, "rb") as existing_tile:
            existing_valid = valid_png(existing_tile.read())
        if existing_valid:
            with stats_lock:
                download_stats["done"] += 1
            return x, y, zoom, True

    # Download with retries
    max_retries = 3
    for attempt in range(max_retries):
        try:
            resp = session.get(url, timeout=30)
            if resp.status_code == 200 and valid_png(resp.content):
                content_type = getattr(resp, "headers", {}).get("Content-Type", "")
                if content_type and not content_type.lower().startswith("image/png"):
                    raise ValueError("server response is not image/png")
                part_path = out_path + ".part"
                with open(part_path, "wb") as f:
                    f.write(resp.content)
                    f.flush()
                    os.fsync(f.fileno())
                os.replace(part_path, out_path)

                with stats_lock:
                    download_stats["done"] += 1
                return x, y, zoom, True
            elif resp.status_code == 404:
                # A refresh never deletes an existing offline tile.
                with stats_lock:
                    download_stats["done"] += 1
                return x, y, zoom, True
            elif resp.status_code == 429:
                time.sleep(2 * (attempt + 1))
            else:
                time.sleep(1 * (attempt + 1))
        except Exception:
            if attempt == max_retries - 1:
                with stats_lock:
                    download_stats["failed"] += 1
                return x, y, zoom, False
            time.sleep(1 * (attempt + 1))

    with stats_lock:
        download_stats["failed"] += 1
    return x, y, zoom, False

# ── Metadata ────────────────────────────────────────────────

def write_metadata(output_dir, name, server_config, lat1, lon1, lat2, lon2, zoom_min, zoom_max):
    """Atomically write metadata.json describing the downloaded area."""
    min_lat, max_lat = min(lat1, lat2), max(lat1, lat2)
    min_lon, max_lon = min(lon1, lon2), max(lon1, lon2)

    metadata = {
        "name": name,
        "attribution": server_config["attribution"],
        "bounds": [min_lon, min_lat, max_lon, max_lat],
        "zoom_range": [zoom_min, zoom_max],
        "format": "png",
        "tile_size": 256,
    }

    meta_path = os.path.join(output_dir, "metadata.json")
    temporary_path = meta_path + ".tmp"
    with open(temporary_path, "w") as f:
        json.dump(metadata, f, indent=2)
        f.write("\n")
        f.flush()
        os.fsync(f.fileno())
    os.replace(temporary_path, meta_path)

    return metadata


def build_tile_index(output_dir):
    """Return validated per-zoom coverage for tiles already on disk."""
    entries = []
    if not os.path.isdir(output_dir):
        return entries

    zoom_names = sorted(
        (name for name in os.listdir(output_dir) if name.isdecimal()),
        key=int,
    )
    for zoom_name in zoom_names:
        zoom = int(zoom_name)
        if zoom < 0 or zoom > 18:
            continue
        zoom_dir = os.path.join(output_dir, zoom_name)
        if not os.path.isdir(zoom_dir):
            continue

        tiles_per_axis = 1 << zoom
        min_x = min_y = None
        max_x = max_y = None
        sample_x = sample_y = None
        count = 0
        x_names = sorted(
            (name for name in os.listdir(zoom_dir) if name.isdecimal()),
            key=int,
        )
        for x_name in x_names:
            x = int(x_name)
            x_dir = os.path.join(zoom_dir, x_name)
            if x < 0 or x >= tiles_per_axis or not os.path.isdir(x_dir):
                continue
            y_names = sorted(os.listdir(x_dir))
            for filename in y_names:
                stem, extension = os.path.splitext(filename)
                if extension.lower() != ".png" or not stem.isdecimal():
                    continue
                y = int(stem)
                path = os.path.join(x_dir, filename)
                if (y < 0 or y >= tiles_per_axis or
                        not os.path.isfile(path) or os.path.getsize(path) == 0):
                    continue
                if sample_x is None:
                    sample_x, sample_y = x, y
                min_x = x if min_x is None else min(min_x, x)
                max_x = x if max_x is None else max(max_x, x)
                min_y = y if min_y is None else min(min_y, y)
                max_y = y if max_y is None else max(max_y, y)
                count += 1

        if count:
            entries.append({
                "z": zoom,
                "min_x": min_x,
                "max_x": max_x,
                "min_y": min_y,
                "max_y": max_y,
                "sample_x": sample_x,
                "sample_y": sample_y,
                "count": count,
            })
    return entries


def write_tile_index(output_dir):
    """Atomically write the bounded index consumed by map discovery."""
    index = {
        "version": 1,
        "tile_size": 256,
        "zooms": build_tile_index(output_dir),
    }
    index_path = os.path.join(output_dir, "index.json")
    temporary_path = index_path + ".tmp"
    with open(temporary_path, "w") as index_file:
        json.dump(index, index_file, indent=2, sort_keys=True)
        index_file.write("\n")
        index_file.flush()
        os.fsync(index_file.fileno())
    os.replace(temporary_path, index_path)
    return index

# ── Main ────────────────────────────────────────────────────

def build_parser():
    parser = argparse.ArgumentParser(
        description="Download map tiles for KrabOS offline use",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--name", help="Region name (defaults to --city)")
    parser.add_argument("--output", default=None, help="Output directory (default: ./maps-{name})")

    # Bounding box: either --bbox "lat1,lon1,lat2,lon2" or individual --lat1/--lon1/--lat2/--lon2
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--bbox", help="Bounding box: 'lat1,lon1,lat2,lon2'")
    group.add_argument("--city", help="City name for quick download (e.g. 'London', 'Manchester')")

    parser.add_argument("--lat1", type=float, default=None)
    parser.add_argument("--lon1", type=float, default=None)
    parser.add_argument("--lat2", type=float, default=None)
    parser.add_argument("--lon2", type=float, default=None)

    parser.add_argument("--zoom", type=int, nargs=2, metavar=("MIN", "MAX"),
                        default=[10, 14], help="Zoom range (default: 10 14)")

    parser.add_argument("--server", choices=["nrcan", "generic"],
                        default="nrcan", help="Tile server (default: nrcan)")
    parser.add_argument("--url-template",
                        help="Permitted generic HTTPS XYZ template")
    parser.add_argument("--attribution",
                        help="Required attribution for a generic source")
    parser.add_argument("--workers", type=int, default=4,
                        help="Parallel download threads (default: 4)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Calculate tile count without downloading")
    parser.add_argument("--resume", action="store_true",
                        help="Skip already-downloaded tiles")
    return parser


def parse_args(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)

    coordinates = (args.lat1, args.lon1, args.lat2, args.lon2)
    any_coordinates = any(value is not None for value in coordinates)
    all_coordinates = all(value is not None for value in coordinates)
    if any_coordinates and not all_coordinates:
        parser.error("coordinate mode requires --lat1, --lon1, --lat2, and --lon2")
    modes = int(args.bbox is not None) + int(args.city is not None) + int(all_coordinates)
    if modes != 1:
        parser.error("choose exactly one location mode: --city, --bbox, or all four coordinates")
    if (args.bbox is not None or all_coordinates) and not args.name:
        parser.error("--name is required with --bbox or coordinate mode")
    if args.workers < 1:
        parser.error("--workers must be at least 1")
    if not (0 <= min(args.zoom) <= max(args.zoom) <= 18):
        parser.error("--zoom values must be in the firmware range 0..18")
    if args.server == "generic":
        if not args.attribution or not https_template_valid(args.url_template):
            parser.error(
                "generic sources require --attribution and a safe HTTPS "
                "--url-template containing {z}, {x}, and {y}")

    # Parse bounding box
    if args.bbox is not None:
        try:
            parts = [float(x.strip()) for x in args.bbox.split(",")]
        except ValueError:
            parser.error("--bbox values must be numbers")
        if len(parts) != 4:
            parser.error("--bbox needs exactly 4 values: lat1,lon1,lat2,lon2")
        lat1, lon1, lat2, lon2 = parts
    elif args.city:
        key = args.city.lower()
        if key not in CITIES:
            parser.error(f"unknown city '{args.city}'. Known: {', '.join(sorted(CITIES))}")
        lat1, lon1, lat2, lon2 = CITIES[key]
        args.name = args.name or key
    else:
        lat1, lon1, lat2, lon2 = args.lat1, args.lon1, args.lat2, args.lon2

    args.bounds = (lat1, lon1, lat2, lon2)
    return args


def main(argv=None):
    args = parse_args(argv)
    lat1, lon1, lat2, lon2 = args.bounds
    if args.city:
        print(f"City '{args.city}': bbox={lat1},{lon1},{lat2},{lon2}")

    zoom_min, zoom_max = args.zoom
    if zoom_min > zoom_max:
        zoom_min, zoom_max = zoom_max, zoom_min

    server_config = TILE_SERVERS.get(args.server)
    if args.server == "generic":
        server_config = {
            "kind": "xyz",
            "url": args.url_template,
            "attribution": args.attribution,
            "max_zoom": 18,
            "user_agent": "KrabOS-MapDownloader/1.0",
        }
    output_dir = args.output or f"maps-{args.name}"
    tiles_dir = os.path.join(output_dir, "tiles")

    print(f"Region: {args.name}")
    print(f"Bounds: {lat1:.4f},{lon1:.4f} -> {lat2:.4f},{lon2:.4f}")
    print(f"Zooms:  {zoom_min}-{zoom_max}")
    print(f"Server: {server_description_for_log(args.server, server_config)}")
    print(f"Output: {output_dir}/")
    print()

    # Count tiles
    total_tiles = 0
    for zoom in range(zoom_min, zoom_max + 1):
        count = 0
        for _ in tiles_for_bbox(lat1, lon1, lat2, lon2, zoom):
            count += 1
        total_tiles += count
        if count > 0:
            print(f"  zoom {zoom}: {count} tiles")
        else:
            print(f"  zoom {zoom}: 0 tiles (skipped)")

    print(f"\nTotal: {total_tiles} tiles")

    if total_tiles == 0:
        print("No tiles to download. Check your bounding box and zoom range.")
        sys.exit(0)

    # Estimate size and time
    est_mb = total_tiles * 0.025  # ~25KB per PNG tile
    print(f"Estimated size: ~{est_mb:.0f} MB")
    print(f"Estimated time: ~{total_tiles / args.workers / 2:.0f} seconds ({args.workers} workers)")

    if args.dry_run:
        print("\nDry run - no tiles downloaded. Remove --dry-run to download.")
        return

    print(f"\nDownloading with {args.workers} workers...")
    print("Press Ctrl+C to cancel (partial downloads are safe to --resume)\n")
    os.makedirs(tiles_dir, exist_ok=True)

    # Build tile list
    tiles = []
    for zoom in range(zoom_min, zoom_max + 1):
        for x, y, _ in tiles_for_bbox(lat1, lon1, lat2, lon2, zoom):
            tiles.append((x, y, zoom, server_config, tiles_dir, args.resume))

    download_stats["total"] = len(tiles)
    download_stats["done"] = 0
    download_stats["failed"] = 0

    # Download in parallel
    start_time = time.time()
    init_session(server_config)

    with ThreadPoolExecutor(max_workers=args.workers) as executor:
        futures = {executor.submit(download_tile, t): t for t in tiles}

        last_report = 0
        for future in as_completed(futures):
            future.result()
            now = time.time()
            if now - last_report > 2.0:  # report every 2 seconds
                elapsed = now - start_time
                rate = download_stats["done"] / elapsed if elapsed > 0 else 0
                pct = download_stats["done"] / download_stats["total"] * 100
                print(f"\r  {download_stats['done']}/{download_stats['total']} tiles "
                      f"({pct:.0f}%) - {rate:.0f} tiles/s "
                      f"- {download_stats['failed']} failed  ", end="", flush=True)
                last_report = now

    elapsed = time.time() - start_time
    print(f"\r  {download_stats['done']}/{download_stats['total']} tiles "
          f"(100%) - {download_stats['done']/elapsed:.0f} tiles/s "
          f"- {download_stats['failed']} failed  ")

    # Write metadata
    write_metadata(tiles_dir, args.name, server_config, lat1, lon1, lat2, lon2,
                   zoom_min, zoom_max)
    write_tile_index(tiles_dir)

    print(f"\nDone! {download_stats['done']} tiles downloaded, {download_stats['failed']} failed")
    print(f"Output: {output_dir}/")
    print("\nTo use on T-Deck:")
    print("  Copy the 'tiles/' folder to the root of the SD card")
    print("  The firmware reads from: /sdcard/tiles/{z}/{x}/{y}.png")


if __name__ == "__main__":
    main()
