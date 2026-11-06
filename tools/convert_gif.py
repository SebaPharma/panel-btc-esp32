"""Convierte todos los GIF de data/ en include/gif_data.h.

PlatformIO lo ejecuta solo antes de compilar (ver extra_scripts en platformio.ini).
También puedes correrlo a mano:  python tools/convert_gif.py

Para usar tus propios GIF: copia archivos de 64x64 px en data/ y compila.
Se muestran en orden alfabético, cambiando cada 10 segundos.
"""
import os
import re

try:
    Import("env")  # noqa: F821  (definido por PlatformIO/SCons)
    PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DATA_DIR = os.path.join(PROJECT_DIR, "data")
OUT_FILE = os.path.join(PROJECT_DIR, "include", "gif_data.h")


def c_name(filename):
    base = os.path.splitext(filename)[0].lower()
    return "gif_" + re.sub(r"[^a-z0-9]+", "_", base).strip("_")


def to_array(path, name):
    with open(path, "rb") as f:
        data = f.read()
    lines = []
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    return f"static const uint8_t {name}[] = {{\n" + "\n".join(lines) + "\n};\n"


def main():
    gifs = sorted(f for f in os.listdir(DATA_DIR) if f.lower().endswith(".gif"))
    if not gifs:
        raise SystemExit("No hay archivos .gif en data/")

    parts = [
        "// Archivo generado por tools/convert_gif.py. No lo edites a mano.\n",
        "#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n",
        "struct GifAsset {\n  const uint8_t* data;\n  size_t len;\n};\n\n",
    ]
    names = []
    for g in gifs:
        name = c_name(g)
        names.append(name)
        parts.append(to_array(os.path.join(DATA_DIR, g), name) + "\n")

    parts.append("static const GifAsset GIFS[] = {\n")
    parts.extend(f"  {{{n}, sizeof({n})}},\n" for n in names)
    parts.append("};\nstatic const int GIF_COUNT = sizeof(GIFS) / sizeof(GIFS[0]);\n")

    content = "".join(parts)
    old = open(OUT_FILE).read() if os.path.exists(OUT_FILE) else None
    if content != old:
        with open(OUT_FILE, "w") as f:
            f.write(content)
    print(f"gif_data.h: {len(gifs)} GIF ({', '.join(gifs)})")


main()
