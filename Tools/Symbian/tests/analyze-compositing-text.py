#!/usr/bin/env python3
"""Compare unchanged one-pixel stripe rows in a compositing-text screenshot (Pillow required)."""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("screenshot", type=Path)
    parser.add_argument("--left", type=int, default=283)
    parser.add_argument("--plain-y", type=int, default=406)
    parser.add_argument("--promoted-y", type=int, default=420)
    parser.add_argument("--width", type=int, default=280)
    parser.add_argument("--tolerance", type=int, default=2)
    parser.add_argument("--require-match", action="store_true")
    args = parser.parse_args()
    image = Image.open(args.screenshot).convert("RGB")
    if not (0 <= args.left < args.left + args.width <= image.width
            and 0 <= args.plain_y < image.height and 0 <= args.promoted_y < image.height
            and 0 <= args.tolerance <= 255):
        parser.error("Row coordinates, width or tolerance are outside the image/range")
    rows = {}
    for name, y in (("plain", args.plain_y), ("promoted", args.promoted_y)):
        pixels = [image.getpixel((args.left + x, y)) for x in range(args.width)]
        errors = [abs(channel - (255 if x % 2 else 0)) for x, pixel in enumerate(pixels) for channel in pixel]
        rows[name] = {"y": y, "maximum_error": max(errors), "mean_absolute_error": sum(errors) / len(errors),
                      "mismatched_channels": sum(error > args.tolerance for error in errors),
                      "one_pixel_alternation": all(error <= args.tolerance for error in errors)}
    result = {"screenshot_sha256": hashlib.sha256(args.screenshot.read_bytes()).hexdigest(),
              "image_size": list(image.size), "left": args.left, "width": args.width,
              "tolerance": args.tolerance, "rows": rows,
              "matched": all(row["one_pixel_alternation"] for row in rows.values()),
              "scope": "Checks two specified stripe rows only; text and the rest of the page require visual review."}
    print(json.dumps(result, indent=2))
    return int(args.require_match and not result["matched"])


if __name__ == "__main__":
    raise SystemExit(main())
