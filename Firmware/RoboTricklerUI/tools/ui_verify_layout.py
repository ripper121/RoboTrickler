"""Render and exercise the real LVGL screen/factories on Windows, without hardware.

Uses installed LVGL and Visual Studio CMake/MSVC. Build and audit output stay in
the system temporary directory; --manual writes PNGs to docs/screenshots.
Dialog factory bodies are extracted from the sketch; hardware action callbacks
are replaced with counters and never operate a device.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
AUDIT = Path(tempfile.gettempdir()) / "rtui_touch_audit"
LVGL = Path.home() / "Documents/Arduino/libraries/lvgl"
CMAKE = Path("C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")


def function_body(source, name):
    match = re.search(r"^(?:static )?[^\n;]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise ValueError(f"Missing factory: {name}")
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manual", action="store_true", help="Render German manual screenshots with illustrative data")
    args = parser.parse_args()
    AUDIT.mkdir(parents=True, exist_ok=True)
    dialogs = (ROOT / "ui_dialogs.ino").read_text(encoding="utf-8")
    helpers = (ROOT / "display_helpers.ino").read_text(encoding="utf-8")
    names = ["createDialogButtonBase", "createDialogButton", "createDialogSymbolButton",
             "createDialogPanel", "createDialogTitle",
             "createDialogValueLabel", "ensureNoButton", "createMessageDialog",
             "presentDialog", "layoutProfileTuneDialog", "createProfileTuneDialog"]
    bodies = "\n\n".join(function_body(helpers, n) for n in
                          ["showDialog", "closeDialog", "setProfileTabEnabled"])
    bodies += "\n\n" + "\n\n".join(function_body(dialogs, n) for n in names)
    declared = set(re.findall(r"extern lv_obj_t \*(\w+)", (ROOT / "ui.h").read_text()))
    pointers = set(re.findall(r"lv_obj_t \*(\w+) = NULL;", dialogs)) - declared
    header = "\n".join("lv_obj_t *" + n + " = nullptr;" for n in sorted(pointers))
    callbacks = set(re.findall(r"\b(\w+_event_cb)\b", bodies))
    callbacks |= set(re.findall(r"void (\w+_event_cb)\(", (ROOT / "ui_events.h").read_text()))
    callbacks.discard("lv_obj_add_event_cb")
    header += "\n" + "\n".join('extern "C" void ' + n +
                               '(lv_event_t *) { ++actionCount; }' for n in sorted(callbacks))
    for lang in ("en", "de"):
        strings = json.loads((ROOT / f"SD-Files/lang/{lang}.json").read_text(encoding="utf-8"))["ui"]
        entries = ",\n".join("{" + json.dumps(k) + "," + json.dumps(v, ensure_ascii=True) + "}"
                              for k, v in strings.items())
        header += f"\nstatic const TextEntry {lang}Text[] = {{\n{entries}\n}};\n"
    header += "\nstatic const char *langText(const char *key) {\n"
    header += " if (german) { for (const auto &e : deText) if (!strcmp(e.key,key)) return e.value; }\n"
    header += " for (const auto &e : enText) if (!strcmp(e.key,key)) return e.value; return key; }\n"
    (AUDIT / "ui_factories.inc").write_text(header + "\n" + bodies, encoding="utf-8")
    # MSVC rejects LVGL's GCC zero-length cache-array extension. Use one
    # cache slot only in the native harness; firmware configuration is untouched.
    native_config = '#include "' + (ROOT / 'lv_conf.h').as_posix() + '"\n#undef LV_DRAW_SW_CIRCLE_CACHE_SIZE\n#define LV_DRAW_SW_CIRCLE_CACHE_SIZE 1\n'
    (AUDIT / 'lv_native_conf.h').write_text(native_config)
    cmake = f'''cmake_minimum_required(VERSION 3.20)
project(ui_touch_audit LANGUAGES C CXX)
set(LV_BUILD_CONF_PATH "{AUDIT.as_posix()}/lv_native_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("{LVGL.as_posix()}" lvgl)
target_compile_features(lvgl PUBLIC c_std_11)
add_executable(ui_touch_audit "{ROOT.as_posix()}/tools/ui_layout_harness.cpp"
 "{ROOT.as_posix()}/ui.c" "{ROOT.as_posix()}/ui_Screen1.c" "{ROOT.as_posix()}/ui_touch.c")
target_include_directories(ui_touch_audit PRIVATE "{ROOT.as_posix()}" "{AUDIT.as_posix()}")
target_compile_features(ui_touch_audit PRIVATE cxx_std_17)
target_link_libraries(ui_touch_audit PRIVATE lvgl)
'''
    (AUDIT / "CMakeLists.txt").write_text(cmake, encoding="utf-8")
    subprocess.run([str(CMAKE), "-S", str(AUDIT), "-B", str(AUDIT / "native"),
                    "-G", "Visual Studio 17 2022", "-A", "Win32"], check=True)
    subprocess.run([str(CMAKE), "--build", str(AUDIT / "native"), "--config", "Release",
                    "--parallel", "4"], check=True)
    output = ROOT / "docs/screenshots" if args.manual else AUDIT
    output.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(AUDIT / "native/Release/ui_touch_audit.exe")] +
                   (["--manual"] if args.manual else []), cwd=output, check=True, timeout=45)
    from PIL import Image
    for path in output.glob("*.ppm"):
        with Image.open(path) as picture:
            picture.save(path.with_suffix(".png"))
        if args.manual:
            path.unlink()
    print(f"Rendered previews: {output}")


if __name__ == "__main__":
    main()
