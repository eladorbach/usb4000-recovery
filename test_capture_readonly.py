import io
import unittest

from usb4000_capture_readonly import parse_dump, TOTAL

class ParserTests(unittest.TestCase):
    @staticmethod
    def lines(addr, changes=None):
        data = bytearray(TOTAL)
        if changes:
            for i, v in changes.items():
                data[i] = v
        lines = [f"BEGIN 0x{addr:02X} {TOTAL}\n"]
        for off in range(0, TOTAL, 16):
            lines.append(f"DATA {off:04X} {data[off:off+16].hex()}\n")
        lines.append(f"END 0x{addr:02X} {TOTAL}\n")
        return lines

    def test_complete(self):
        lines = iter(self.lines(0x51, {0: 193, 32767: 249}))
        got = parse_dump(lambda: next(lines), 0x51)
        self.assertEqual(len(got), TOTAL)
        self.assertEqual(got[0], 193)
        self.assertEqual(got[-1], 249)

    def test_wrong_header_rejected(self):
        lines = iter(self.lines(0x53))
        with self.assertRaises(ValueError):
            parse_dump(lambda: next(lines), 0x51)

    def test_wrong_block_offset_rejected(self):
        lines = self.lines(0x51)
        lines[2] = lines[2].replace("0010", "0000", 1)
        it = iter(lines)
        with self.assertRaises(ValueError):
            parse_dump(lambda: next(it), 0x51)

    def test_missing_end_rejected(self):
        lines = self.lines(0x53)
        lines[-1] = "END WRONG\n"
        it = iter(lines)
        with self.assertRaises(ValueError):
            parse_dump(lambda: next(it), 0x53)

if __name__ == "__main__":
    unittest.main()
