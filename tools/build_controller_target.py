"""Cross-compile portable control objects for STM32F107; this does not build flashable firmware."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = json.loads((ROOT / "configs/control-source-scope.json").read_text())["files"]
    flags = [
        "-std=c++03",
        "-mcpu=cortex-m3",
        "-mthumb",
        "-mfloat-abi=soft",
        "-fno-exceptions",
        "-fno-rtti",
        "-fno-fast-math",
        "-ffunction-sections",
        "-fdata-sections",
        "-Os",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wconversion",
        "-Wshadow",
        "-Werror",
        "-I",
        str(ROOT / "include"),
    ]
    evidence = {
        "status": "portable_objects_only_no_linker_startup_or_physical_port",
        "target": "STM32F107 Cortex-M3 soft-float",
        "compiler": subprocess.check_output(
            [str(args.compiler), "--version"], text=True
        ).splitlines()[0],
        "flags": flags,
        "objects": [],
    }
    for name in sources:
        if not name.endswith(".cpp"):
            continue
        source = ROOT / name
        output = args.output / (source.stem + ".o")
        subprocess.run(
            [str(args.compiler), *flags, "-c", str(source), "-o", str(output)], check=True
        )
        evidence["objects"].append(
            {
                "source": name,
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "object_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            }
        )
    (args.output / "manifest.json").write_text(
        json.dumps(evidence, indent=2) + "\n", encoding="utf-8"
    )
    print(
        json.dumps(
            {"target": evidence["target"], "objects": len(evidence["objects"]), "flashable": False}
        )
    )


if __name__ == "__main__":
    main()
