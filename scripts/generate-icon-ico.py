"""Pack the supplied PNG exports into an ICO without altering their pixels."""

from pathlib import Path
import struct


def main():
    root = Path(__file__).resolve().parents[1] / "assets" / "icon"
    sizes = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
    payloads = []
    for size in sizes:
        data = (root / "png" / f"sdrnext-{size}.png").read_bytes()
        if (
            data[:8] != b"\x89PNG\r\n\x1a\n"
            or data[12:16] != b"IHDR"
            or struct.unpack_from(">II", data, 16) != (size, size)
            or data[24:26] != bytes((8, 6))
        ):
            raise ValueError(f"Expected {size}px RGBA8 PNG")
        payloads.append(data)

    entries = []
    offset = 6 + 16 * len(sizes)
    for size, data in zip(sizes, payloads):
        entries.append(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    output = root / "sdrnext.ico"
    output.write_bytes(struct.pack("<HHH", 0, 1, len(sizes)) + b"".join(entries + payloads))
    print(f"Wrote {output.name}: {len(sizes)} sizes, unchanged PNG payloads")


if __name__ == "__main__":
    main()
