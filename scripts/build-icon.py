"""Rasterize the SVG at native Windows icon sizes. Requires PyMuPDF and Pillow."""
from pathlib import Path
import io
import struct
import fitz
from PIL import Image

root = Path(__file__).resolve().parent.parent
source = root / "assets/astrophoto.svg"
sizes = (16, 20, 24, 32, 40, 48, 64, 96, 128, 256)
document = fitz.open(source)
page = document[0]
images = []
for size in sizes:
    pixmap = page.get_pixmap(matrix=fitz.Matrix(size / page.rect.width, size / page.rect.height), alpha=True)
    png = pixmap.tobytes("png")
    with Image.open(io.BytesIO(png)) as image:
        assert image.size == (size, size)
    images.append(png)
offset = 6 + 16 * len(sizes)
directory = bytearray(struct.pack("<HHH", 0, 1, len(sizes)))
for size, png in zip(sizes, images):
    directory += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32, len(png), offset)
    offset += len(png)
destination = source.with_suffix(".ico")
destination.write_bytes(directory + b"".join(images))
with Image.open(destination) as icon:
    assert icon.info["sizes"] == {(size, size) for size in sizes}
print(f"Created {destination}: {', '.join(map(str, sizes))} px, RGBA")
