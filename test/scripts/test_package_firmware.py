#!/usr/bin/env python3
import importlib.util
import json
import tempfile
from hashlib import sha256
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "package_firmware.py"

spec = importlib.util.spec_from_file_location("package_firmware", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root / "lib/BoardSupport/include").mkdir(parents=True)
    (root / "lib/BoardSupport/include/HardwareIdentity.h").write_text(
        "inline constexpr uint8_t kProfileSchemaVersion = 7;\n", encoding="utf-8"
    )
    (root / "platformio.ini").write_text(
        "[papyrix]\nversion = 9.8.7\n[base]\nboard_upload.offset_address = 0x10000\n", encoding="utf-8"
    )
    payloads = {
        "release_xteink_c3": b"c3-image",
        "release_x4pro": b"x4pro-image",
        "release_x4c": b"classic-image",
    }
    for environment, payload in payloads.items():
        build = root / ".pio/build" / environment
        build.mkdir(parents=True)
        (build / "firmware.bin").write_bytes(payload)

    output = root / "dist"
    module.package(root, output, build=False, check_features=False)
    first_manifest = (output / "manifest.json").read_bytes()
    module.package(root, output, build=False, check_features=False)
    assert (output / "manifest.json").read_bytes() == first_manifest

    manifest = json.loads(first_manifest)
    assert manifest["version"] == "9.8.7"
    assert manifest["profile_schema"] == 7
    assert manifest["flash_offset"] == "0x10000"
    assert [entry["filename"] for entry in manifest["artifacts"]] == [
        "papyrix-xteink-c3.bin",
        "papyrix-x4pro.bin",
        "papyrix-x4c.bin",
    ]
    for entry in manifest["artifacts"]:
        payload = payloads[entry["environment"]]
        assert entry["sha256"] == sha256(payload).hexdigest()
        assert (output / entry["filename"]).read_bytes() == payload
    classic = manifest["artifacts"][2]
    assert classic["boards"] == ["X4Classic"]
    assert classic["panels"] == ["SSD1677", "UC8179_X4PRO", "UC8279_X4PRO"]
    (root / ".pio/build/release_x4c/firmware.bin").unlink()
    try:
        module.package(root, output, build=False, check_features=False)
    except (FileNotFoundError, RuntimeError):
        pass
    else:
        raise AssertionError("missing Classic input must fail packaging")

print("package firmware tests passed")
