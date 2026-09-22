#!/usr/bin/env python3
import argparse
import configparser
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ARTIFACTS = (
    {
        "environment": "release_xteink_c3",
        "checker_target": "default",
        "filename": "papyrix-xteink-c3.bin",
        "mcu": "ESP32-C3",
        "boards": ["X3", "X4"],
        "panels": ["SSD1677", "UC8253", "UC8279_X3"],
        "hardware_status": "partial",
    },
    {
        "environment": "release_x4pro",
        "checker_target": "x4pro",
        "filename": "papyrix-x4pro.bin",
        "mcu": "ESP32-S3",
        "boards": ["X4Pro"],
        "panels": ["UC8279_X4PRO", "UC8179_X4PRO"],
        "hardware_status": "partial",
    },
    {
        "environment": "release_x4c",
        "checker_target": "x4c",
        "filename": "papyrix-x4c.bin",
        "mcu": "ESP32-S3",
        "boards": ["X4Classic"],
        "panels": ["SSD1677", "UC8179_X4PRO", "UC8279_X4PRO"],
        "hardware_status": "partial",
    },
)


def load_release_contract(root: Path):
    config = configparser.ConfigParser(interpolation=None)
    config.read(root / "platformio.ini", encoding="utf-8")
    version = config["papyrix"]["version"]
    flash_offset = config["base"]["board_upload.offset_address"]
    identity = (root / "lib/BoardSupport/include/HardwareIdentity.h").read_text(encoding="utf-8")
    match = re.search(r"kProfileSchemaVersion\s*=\s*(\d+)", identity)
    if match is None:
        raise RuntimeError("profile schema version is missing")
    return version, flash_offset, int(match.group(1))


def run_builds(root: Path):
    project_pio = root / ".venv/bin/pio"
    pio = str(project_pio) if project_pio.is_file() else shutil.which("pio")
    if pio is None:
        raise RuntimeError("PlatformIO executable is missing")
    command = [pio, "run"]
    for artifact in ARTIFACTS:
        command.extend(["-e", artifact["environment"]])
    subprocess.run(command, cwd=root, check=True)


def run_feature_checks(root: Path):
    toolchains = {
        "ESP32-C3": root / ".pio/packages/toolchain-riscv32-esp/bin/riscv32-esp-elf-nm",
        "ESP32-S3": root / ".pio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-nm",
    }
    for artifact in ARTIFACTS:
        environment = artifact["environment"]
        elf = root / ".pio/build" / environment / "firmware.elf"
        symbols = root / ".pio/build" / environment / "firmware.symbols.txt"
        with symbols.open("wb") as output:
            subprocess.run([str(toolchains[artifact["mcu"]]), "-C", str(elf)], cwd=root, stdout=output, check=True)
        subprocess.run(
            [sys.executable, str(root / "scripts/check_target_features.py"), str(symbols), artifact["checker_target"]],
            cwd=root,
            check=True,
        )


def package(root: Path, output: Path, build: bool = True, check_features: bool = True):
    root = root.resolve()
    output = output.resolve()
    if build:
        run_builds(root)
    if check_features:
        run_feature_checks(root)

    version, flash_offset, profile_schema = load_release_contract(root)
    output.mkdir(parents=True, exist_ok=True)
    manifest_artifacts = []
    for artifact in ARTIFACTS:
        source = root / ".pio/build" / artifact["environment"] / "firmware.bin"
        if not source.is_file():
            raise FileNotFoundError(f"missing firmware build: {source}")
        destination = output / artifact["filename"]
        shutil.copyfile(source, destination)
        digest = hashlib.sha256(destination.read_bytes()).hexdigest()
        manifest_artifacts.append(
            {
                "filename": artifact["filename"],
                "environment": artifact["environment"],
                "checker_target": artifact["checker_target"],
                "mcu": artifact["mcu"],
                "boards": artifact["boards"],
                "panels": artifact["panels"],
                "hardware_status": artifact["hardware_status"],
                "sha256": digest,
            }
        )

    manifest = {
        "version": version,
        "flash_offset": flash_offset,
        "profile_schema": profile_schema,
        "artifacts": manifest_artifacts,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main():
    parser = argparse.ArgumentParser(description="Build and package deterministic PapyriX firmware artifacts")
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-feature-check", action="store_true")
    args = parser.parse_args()
    root = args.project_root.resolve()
    output = args.output.resolve() if args.output else root / "dist"
    package(root, output, build=not args.no_build, check_features=not args.no_feature_check)
    print(f"packaged {len(ARTIFACTS)} artifacts in {output}")


if __name__ == "__main__":
    main()
