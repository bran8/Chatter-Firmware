#!/usr/bin/env python3

from pathlib import Path
import sys

BOMS = [
    (b"\xff\xfe\x00\x00", "utf-32-le"),
    (b"\x00\x00\xfe\xff", "utf-32-be"),
    (b"\xff\xfe", "utf-16-le"),
    (b"\xfe\xff", "utf-16-be"),
    (b"\xef\xbb\xbf", "utf-8-sig"),
]

CANDIDATE_ENCODINGS = [
    "utf-8",
    "utf-8-sig",
    "utf-16",
    "utf-32",
    "cp1252",
    "latin-1",
]

def decode_bytes(raw: bytes) -> tuple[str, str]:
    for bom, encoding in BOMS:
        if raw.startswith(bom):
            return raw.decode(encoding), encoding

    for encoding in CANDIDATE_ENCODINGS:
        try:
            return raw.decode(encoding), encoding
        except UnicodeDecodeError:
            pass

    return raw.decode("utf-8", errors="replace"), "utf-8 (with replacement)"

def scrub_text_file(input_path: str, output_path: str | None = None) -> Path:
    src = Path(input_path)
    if output_path is None:
        output = src.with_name(f"{src.stem}.utf8{src.suffix}")
    else:
        output = Path(output_path)

    raw = src.read_bytes()
    text, detected_encoding = decode_bytes(raw)

    text = text.replace("\r\n", "\n").replace("\r", "\n")

    output.write_text(text, encoding="utf-8", newline="\n")

    print(f"Input: {src}")
    print(f"Detected/used encoding: {detected_encoding}")
    print(f"Output: {output} (UTF-8)")

    return output

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python scrub_text.py <input-file> [output-file]")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    scrub_text_file(input_file, output_file)