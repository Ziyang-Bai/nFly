#!/usr/bin/env python3
"""Relocate a Firebird v3 snapshot and its embedded images into a fresh directory.

Header layout follows firebird/core/emu.h (emu_snapshot.header); emu_start
restores these embedded paths. Run on the same host as firebird-headless.
"""

import argparse
import gzip
from pathlib import Path
import shutil
import struct

HEADER = struct.Struct("<II512s512s")


def prepare(snapshot, destination):
    with gzip.open(snapshot, "rb") as source:
        raw_header = source.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise ValueError("Truncated Firebird snapshot header")
        signature, version, boot_path, flash_path = HEADER.unpack(raw_header)
        if (signature, version) != (0xCAFEBEE0, 3):
            raise ValueError("Only the verified Firebird snapshot v3 format is supported")
        images = [Path(p.split(b"\0", 1)[0].decode("utf-8")) for p in (boot_path, flash_path)]
        for image in images:
            if not image.is_file():
                raise FileNotFoundError(f"Embedded image does not exist on this host: {image}")
        destination = destination.resolve()
        new_images = [destination / "boot1.bin", destination / "flash.bin"]
        new_paths = [p.as_posix().encode("utf-8") for p in new_images]
        if any(len(p) >= 512 for p in new_paths):
            raise ValueError("Relocated paths exceed the snapshot header capacity")
        destination.mkdir(parents=True, exist_ok=False)
        for original, relocated in zip(images, new_images):
            shutil.copyfile(original, relocated)
        output = destination / "baseline.snapshot"
        with gzip.open(output, "wb", compresslevel=1) as target:
            target.write(HEADER.pack(signature, version, *new_paths))
            shutil.copyfileobj(source, target, length=1024 * 1024)
    print(f"Snapshot: {output}")
    for path in new_images:
        print(f"Private image: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    prepare(args.snapshot, args.destination)


if __name__ == "__main__":
    main()
