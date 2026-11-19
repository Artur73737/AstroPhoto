"""Rasterize the SVG at native Windows icon and Android launcher sizes.

Requires resvg-py and Pillow. These packages are only needed to regenerate the
assets, not to build or run the applications.
"""
from pathlib import Path
import io
import struct
import resvg_py
from PIL import Image

root = Path(__file__).resolve().parent.parent
source = root / "assets/astrophoto.svg"
windows_sizes = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
android_sizes = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
adaptive_sizes = {"mdpi": 108, "hdpi": 162, "xhdpi": 216, "xxhdpi": 324, "xxxhdpi": 432}


def render(size: int) -> bytes:
    png = resvg_py.svg_to_bytes(svg_path=str(source), width=size, height=size)
    with Image.open(io.BytesIO(png)) as image:
        assert image.size == (size, size)
    return png


images = [render(size) for size in windows_sizes]
offset = 6 + 16 * len(windows_sizes)
directory = bytearray(struct.pack("<HHH", 0, 1, len(windows_sizes)))
for size, png in zip(windows_sizes, images):
    directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(png), offset)
    offset += len(png)
destination = source.with_suffix(".ico")
destination.write_bytes(directory + b"".join(images))
with Image.open(destination) as icon:
    assert icon.info["sizes"] == {(size, size) for size in windows_sizes}

for density, size in android_sizes.items():
    directory = root / f"android/app/src/main/res/mipmap-{density}"
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "ic_astrophoto.png").write_bytes(render(size))

for density, size in adaptive_sizes.items():
    directory = root / f"android/app/src/main/res/mipmap-{density}"
    (directory / "ic_astrophoto_foreground.png").write_bytes(render(size))

print(f"Created {destination}: {', '.join(map(str, windows_sizes))} px, RGBA")
print("Created Android launcher icons for " + ", ".join(android_sizes))
