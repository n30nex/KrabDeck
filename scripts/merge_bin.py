#!/usr/bin/env python3
"""Post-build: merge bootloader + partitions + firmware into a single flashable binary."""
import os
Import("env")

print("SlopOS: merge script loaded")

def merge_firmware(target, source, env):
    build_dir = env.subst("$BUILD_DIR")
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware   = os.path.join(build_dir, "firmware.bin")
    merged     = os.path.join(build_dir, "firmware-merged.bin")

    print(f"SlopOS: merge action fired, build_dir={build_dir}")

    missing = [f for f in [bootloader, partitions, firmware] if not os.path.isfile(f)]
    if missing:
        print(f"SlopOS: skipping merge — missing: {missing}")
        return

    cmd = (
        f'esptool.py --chip esp32s3 merge_bin '
        f'-o {merged} --flash_mode dio --flash_size 16MB '
        f'0x0000 {bootloader} 0x8000 {partitions} 0x10000 {firmware}'
    )
    print(f"SlopOS: merging firmware...")
    ret = os.system(cmd)
    if ret == 0 and os.path.isfile(merged):
        size = os.path.getsize(merged)
        print(f"SlopOS: firmware-merged.bin ready ({size:,} bytes)")
    else:
        print(f"SlopOS: merge failed (exit {ret})")

# Try multiple hook points — one of these should fire
env.AddPostAction("buildprog", merge_firmware)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", merge_firmware)
