#!/usr/bin/env python3
"""
ShitBird Firmware - Firmware Merge Script
Merges bootloader, partition table, and application into single flashable binary
"""

import os
import sys
import struct

# ESP32-S3 flash offsets
BOOTLOADER_OFFSET = 0x0
PARTITION_TABLE_OFFSET = 0x8000
APP_OFFSET = 0x10000

def merge_bin(output_path, bootloader_path, partition_path, app_path, flash_size=16*1024*1024):
    """
    Merge ESP32 binaries into a single flashable file

    Args:
        output_path: Path for merged output binary
        bootloader_path: Path to bootloader.bin
        partition_path: Path to partitions.bin
        app_path: Path to firmware.bin (application)
        flash_size: Total flash size in bytes (default 16MB)
    """

    # Create output buffer filled with 0xFF (erased flash state)
    merged = bytearray([0xFF] * flash_size)

    # Read and place bootloader
    if os.path.exists(bootloader_path):
        with open(bootloader_path, 'rb') as f:
            bootloader = f.read()
        merged[BOOTLOADER_OFFSET:BOOTLOADER_OFFSET + len(bootloader)] = bootloader
        print(f"[Merge] Bootloader: {len(bootloader)} bytes @ 0x{BOOTLOADER_OFFSET:08X}")
    else:
        print(f"[Merge] Warning: Bootloader not found: {bootloader_path}")

    # Read and place partition table
    if os.path.exists(partition_path):
        with open(partition_path, 'rb') as f:
            partitions = f.read()
        merged[PARTITION_TABLE_OFFSET:PARTITION_TABLE_OFFSET + len(partitions)] = partitions
        print(f"[Merge] Partitions: {len(partitions)} bytes @ 0x{PARTITION_TABLE_OFFSET:08X}")
    else:
        print(f"[Merge] Warning: Partition table not found: {partition_path}")

    # Read and place application
    if os.path.exists(app_path):
        with open(app_path, 'rb') as f:
            app = f.read()
        merged[APP_OFFSET:APP_OFFSET + len(app)] = app
        print(f"[Merge] Application: {len(app)} bytes @ 0x{APP_OFFSET:08X}")
    else:
        print(f"[Merge] Error: Application not found: {app_path}")
        return False

    # Calculate actual size needed (trim trailing 0xFF)
    actual_size = len(merged)
    while actual_size > 0 and merged[actual_size - 1] == 0xFF:
        actual_size -= 1

    # Round up to nearest 4KB boundary
    actual_size = ((actual_size + 4095) // 4096) * 4096

    # Write merged binary
    with open(output_path, 'wb') as f:
        f.write(merged[:actual_size])

    print(f"[Merge] Output: {output_path} ({actual_size} bytes)")
    return True


def main(env, target):
    """PlatformIO post-build script entry point"""

    # Get build directory
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    # Paths
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    partition_path = os.path.join(build_dir, "partitions.bin")
    app_path = os.path.join(build_dir, "firmware.bin")
    output_path = os.path.join(project_dir, "build", "shitbird_merged.bin")

    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Alternative paths for ESP-IDF builds
    if not os.path.exists(bootloader_path):
        bootloader_path = os.path.join(
            env.PioPlatform().get_package_dir("framework-arduinoespressif32"),
            "tools", "sdk", "esp32s3", "bin", "bootloader_dio_80m.bin"
        )

    if merge_bin(output_path, bootloader_path, partition_path, app_path):
        print(f"[ShitBird] Merged firmware created: {output_path}")
    else:
        print(f"[ShitBird] Failed to create merged firmware")


# For standalone execution
if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: merge_firmware.py <bootloader.bin> <partitions.bin> <firmware.bin> [output.bin]")
        print("")
        print("Merges ESP32-S3 binaries into single flashable file")
        sys.exit(1)

    bootloader = sys.argv[1]
    partitions = sys.argv[2]
    firmware = sys.argv[3]
    output = sys.argv[4] if len(sys.argv) > 4 else "merged_firmware.bin"

    if merge_bin(output, bootloader, partitions, firmware):
        print(f"Success! Merged firmware: {output}")
    else:
        print("Failed to merge firmware")
        sys.exit(1)
