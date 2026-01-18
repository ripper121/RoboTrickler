import argparse
from pathlib import Path

import fitz  # PyMuPDF

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None


CROP_BOX = (4090, 220, 6435, 4850)  # left, top, right, bottom in output pixels


def collect_pdfs(paths: list[Path]) -> list[Path]:
    pdfs: list[Path] = []
    missing: list[Path] = []
    for path in paths:
        if not path.exists() and not path.is_absolute():
            path = (Path.cwd() / path)
        if not path.exists():
            if path.suffix == "" and path.name.lower() == "pdfs":
                path.mkdir(parents=True, exist_ok=True)
            else:
                missing.append(path)
                continue
        if path.is_dir():
            pdfs.extend(sorted(path.glob("*.pdf")))
        else:
            pdfs.append(path)
    if missing:
        missing_list = ", ".join(str(path) for path in missing)
        raise SystemExit(f"Path(s) not found: {missing_list}")
    return pdfs


def crop_pixmap_to_png(pix: fitz.Pixmap, out_path: Path) -> None:
    if Image is None:
        raise SystemExit("Pillow is required for cropping. Install with: pip install Pillow")

    left, top, right, bottom = CROP_BOX
    left = max(0, left)
    top = max(0, top)
    right = min(pix.width, right)
    bottom = min(pix.height, bottom)
    if right <= left or bottom <= top:
        raise SystemExit("Crop box is outside the rendered image.")

    mode = "RGB" if pix.n >= 3 else "L"
    image = Image.frombytes(mode, (pix.width, pix.height), pix.samples)
    cropped = image.crop((left, top, right, bottom))
    cropped.save(out_path.as_posix(), format="PNG")


def print_png(path: Path) -> None:
    if Image is None:
        raise SystemExit("Pillow is required for printing. Install with: pip install Pillow")

    try:
        import win32con
        import win32print
        import win32ui
        import win32gui
        from PIL import ImageWin
    except ImportError as exc:  # pragma: no cover
        raise SystemExit(
            "Printing requires pywin32. Install with: pip install pywin32"
        ) from exc

    image = Image.open(path)
    if image.mode not in ("RGB", "L"):
        image = image.convert("RGB")

    printer_name = win32print.GetDefaultPrinter()
    hprinter = win32print.OpenPrinter(printer_name)
    try:
        devmode = win32print.GetPrinter(hprinter, 2)["pDevMode"]
        devmode.Orientation = win32con.DMORIENT_PORTRAIT
        devmode.Fields |= win32con.DM_ORIENTATION
        devmode_candidate = win32print.DocumentProperties(
            None,
            hprinter,
            printer_name,
            devmode,
            devmode,
            win32con.DM_IN_BUFFER | win32con.DM_OUT_BUFFER,
        )
        if hasattr(devmode_candidate, "Orientation"):
            devmode = devmode_candidate
    finally:
        win32print.ClosePrinter(hprinter)

    hdc = win32ui.CreateDC()
    hdc.CreatePrinterDC(printer_name)
    win32gui.ResetDC(hdc.GetSafeHdc(), devmode)

    printable_width = hdc.GetDeviceCaps(win32con.HORZRES)
    printable_height = hdc.GetDeviceCaps(win32con.VERTRES)
    offset_x = hdc.GetDeviceCaps(win32con.PHYSICALOFFSETX)
    offset_y = hdc.GetDeviceCaps(win32con.PHYSICALOFFSETY)

    img_width, img_height = image.size
    if img_width > img_height and printable_height >= printable_width:
        image = image.rotate(90, expand=True)
        img_width, img_height = image.size

    scale = min(printable_width / img_width, printable_height / img_height)
    scaled_width = int(img_width * scale)
    scaled_height = int(img_height * scale)

    left = offset_x + (printable_width - scaled_width) // 2
    top = offset_y + (printable_height - scaled_height) // 2
    right = left + scaled_width
    bottom = top + scaled_height

    hdc.StartDoc(path.name)
    hdc.StartPage()
    ImageWin.Dib(image).draw(hdc.GetHandleOutput(), (left, top, right, bottom))
    hdc.EndPage()
    hdc.EndDoc()
    hdc.DeleteDC()


def pdf_to_png(pdf_path: Path, out_dir: Path, dpi: int) -> list[Path]:
    out_dir.mkdir(parents=True, exist_ok=True)
    output_paths: list[Path] = []

    with fitz.open(pdf_path) as doc:
        for page_index in range(len(doc)):
            page = doc.load_page(page_index)
            zoom = dpi / 72
            matrix = fitz.Matrix(zoom, zoom).prerotate(90)
            pix = page.get_pixmap(alpha=False, matrix=matrix)
            out_path = out_dir / f"{pdf_path.stem}_page_{page_index + 1}_crop.png"
            crop_pixmap_to_png(pix, out_path)
            print_png(out_path)
            output_paths.append(out_path)

    return output_paths


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert PDF files to PNG images."
    )
    parser.add_argument(
        "pdfs",
        nargs="*",
        type=Path,
        help="One or more PDF paths or folders (supports drag-and-drop).",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("output_png"),
        help="Directory to write PNG files.",
    )
    parser.add_argument("--dpi", type=int, default=600, help="Output DPI.")
    args = parser.parse_args()

    if args.pdfs:
        pdf_paths = collect_pdfs(args.pdfs)
    else:
        candidate_dirs = [Path("pdfs"), Path.cwd()]
        candidates: list[Path] = []
        for candidate_dir in candidate_dirs:
            if candidate_dir.exists() and candidate_dir.is_dir():
                candidates.extend(sorted(candidate_dir.glob("*.pdf")))
            if candidates:
                break
        pdf_paths = candidates[:1]
    if not pdf_paths:
        raise SystemExit("No PDFs found.")

    for pdf_path in pdf_paths:
        output_paths = pdf_to_png(pdf_path, args.out_dir, args.dpi)
        for path in output_paths:
            print(path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
