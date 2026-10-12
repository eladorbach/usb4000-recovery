#!/usr/bin/env python3
"""READ-ONLY forensic comparison for backed-up Ocean Optics USB4000 EEPROMs.

Never touches a serial port, I2C bus, USB controller or device firmware.
No hardcoded serial number, hashes, local paths or target EEPROM offsets.

Usage:
  python3 usb4000_offline_diff.py original_0x51_read1.bin original_0x51_read2.bin official_usb4000.iic

Before any hardware repair, investigate instrument-specific calibration regions,
board revision, model firmware compatibility and exact EEPROM boot format.
"""
import argparse
import hashlib
from pathlib import Path


def sha(data):
    return hashlib.sha256(data).hexdigest()


def contiguous_runs(indexes):
    if not indexes:
        return []
    runs = []
    start = last = indexes[0]
    for i in indexes[1:]:
        if i != last + 1:
            runs.append((start, last))
            start = i
        last = i
    runs.append((start, last))
    return runs


def analyze(a, b, reference):
    if len(a) != 32768 or len(b) != 32768:
        raise ValueError('Expected two complete 32,768-byte 24LC256 backups')
    if a != b:
        raise ValueError('Backups do not match; STOP. No single trusted baseline.')
    if not reference or len(reference) > len(a):
        raise ValueError('Reference firmware must be nonempty and fit within EEPROM')
    differences = [i for i in range(len(reference)) if a[i] != reference[i]]
    runs = contiguous_runs(differences)
    return {
        'backup_sha256': sha(a),
        'reference_sha256': sha(reference),
        'eeprom_bytes': len(a),
        'reference_bytes': len(reference),
        'differing_bytes': len(differences),
        'runs': runs,
        'first_differing_offset': differences[0] if differences else None,
        'last_differing_offset': differences[-1] if differences else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('read1', type=Path)
    parser.add_argument('read2', type=Path)
    parser.add_argument('manufacturer_iic', type=Path)
    p = parser.parse_args()
    result = analyze(p.read1.read_bytes(), p.read2.read_bytes(), p.manufacturer_iic.read_bytes())
    for key, value in result.items():
        if key != 'runs':
            if key.endswith('offset') and value is not None:
                print('%s: 0x%04X' % (key, value))
            else:
                print('%s: %s' % (key, value))
    print('Contiguous difference ranges:')
    for a, b in result['runs']:
        print('  0x%04X–0x%04X (%d differing bytes)' % (a, b, b-a+1))
    print('READ-ONLY: no EEPROM changes made.')


if __name__ == '__main__':
    main()
