#!/usr/bin/env python3
"""Exercise the production screen/font/stream logic and render onboarding previews."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "host-tests"
BUILD.mkdir(parents=True, exist_ok=True)
APP = ROOT / "main" / "agent_link_app"
FONTS = ROOT / "assets" / "fonts" / "agent_link"
sources = [ROOT / "tests/test_passport_screen.cc", APP / "passport_screen.cc", APP / "passport_language.cc", APP / "stripe_canvas.cc",
           *[FONTS / f"raster_{name}.c" for name in ("cjk_14", "montserrat_14", "montserrat_20", "montserrat_28")]]
inventory = sorted({char for source in [*APP.glob("*.cc"), *APP.glob("*.inc"), ROOT / "tests/test_passport_screen.cc"]
                    for char in source.read_text(encoding="utf-8") if ord(char) >= 128})
(BUILD / "glyphs.txt").write_text("".join(inventory), encoding="utf-8")
environment = os.environ.copy()
if os.name == "nt":
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    setup = Path(installation) / "VC/Auxiliary/Build/vcvarsall.bat"
    result = subprocess.check_output(f'"{setup}" x64 >nul && set', shell=True, text=True)
    for line in result.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            environment[key] = value
    compiler = shutil.which("cl.exe", path=environment.get("Path") or environment.get("PATH"))
    binary = BUILD / "passport-screen.exe"
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/utf-8", "/TP", "/W4",
               "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(APP), *map(str, sources),
               "/Fo" + BUILD.as_posix() + "/", "/Fe" + str(binary)]
else:
    binary = BUILD / "passport-screen"
    command = ["c++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-I" + str(APP),
               *map(str, sources), "-o", str(binary)]
subprocess.run(command, env=environment, cwd=BUILD, check=True)
subprocess.run([str(binary), str(BUILD / "glyphs.txt"), str(BUILD)], check=True)
try:
    from PIL import Image
except ImportError:
    print("Screen previews saved as PPM; install Pillow to also export PNG.")
else:
    names = tuple(f"{language}-{page}" for language in ("zh", "en") for page in ("setup", "pairing", "expired", "ready", "language"))
    gallery = Image.new("RGB", (240 * 5, 320 * 2))
    for index, name in enumerate(names):
        with Image.open(BUILD / (name + ".ppm")) as preview:
            preview.save(BUILD / (name + ".png"))
            gallery.paste(preview, ((index % 5) * 240, (index // 5) * 320))
    gallery.save(BUILD / "onboarding-preview.png")
    print("Preview:", BUILD / "onboarding-preview.png")
