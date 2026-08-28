#!/usr/bin/env python3
import importlib.util
import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "build_html.py"

with tempfile.TemporaryDirectory() as directory:
    previous = Path.cwd()
    os.chdir(directory)
    try:
        spec = importlib.util.spec_from_file_location("build_html", SCRIPT)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        first = module.compress_html("<html>  <body>test</body> </html>")
        second = module.compress_html("<html>  <body>test</body> </html>")
    finally:
        os.chdir(previous)

assert first == second
assert first[4:8] == b"\x00\x00\x00\x00"
print("build HTML determinism tests passed")
