"""Record JSF review evidence; neither scan mode establishes compliance.

Usage: python tools/run_jsf_review.py --checker PATH/TO/jsfav.exe
Add --compdb BUILD_DIR to analyze the scoped C++ control implementation.
Without --compdb this records the historical limited C-header baseline only.
"""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SELECTED = ("include/dual_drive.h", "include/prius_inverter_adapter.h")
CONTROL = ("src/core/dual_drive.c", "src/core/prius_inverter_adapter.c")


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checker", required=True, type=Path)
    parser.add_argument("--compdb", type=Path)
    args = parser.parse_args()
    checker = args.checker.resolve(strict=True)
    evidence = ROOT / "docs/capstone/evidence"
    evidence.mkdir(parents=True, exist_ok=True)
    version = subprocess.run(
        [str(checker), "version"], check=True, capture_output=True, text=True
    ).stdout.strip()
    selected = SELECTED
    stem = "jsf-header-baseline"
    manifest_name = "jsf-baseline-manifest.json"
    database = None
    scope_path = ROOT / "configs/control-source-scope.json"
    if args.compdb:
        selected = tuple(json.loads(scope_path.read_text(encoding="utf-8"))["files"])
        database = args.compdb.resolve(strict=True)
        if database.is_dir():
            database = database / "compile_commands.json"
        entries = json.loads(database.read_text(encoding="utf-8"))
        compiled = set()
        for entry in entries:
            source = Path(entry["file"])
            if not source.is_absolute():
                source = Path(entry["directory"]) / source
            compiled.add(source.resolve())
        required = {(ROOT / name).resolve() for name in selected if name.endswith(".cpp")}
        if not required.issubset(compiled):
            parser.error("Compilation database does not cover every scoped implementation")
        stem = "jsf-cpp-review"
        manifest_name = "jsf-cpp-manifest.json"
    destination = evidence / (stem + ".json")
    command = [
        str(checker),
        "scan",
        *selected,
        "--profile",
        "literal",
        "--format",
        "json",
        "--output",
        str(destination),
    ]
    if database:
        command.extend(["-p", str(database)])
    subprocess.run(command, cwd=ROOT, check=True)
    scan = json.loads(destination.read_text(encoding="utf-8"))
    catalog_path = checker.parent / "data/rules.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    record = {
        "status": (
            "cpp_automated_review_not_compliance"
            if database
            else "partial_header_scan_not_compliance"
        ),
        "tool_version": version,
        "tool_path": str(checker),
        "tool_sha256": digest(checker),
        "catalog_sha256": digest(catalog_path),
        "command": command,
        "working_directory": str(ROOT),
        "selected_files_sha256": {name: digest(ROOT / name) for name in selected},
        "unscanned_c_implementations_sha256": {name: digest(ROOT / name) for name in CONTROL},
        "limitations": [
            "No compilation database",
            "No semantic C++ analysis",
            "C11 implementation excluded by checker suffix selection",
            "Every rule disposition remains open",
        ],
        "profile": scan["profile"],
        "partial": scan["partial"],
        "coverage": scan["coverage"],
        "summary": scan["summary"],
    }
    if database:
        record["limitations"] = [
            "193 catalog rules require manual review",
            "Automated checks are review aids, not complete proofs for their rules",
            "Host build does not prove MCU timing or physical inhibit behavior",
            "Scope, platform assumptions and deviations require review",
        ]
        record["compilation_database_sha256"] = digest(database)
        record["scope_sha256"] = digest(scope_path)
        record["cmake_sha256"] = digest(ROOT / "CMakeLists.txt")
        (evidence / "jsf-cpp-compile-commands.json").write_bytes(database.read_bytes())
        sarif_command = command[:]
        sarif_command[sarif_command.index("--format") + 1] = "sarif"
        sarif_command[sarif_command.index("--output") + 1] = str(evidence / (stem + ".sarif"))
        subprocess.run(sarif_command, cwd=ROOT, check=True)
    (evidence / manifest_name).write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    # Preserve completed manual work on subsequent runs.
    matrix = evidence / "jsf-rule-review.csv"
    if not matrix.exists():
        with matrix.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(
                [
                    "rule_id",
                    "pdf_page",
                    "checker_detection",
                    "catalog_description_verify_against_standard",
                    "disposition",
                    "evidence",
                    "reviewer",
                    "review_date",
                    "deviation_approval_reference",
                ]
            )
            for rule in catalog["rules"]:
                writer.writerow(
                    [
                        rule["id"],
                        rule["pdf_page"],
                        rule["detection"],
                        rule["description"],
                        "open",
                        "",
                        "",
                        "",
                        "",
                    ]
                )
    print(
        json.dumps(
            {"status": record["status"], "summary": scan["summary"], "coverage": scan["coverage"]},
            indent=2,
        )
    )
    if database and (scan["partial"] or scan["summary"]["violations"]):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
