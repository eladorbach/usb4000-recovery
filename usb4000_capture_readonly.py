#!/usr/bin/env python3
"""Read-only USB4000 EEPROM backup collector for USB4000_I2C_READONLY.ino.

Makes two copies of each EEPROM and verifies identical 32 KiB reads.
The code never issues EEPROM write commands. It requires an explicit serial port.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

TOTAL = 32768
BLOCK = 16
LINE = re.compile(r"DATA ([0-9A-Fa-f]{4}) ([0-9A-Fa-f]{32})")


def parse_dump(readline, addr):
    header = f"BEGIN 0x{addr:02X} {TOTAL}"
    end = f"END 0x{addr:02X} {TOTAL}"
    first = readline().strip()
    if first != header:
        raise ValueError(f"Expected {header!r}, got {first!r}")
    out = bytearray()
    for off in range(0, TOTAL, BLOCK):
        line = readline().strip()
        match = LINE.fullmatch(line)
        if not match:
            raise ValueError(f"Bad/missing block at 0x{off:04X}: {line!r}")
        seen = int(match.group(1), 16)
        if seen != off:
            raise ValueError(f"Unexpected offset 0x{seen:04X} instead of 0x{off:04X}")
        out.extend(bytes.fromhex(match.group(2)))
    if readline().strip() != end:
        raise ValueError(f"Bad/missing END marker for EEPROM 0x{addr:02X}")
    if len(out) != TOTAL:
        raise ValueError("Short read")
    return bytes(out)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--port", required=True, help="ESP32 serial port or /dev/serial/by-id path")
    ap.add_argument("--output", required=True, type=Path, help="Private output directory")
    ap.add_argument("--baud", default=115200, type=int)
    args = ap.parse_args()
    import serial  # pip install pyserial
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit("Output directory is not empty; refusing to overwrite or mix backups")
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    with serial.Serial(args.port, args.baud, timeout=12, write_timeout=3) as port:
        port.reset_input_buffer()
        for addr in (0x51, 0x53):
            images = []
            for seq in (1, 2):
                port.reset_input_buffer()
                port.write((f"DUMP{addr:02X}\n").encode("ascii"))
                def line():
                    raw = port.readline()
                    if not raw:
                        raise TimeoutError(f"No serial reply from ESP32 (0x{addr:02X})")
                    return raw.decode("ascii", "replace")
                data = parse_dump(line, addr)
                images.append(data)
                path = args.output / f"usb4000_0x{addr:02x}_read{seq}.bin"
                with path.open("xb") as f:
                    f.write(data)
            if images[0] != images[1]:
                raise RuntimeError(f"0x{addr:02X}: two EEPROM reads differ, STOP")
            manifest[f"0x{addr:02X}"] = {
                "bytes": TOTAL, "copies_match": True, "sha256": digest(images[0])
            }
    with (args.output / "manifest.json").open("x", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
