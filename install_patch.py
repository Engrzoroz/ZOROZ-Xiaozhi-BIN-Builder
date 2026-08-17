#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

BOARD = "engrzoroz-s3cam-ili9486"
SYMBOL = "BOARD_TYPE_ENGRZOROZ_S3CAM_ILI9486"

def die(msg):
    print("ERROR:", msg)
    sys.exit(1)

def main():
    here = Path(__file__).resolve().parent
    if len(sys.argv) >= 2:
        repo = Path(sys.argv[1]).resolve()
    else:
        repo = (here / "xiaozhi-esp32").resolve()

    if not (repo / "main" / "CMakeLists.txt").exists():
        die(f"XiaoZhi repo not found at: {repo}")

    src = here / "board" / BOARD
    dst = repo / "main" / "boards" / BOARD
    dst.mkdir(parents=True, exist_ok=True)
    for p in src.iterdir():
        if p.is_file():
            shutil.copy2(p, dst / p.name)
    print("Copied board files ->", dst)

    kconfig = repo / "main" / "Kconfig.projbuild"
    text = kconfig.read_text(encoding="utf-8")
    if SYMBOL not in text:
        anchor = '    config BOARD_TYPE_BREAD_COMPACT_WIFI\n'
        if anchor not in text:
            die("Could not locate BOARD_TYPE_BREAD_COMPACT_WIFI in Kconfig.projbuild")
        entry = (
            '    config BOARD_TYPE_ENGRZOROZ_S3CAM_ILI9486\n'
            '        bool "Engr Zoroz ESP32-S3 CAM + 3.5in ILI9486"\n'
            '        depends on IDF_TARGET_ESP32S3\n'
        )
        text = text.replace(anchor, entry + anchor, 1)
        kconfig.write_text(text, encoding="utf-8")
        print("Patched:", kconfig)
    else:
        print("Kconfig already patched")

    cmake = repo / "main" / "CMakeLists.txt"
    text = cmake.read_text(encoding="utf-8")
    if f"CONFIG_{SYMBOL}" not in text:
        anchor = "if(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)\n"
        if anchor not in text:
            die("Could not locate first board branch in main/CMakeLists.txt")
        entry = (
            "if(CONFIG_BOARD_TYPE_ENGRZOROZ_S3CAM_ILI9486)\n"
            '    set(BOARD_DIR "engrzoroz-s3cam-ili9486")\n'
            "    set(BUILTIN_TEXT_FONT font_noto_sans_basic_20_4)\n"
            "    set(BUILTIN_ICON_FONT font_material_symbols_20_4)\n"
            "    set(DEFAULT_EMOJI_COLLECTION noto-color-emoji_64)\n"
            "elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI)\n"
        )
        text = text.replace(anchor, entry, 1)
        cmake.write_text(text, encoding="utf-8")
        print("Patched:", cmake)
    else:
        print("CMake already patched")

    print()
    print("PATCH COMPLETE")
    print("Next:")
    print(f"  python scripts/build.py {BOARD} --language en-US --wake-word disabled --zip")

if __name__ == "__main__":
    main()
