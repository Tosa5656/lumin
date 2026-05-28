#!/usr/bin/env python3
"""Convert PSF2 font to C header"""
import sys

def psf2_to_c(psf_path, out_path):
    with open(psf_path, 'rb') as f:
        magic = f.read(4)
        if magic != b'\x72\xb5\x4a\x86':
            print(f"Not a PSF2 file: {psf_path}", file=sys.stderr)
            sys.exit(1)

        version = int.from_bytes(f.read(4), 'little')
        hdr_size = int.from_bytes(f.read(4), 'little')
        flags = int.from_bytes(f.read(4), 'little')
        glyph_count = int.from_bytes(f.read(4), 'little')
        glyph_bytes = int.from_bytes(f.read(4), 'little')
        height = int.from_bytes(f.read(4), 'little')
        width = int.from_bytes(f.read(4), 'little')

        print(f"  PSF2: {glyph_count} glyphs, {glyph_bytes}B each, {width}x{height}")

        f.seek(hdr_size)
        font_data = f.read(glyph_count * glyph_bytes)

    with open(out_path, 'w') as f:
        f.write("#ifndef FB_FONT_H\n#define FB_FONT_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("static const uint8_t fb_font[{}][{}] = {{\n".format(glyph_count, glyph_bytes))
        for g in range(glyph_count):
            offset = g * glyph_bytes
            row_bytes = ', '.join(f'0x{b:02X}' for b in font_data[offset:offset+glyph_bytes])
            f.write(f"    {{ {row_bytes} }}, // {g:02X}\n")
        f.write("};\n\n")
        f.write("#endif\n")

    print(f"  Wrote {out_path}")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.psf> <output.h>", file=sys.stderr)
        sys.exit(1)
    psf2_to_c(sys.argv[1], sys.argv[2])
