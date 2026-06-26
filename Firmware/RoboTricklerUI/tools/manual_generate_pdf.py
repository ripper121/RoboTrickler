#!/usr/bin/env python3
"""Render the German end-user manual (manual.md) into a printable PDF.

Used by the release pipeline (release_update_usb_flash.py -pdf) to drop a
Manual.pdf into the SD-Files-Gz tree, but can also be run standalone:

    python tools/manual_generate_pdf.py
    python tools/manual_generate_pdf.py --output some/where/Manual.pdf

Conversion uses pure-pip dependencies only (no system libraries):

    pip install markdown xhtml2pdf pillow

markdown + xhtml2pdf are required; pillow is optional and only used to downscale
the embedded screenshots and fit them to the page (without it the originals are
embedded at a fixed width).

Remote screenshots referenced by https URLs are downloaded and embedded;
images that cannot be fetched (e.g. no network) are dropped with a warning so
the build still produces a PDF.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import tempfile
import urllib.request
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SKETCH_DIR = SCRIPT_DIR.parent
DEFAULT_MANUAL = SKETCH_DIR / "manual.md"
# The PDF ships inside the generated SD-Files-Gz tree (alongside the gzipped web
# files) but is deliberately left uncompressed so it stays a readable Manual.pdf.
DEFAULT_OUTPUT = SKETCH_DIR / "SD-Files-Gz" / "Manual.pdf"

MARKDOWN_EXTENSIONS = ["tables", "fenced_code", "toc", "sane_lists"]

# Embedded screenshots are downscaled in pixels to keep the PDF small, then sized
# on the page to fit a box so nothing overflows. REF_DPI controls how large an
# image is shown relative to its (downscaled) pixel size before the box clamps it.
MAX_IMAGE_WIDTH = 1100
JPEG_QUALITY = 80
REF_DPI = 150.0
MAX_DISPLAY_WIDTH_CM = 11.0
MAX_DISPLAY_HEIGHT_CM = 20.0
CM_PER_INCH = 2.54

# xhtml2pdf renders a limited CSS subset; keep the stylesheet conservative.
PAGE_CSS = """
@page { size: a4 portrait; margin: 1.8cm; }
body { font-family: Helvetica, Arial, sans-serif; font-size: 10pt; line-height: 1.4; color: #222222; }
h1 { font-size: 20pt; border-bottom: 2px solid #444444; padding-bottom: 4px; margin-top: 18pt; }
h2 { font-size: 15pt; margin-top: 14pt; }
h3 { font-size: 12pt; margin-top: 10pt; }
h4 { font-size: 11pt; }
p { margin: 4pt 0; }
code { font-family: 'Courier New', monospace; font-size: 9pt; background-color: #f0f0f0; }
/* xhtml2pdf does not honor 'pre-wrap' (it collapses newlines); 'pre' keeps the
   line breaks. Overlong lines are soft-wrapped beforehand in _wrap_code_lines. */
pre { background-color: #f4f4f4; border: 1px solid #dddddd; padding: 6px; font-family: 'Courier New', monospace; font-size: 8pt; white-space: pre; }
pre code { background-color: transparent; font-size: 8pt; }
table { border-collapse: collapse; margin: 6pt 0; }
th, td { border: 1px solid #999999; padding: 4px; font-size: 9pt; }
th { background-color: #eeeeee; }
blockquote { border-left: 3px solid #cccccc; margin-left: 0; padding-left: 10px; color: #555555; }
.figure { text-align: center; margin: 10pt 0; }
a { color: #1a0dab; text-decoration: none; }
"""

HTML_TEMPLATE = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8" />
<style>{css}</style>
</head>
<body>
{body}
</body>
</html>
"""

_IMG_TAG_RE = re.compile(r"<img\b[^>]*>", re.IGNORECASE)
_IMG_SRC_RE = re.compile(r'src\s*=\s*"([^"]*)"', re.IGNORECASE)

# Fenced code blocks (```...```), captured so their long lines can be soft-wrapped.
_FENCE_RE = re.compile(r"(^```[^\n]*\n)(.*?)(^```[ \t]*$)", re.DOTALL | re.MULTILINE)
# Soft-wrap code lines longer than this so they fit the page under white-space:pre.
MAX_CODE_LINE = 100


def _wrap_text_line(line: str, width: int = MAX_CODE_LINE) -> str:
    """Break a single overlong code line into several, preferring spaces."""
    pieces = []
    while len(line) > width:
        break_at = line.rfind(" ", 0, width)
        if break_at <= 0:
            break_at = width
        pieces.append(line[:break_at].rstrip())
        line = line[break_at:].lstrip()
    pieces.append(line)
    return "\n".join(pieces)


def _wrap_code_lines(markdown_text: str) -> str:
    """Soft-wrap overlong lines inside fenced code blocks. JSON examples (short
    lines) are untouched; long shell commands wrap instead of overflowing."""

    def wrap_block(match: re.Match[str]) -> str:
        body = "\n".join(_wrap_text_line(line) for line in match.group(2).split("\n"))
        return match.group(1) + body + match.group(3)

    return _FENCE_RE.sub(wrap_block, markdown_text)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Render manual.md into a PDF.")
    parser.add_argument(
        "--manual",
        type=Path,
        default=DEFAULT_MANUAL,
        help=f"source markdown file (default: {DEFAULT_MANUAL})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"output PDF path (default: {DEFAULT_OUTPUT})",
    )
    return parser.parse_args()


def _import_dependencies():
    try:
        import markdown  # type: ignore
        from xhtml2pdf import pisa  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "PDF generation needs the 'markdown' and 'xhtml2pdf' packages. "
            "Install them with: pip install markdown xhtml2pdf pillow"
        ) from exc
    return markdown, pisa


def _load_image_bytes(src: str, base_dir: Path) -> bytes | None:
    """Fetch the raw bytes for an <img> source (http(s) download or local file)."""
    if src.startswith(("http://", "https://")):
        request = urllib.request.Request(
            src, headers={"User-Agent": "Mozilla/5.0 (RoboTrickler-doc-build)"}
        )
        with urllib.request.urlopen(request, timeout=20) as response:
            return response.read()
    local_path = (base_dir / src).resolve()
    return local_path.read_bytes()


def _process_image(data: bytes) -> tuple[bytes, str, int | None, int | None]:
    """Downscale + re-encode an image and report its final pixel size.
    Returns (bytes, suffix, width_px, height_px); width/height are None when
    Pillow is unavailable or the data cannot be decoded as an image."""
    try:
        import io

        from PIL import Image  # type: ignore
    except ImportError:
        return data, ".img", None, None

    try:
        with Image.open(io.BytesIO(data)) as image:
            image.load()
            if image.width > MAX_IMAGE_WIDTH:
                new_height = round(image.height * MAX_IMAGE_WIDTH / image.width)
                image = image.resize((MAX_IMAGE_WIDTH, new_height), Image.LANCZOS)
            # Flatten transparency/palette onto white so it stores as compact JPEG.
            if image.mode in ("RGBA", "LA", "P"):
                rgba = image.convert("RGBA")
                background = Image.new("RGB", rgba.size, (255, 255, 255))
                background.paste(rgba, mask=rgba.split()[-1])
                image = background
            else:
                image = image.convert("RGB")
            buffer = io.BytesIO()
            image.save(buffer, format="JPEG", quality=JPEG_QUALITY, optimize=True)
            return buffer.getvalue(), ".jpg", image.width, image.height
    except Exception as exc:  # decoding/encoding issues must not fail the build
        print(f"  warning: could not process an image ({exc})", flush=True)
        return data, ".img", None, None


def _display_size_cm(width_px: int, height_px: int) -> tuple[float, float]:
    """Compute the on-page size (cm) for an image, fitted into the max box while
    preserving aspect ratio and never upscaling beyond REF_DPI."""
    width_cm = width_px / REF_DPI * CM_PER_INCH
    height_cm = height_px / REF_DPI * CM_PER_INCH
    if width_cm > MAX_DISPLAY_WIDTH_CM:
        scale = MAX_DISPLAY_WIDTH_CM / width_cm
        width_cm *= scale
        height_cm *= scale
    if height_cm > MAX_DISPLAY_HEIGHT_CM:
        scale = MAX_DISPLAY_HEIGHT_CM / height_cm
        width_cm *= scale
        height_cm *= scale
    return width_cm, height_cm


def _embed_images(body_html: str, base_dir: Path, cache_dir: Path) -> str:
    """Replace every <img> with a centered figure pointing at a cached, downscaled
    copy sized to fit the page. Images that fail to load are dropped."""
    resolved: dict[str, str] = {}

    def replace(match: re.Match[str]) -> str:
        tag = match.group(0)
        src_match = _IMG_SRC_RE.search(tag)
        if not src_match:
            return ""
        src = src_match.group(1)

        if src not in resolved:
            try:
                data = _load_image_bytes(src, base_dir)
            except Exception as exc:  # network/missing file must not fail the build
                print(f"  warning: skipping image {src} ({exc})", flush=True)
                resolved[src] = ""
            else:
                data, suffix, width_px, height_px = _process_image(data)
                file_descriptor, path = tempfile.mkstemp(suffix=suffix, dir=cache_dir)
                with os.fdopen(file_descriptor, "wb") as image_file:
                    image_file.write(data)
                if width_px and height_px:
                    width_cm, height_cm = _display_size_cm(width_px, height_px)
                    style = f"width:{width_cm:.2f}cm;height:{height_cm:.2f}cm;"
                else:
                    style = f"width:{MAX_DISPLAY_WIDTH_CM:.2f}cm;"
                resolved[src] = (
                    f'<div class="figure"><img src="{path}" style="{style}" /></div>'
                )

        return resolved[src]

    return _IMG_TAG_RE.sub(replace, body_html)


def render_manual_pdf(manual_path: Path, output_path: Path) -> None:
    markdown, pisa = _import_dependencies()

    if not manual_path.is_file():
        raise FileNotFoundError(f"manual not found: {manual_path}")

    markdown_text = manual_path.read_text(encoding="utf-8")
    markdown_text = _wrap_code_lines(markdown_text)
    body_html = markdown.markdown(markdown_text, extensions=MARKDOWN_EXTENSIONS)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="manual_pdf_") as cache_dir:
        body_html = _embed_images(body_html, manual_path.resolve().parent, Path(cache_dir))
        document_html = HTML_TEMPLATE.format(css=PAGE_CSS, body=body_html)
        # All <img> sources are now absolute local cache paths; hand them back
        # to pisa unchanged.
        with output_path.open("wb") as pdf_file:
            result = pisa.CreatePDF(
                src=document_html,
                dest=pdf_file,
                encoding="utf-8",
                link_callback=lambda uri, rel: uri,
            )

    if result.err:
        raise RuntimeError(f"xhtml2pdf reported {result.err} error(s) building the PDF")

    print(f"Generated {output_path} ({output_path.stat().st_size} bytes)", flush=True)


def main() -> int:
    args = parse_args()
    try:
        render_manual_pdf(args.manual.resolve(), args.output.resolve())
        return 0
    except (FileNotFoundError, OSError, RuntimeError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
