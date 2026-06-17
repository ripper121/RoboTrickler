#!/usr/bin/env python3
"""Create the compressed web trees: the full SD-card tree (SD-Files-Gz) and a
minimal LittleFS tree (SD-Files-LittleFS) that keeps only .gz variants in
system/ and drops the offline-only landing page."""

from __future__ import annotations

import argparse
import gzip
import io
import os
import sys
import tempfile
from pathlib import Path


DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / "SD-Files"
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "SD-Files-Gz"
DEFAULT_OUTPUT_LITTLEFS = Path(__file__).resolve().parent.parent / "SD-Files-LittleFS"
EXCLUDED_TOP_LEVEL_DIRECTORIES = {"profiles","lang"}
EXCLUDED_FILES = {"avg.txt", "calibrate.txt"}
# Dropped from the minimal LittleFS tree only. LittleFS is the fallback
# filesystem served exclusively by the webserver (which can serve .gz
# transparently), so the offline file:// landing page is dead weight there.
# The Ace web-worker linters are optional (the editor works without inline
# lint warnings); they are the largest assets, so the slim LittleFS editor
# drops them while the SD-card tree keeps full linting.
LITTLEFS_EXCLUDED_FILES = {
    "start.html",
    "system/resources/src/worker-javascript.js.gz",
    "system/resources/src/worker-html.js.gz",
    "system/resources/src/worker-css.js.gz",
    "system/resources/src/worker-json.js.gz",
}
# Directory prefixes dropped from the minimal LittleFS tree. Language data is
# omitted to keep the image small: on-device text falls back to the built-in
# English table (languageFallback) when /lang is absent.
LITTLEFS_EXCLUDED_DIRECTORIES = {"lang", "system/lang"}
# Assets the offline web UI loads via <link>/<script> when index.html /
# settings.html / profileEditor.html are opened from a file:// URL or localhost
# (see the `location.hostname === "localhost" || ... === ""` checks in those
# pages and in lang.js). <link>/<script> work over file://, but the browser
# cannot transparently gunzip a file:// response (no `Content-Encoding: gzip`
# header), so for these files the tree keeps BOTH variants: the compressed `.gz`
# (the webserver prefers and serves it for faster page loads) AND an uncompressed
# copy a local browser can read directly.
LOCAL_UNCOMPRESSED_FILES = {
    "system/index.html",
    "system/settings.html",
    "system/profileEditor.html",
    "system/lang.js",
    "system/resources/style.css",
    "system/resources/favicon.ico",
}
# Folders with web language files. The webserver fetches the `.json` (kept as a
# `.gz`). A local browser cannot fetch() over file://, so for every `.json` here
# the build also emits an uncompressed JSONP `<name>.js` wrapper that lang.js
# loads via <script> injection when running locally.
LOCAL_JSONP_DIRECTORIES = {
    "system/lang",
}
MAX_GZIP_COMPRESSION = 9


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def is_local_uncompressed(relative_path: Path) -> bool:
    """Return True for assets the offline (file://) UI loads via <link>/<script>.

    For these an extra uncompressed copy is written next to the `.gz` so a local
    browser can read them; the webserver still prefers and serves the `.gz`.
    """
    return relative_path.as_posix() in LOCAL_UNCOMPRESSED_FILES


def local_jsonp_variant(relative_path: Path, data: bytes) -> tuple[Path, bytes] | None:
    """Wrap a web language `.json` as a JSONP `.js` for offline (file://) loading.

    Returns (js_relative_path, js_bytes) or None when it is not such a JSON file.
    """
    if (
        relative_path.suffix.lower() == ".json"
        and relative_path.parent.as_posix() in LOCAL_JSONP_DIRECTORIES
    ):
        wrapped = b"window.rtLoadLanguage(" + data.rstrip() + b");\n"
        return relative_path.with_suffix(".js"), wrapped
    return None


def should_compress(relative_path: Path) -> bool:
    return (
        len(relative_path.parts) > 1
        and relative_path.parts[0] not in EXCLUDED_TOP_LEVEL_DIRECTORIES
        and relative_path.suffix.lower() != ".gz"
    )


def gzip_bytes(data: bytes) -> bytes:
    output = io.BytesIO()
    with gzip.GzipFile(
        filename="",
        mode="wb",
        fileobj=output,
        compresslevel=MAX_GZIP_COMPRESSION,
        mtime=0,
    ) as gzip_file:
        gzip_file.write(data)
    return output.getvalue()


def write_if_changed(output: Path, data: bytes) -> bool:
    if output.exists() and output.read_bytes() == data:
        return False

    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            dir=output.parent,
            prefix=f".{output.name}.",
            suffix=".tmp",
            delete=False,
        ) as temporary_file:
            temporary_file.write(data)
            temporary_path = Path(temporary_file.name)

        os.replace(temporary_path, output)
        return True
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def create_output_tree(
    source: Path, output: Path, dry_run: bool, minimal: bool = False
) -> tuple[int, int, int]:
    processed = 0
    changed = 0
    removed = 0
    expected_files: set[Path] = set()

    def emit(output_relative_path: Path, output_data: bytes, is_compressed: bool) -> None:
        nonlocal processed, changed
        output_path = output / output_relative_path
        expected_files.add(output_path)
        processed += 1

        if dry_run:
            if output_path.exists() and output_path.read_bytes() == output_data:
                status = "unchanged"
            else:
                status = "would update"
                changed += 1
            status += " compressed" if is_compressed else " copied"
        else:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            if write_if_changed(output_path, output_data):
                status = "compressed" if is_compressed else "copied"
                changed += 1
            else:
                status = "unchanged"

        print(f"{status}: {output_relative_path}")

    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue

        relative_path = path.relative_to(source)
        if relative_path.as_posix() in EXCLUDED_FILES:
            continue
        # The minimal LittleFS tree omits the offline-only landing page and
        # the language directories.
        if minimal:
            if relative_path.as_posix() in LITTLEFS_EXCLUDED_FILES:
                continue
            if any(
                part_prefix in LITTLEFS_EXCLUDED_DIRECTORIES
                for part_prefix in (
                    "/".join(relative_path.parts[:i])
                    for i in range(1, len(relative_path.parts))
                )
            ):
                continue

        data = path.read_bytes()
        if should_compress(relative_path):
            gz_relative_path = relative_path.with_name(relative_path.name + ".gz")
            emit(gz_relative_path, gzip_bytes(data), True)
            # The minimal LittleFS tree keeps only the .gz variants (the
            # webserver serves them directly), so skip every offline-only copy.
            if not minimal:
                # The offline (file://) UI cannot gunzip a local response, so
                # also keep an uncompressed copy of the files it loads next to
                # the .gz.
                if is_local_uncompressed(relative_path):
                    emit(relative_path, data, False)
                # Web language JSON additionally gets an uncompressed JSONP .js
                # so the offline UI can load it via <script> injection (fetch is
                # blocked on file://).
                jsonp = local_jsonp_variant(relative_path, data)
                if jsonp is not None:
                    emit(jsonp[0], jsonp[1], False)
        else:
            emit(relative_path, data, False)

    if output.exists():
        for output_path in sorted(output.rglob("*"), reverse=True):
            if output_path.is_file() and output_path not in expected_files:
                relative_path = output_path.relative_to(output)
                if dry_run:
                    print(f"would remove: {relative_path}")
                else:
                    output_path.unlink()
                    print(f"removed: {relative_path}")
                removed += 1
            elif not dry_run and output_path.is_dir():
                try:
                    output_path.rmdir()
                except OSError:
                    pass

    return processed, changed, removed


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Create the SD-Files-Gz LittleFS source tree. Files parsed directly "
            "by firmware stay uncompressed; web assets are stored as .gz. Assets "
            "the offline file:// UI loads also get an uncompressed copy, and web "
            "language JSON additionally gets a JSONP .js wrapper for offline use."
        )
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"Source files folder. Default: {DEFAULT_SOURCE}",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Generated full SD-card tree (all variants). Default: {DEFAULT_OUTPUT}",
    )
    parser.add_argument(
        "--littlefs-output",
        type=Path,
        default=DEFAULT_OUTPUT_LITTLEFS,
        help=(
            "Generated minimal LittleFS tree (.gz only in system/, no offline "
            f"landing page). Default: {DEFAULT_OUTPUT_LITTLEFS}"
        ),
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show which output files would change without writing them.",
    )
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()
    littlefs_output = args.littlefs_output.resolve()

    if not source.is_dir():
        print(f"Source folder does not exist: {source}", file=sys.stderr)
        return 1

    for candidate in (output, littlefs_output):
        if is_relative_to(candidate, source) or is_relative_to(source, candidate):
            print(
                "Source and output folders must not contain each other.",
                file=sys.stderr,
            )
            return 1
    if output == littlefs_output:
        print("The two output folders must differ.", file=sys.stderr)
        return 1

    action = "Would update" if args.dry_run else "Updated"
    remove_action = "would remove" if args.dry_run else "removed"
    for target, minimal in ((output, False), (littlefs_output, True)):
        if not args.dry_run:
            target.mkdir(parents=True, exist_ok=True)
        processed, changed, removed = create_output_tree(
            source, target, args.dry_run, minimal=minimal
        )
        print(
            f"{action} {changed} of {processed} files and {remove_action} "
            f"{removed} stale files in {target}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
