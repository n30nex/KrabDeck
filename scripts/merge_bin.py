#!/usr/bin/env python3
"""Post-build: merge bootloader + partitions + firmware into a single flashable image.
Patterned after MeshCore merge-bin.py — reads flash config from board JSON."""

Import("env")

def merge_bin_action(target, source, env):
    board_config = env.BoardConfig()
    build_dir = env.subst("$BUILD_DIR")
    merged_bin = env.subst("$BUILD_DIR/${PROGNAME}-merged.bin")
    firmware_bin = env.subst("$BUILD_DIR/${PROGNAME}.bin")

    import os as _os
    bootloader = f"{build_dir}/bootloader.bin"
    partitions = f"{build_dir}/partitions.bin"

    for f in [bootloader, partitions, firmware_bin]:
        if not _os.path.isfile(f):
            print(f"SlopOS: skipping merge — missing: {f}")
            return

    flash_images = [
        *env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])),
        "$ESP32_APP_OFFSET",
        firmware_bin,
    ]

    merge_cmd = " ".join([
        '"$PYTHONEXE"',
        '"$OBJCOPY"',
        "--chip",
        board_config.get("build.mcu", "esp32s3"),
        "merge_bin",
        "-o", merged_bin,
        "--flash_mode",
        board_config.get("build.flash_mode", "dio"),
        "--flash_freq",
        "${__get_board_f_flash(__env__)}",
        "--flash_size",
        board_config.get("upload.flash_size", "16MB"),
        *flash_images,
    ])

    print(f"SlopOS: merging firmware ({board_config.get('build.mcu', 'esp32s3')})…")
    env.Execute(merge_cmd)

    if _os.path.isfile(merged_bin):
        print(f"SlopOS: merged → firmware-merged.bin ({_os.path.getsize(merged_bin):,} bytes)")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
