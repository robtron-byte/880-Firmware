#!/usr/bin/env python3
"""Convert an Intel HEX firmware image into the System80 880 sysex update format.

Format (reverse-engineered from the official 880-1.1.4(-MK2).syx files):
  per 128-byte flash page:
    F0 00 79 08 00 7F 7E 00 <256 nibbles: hi,lo per byte> <checksum hi> <checksum lo> F7
    checksum = sum of the 128 page bytes & 0xFF
  end of transfer:
    F0 00 79 08 00 7F 7F 00 F7

Usage:
  hex2syx.py firmware.hex firmware.syx [--max-size 28672]
  hex2syx.py --selftest official.syx      # decode + re-encode must be byte identical
"""
import sys

PAGE = 128
HEADER = bytes([0xF0, 0x00, 0x79, 0x08, 0x00, 0x7F, 0x7E, 0x00])
END = bytes([0xF0, 0x00, 0x79, 0x08, 0x00, 0x7F, 0x7F, 0x00, 0xF7])


def read_hex(path):
    mem = {}
    base = 0
    with open(path) as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            if line[0] != ':':
                raise ValueError(f"line {line_no}: not an Intel HEX record")
            raw = bytes.fromhex(line[1:])
            if sum(raw) & 0xFF:
                raise ValueError(f"line {line_no}: HEX checksum error")
            count, addr, rtype = raw[0], (raw[1] << 8) | raw[2], raw[3]
            data = raw[4:4 + count]
            if rtype == 0x00:
                for i, b in enumerate(data):
                    mem[base + addr + i] = b
            elif rtype == 0x01:
                break
            elif rtype == 0x02:
                base = ((data[0] << 8) | data[1]) << 4
            elif rtype == 0x04:
                base = ((data[0] << 8) | data[1]) << 16
            # 0x03/0x05 (start address) are irrelevant for flashing
    if not mem:
        raise ValueError("HEX file contains no data")
    if min(mem) != 0:
        raise ValueError("image must start at address 0")
    size = max(mem) + 1
    image = bytearray([0xFF]) * size
    for a, b in mem.items():
        image[a] = b
    return bytes(image)


def encode(image):
    pages = (len(image) + PAGE - 1) // PAGE
    padded = image + bytes([0xFF]) * (pages * PAGE - len(image))
    out = bytearray()
    for p in range(pages):
        page = padded[p * PAGE:(p + 1) * PAGE]
        out += HEADER
        for b in page:
            out += bytes([b >> 4, b & 0x0F])
        chk = sum(page) & 0xFF
        out += bytes([chk >> 4, chk & 0x0F, 0xF7])
    out += END
    return bytes(out)


def decode(syx):
    msgs = syx.split(b'\xF0')
    if msgs[0]:
        raise ValueError("data before first sysex message")
    image = bytearray()
    for i, m in enumerate(msgs[1:-1]):
        m = b'\xF0' + m
        if not m.startswith(HEADER) or len(m) != len(HEADER) + 256 + 3 or m[-1] != 0xF7:
            raise ValueError(f"message {i}: unexpected layout")
        nib = m[len(HEADER):len(HEADER) + 256]
        page = bytes((nib[j] << 4) | nib[j + 1] for j in range(0, 256, 2))
        chk = (m[-3] << 4) | m[-2]
        if sum(page) & 0xFF != chk:
            raise ValueError(f"message {i}: checksum mismatch")
        image += page
    if b'\xF0' + msgs[-1] != END:
        raise ValueError("missing end-of-transfer message")
    return bytes(image)


def main(argv):
    if len(argv) == 3 and argv[1] == '--selftest':
        orig = open(argv[2], 'rb').read()
        image = decode(orig)
        again = encode(image)
        if again != orig:
            print("SELFTEST FAILED: re-encoded sysex differs from original")
            return 1
        print(f"selftest ok: {len(image)} bytes, {len(image) // PAGE} pages, byte-identical")
        return 0
    max_size = 28672
    args = list(argv[1:])
    if '--max-size' in args:
        i = args.index('--max-size')
        max_size = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        print(__doc__)
        return 2
    image = read_hex(args[0])
    if len(image) > max_size:
        print(f"ERROR: firmware is {len(image)} bytes, limit is {max_size} (bootloader area)")
        return 1
    syx = encode(image)
    assert decode(syx)[:len(image)] == image
    open(args[1], 'wb').write(syx)
    print(f"{args[1]}: {len(image)} bytes -> {(len(image) + PAGE - 1) // PAGE} pages, {len(syx)} bytes sysex")
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
