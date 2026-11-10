"""Check capstone formatting and compare existing source semantics with the saved baseline.

This is a review aid, not a formal equivalence proof or a compliance checker.
No source files are rewritten. An optional JSON report records checks and hashes.
"""

import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/capstone/evidence"

# Keep string/character literals intact; comment markers inside a literal are data.
C_TOKENS = re.compile(
    r"//[^\n]*|/\*[\s\S]*?\*/"
    r'|"(?:\\[\s\S]|[^"\\])*"'
    r"|'(?:\\[\s\S]|[^'\\])*'"
    r"|[A-Za-z_]\w*|(?:\d[\w.]*)"
    r"|>>=|<<=|->\*|\.\.\.|::|->|\+\+|--|&&|\|\||<=|>=|==|!="
    r"|<<|>>|\+=|-=|\*=|/=|%=|&=|\|=|\^=|[^\s]"
)


def file_sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def c_token_sha(path):
    # Translation phase two joins escaped lines before preprocessing and tokenization.
    text = re.sub(r"\\\r?\n", "", path.read_text(encoding="utf-8"))
    tokens = [
        match.group()
        for match in C_TOKENS.finditer(text)
        if not match.group().startswith(("//", "/*"))
    ]
    return hashlib.sha256("\0".join(tokens).encode()).hexdigest()


def python_ast_sha(path):
    # Ignore line/column positions, so comments/formatting do not alter the comparison.
    tree = ast.parse(path.read_text(encoding="utf-8"))
    return hashlib.sha256(ast.dump(tree, include_attributes=False).encode()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang-format", required=True, type=Path)
    parser.add_argument("--black-python", required=True, type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument(
        "--format-only",
        action="store_true",
        help="Check current formatting without claiming equivalence to the historical cleanup",
    )
    args = parser.parse_args()
    c_baseline = json.loads((EVIDENCE / "readability-format-baseline.json").read_text())
    py_baseline = json.loads((EVIDENCE / "readability-python-baseline.json").read_text())
    cpp = [ROOT / name for name in c_baseline["baseline_token_hashes"]]
    python = [ROOT / name for name in py_baseline["ast_before"]]
    if args.format_only:
        # Include release additions, but never walk generated .pio/build dependencies.
        additions = [
            ROOT / name
            for name in json.loads((ROOT / "configs/control-source-scope.json").read_text())[
                "files"
            ]
        ]
        for folder in ("firmware/src", "firmware/include", "firmware/lib/CapstoneIO/src"):
            additions.extend(
                path for path in (ROOT / folder).iterdir() if path.suffix in (".cpp", ".h")
            )
        additions.extend(
            [
                ROOT / "tests/test_controller_revision.cpp",
                ROOT / "tests/test_emulator_io.cpp",
                ROOT / "src/app/controller_revision_demo.cpp",
            ]
        )
        cpp = sorted(set(cpp + additions))
        python.extend(
            [
                ROOT / "tools/controller_revision_demo.py",
                ROOT / "tools/build_controller_target.py",
                ROOT / "tools/export_emulator_harness.py",
            ]
        )
    errors = []

    # Exact non-comment tokens/AST must remain unchanged in this readability-only pass.
    for path in [] if args.format_only else cpp:
        name = path.relative_to(ROOT).as_posix()
        if c_token_sha(path) != c_baseline["baseline_token_hashes"][name]:
            errors.append(f"C/C++ token difference: {name}")
    for path in [] if args.format_only else python:
        name = path.relative_to(ROOT).as_posix()
        if python_ast_sha(path) != py_baseline["ast_before"][name]:
            errors.append(f"Python AST difference: {name}")

    commands = [
        [
            str(args.clang_format.resolve()),
            "--style=file:" + str(ROOT / "configs/capstone-cpp-format.yaml"),
            "--dry-run",
            "--Werror",
            *map(str, cpp),
        ],
        [
            str(args.black_python.resolve()),
            "-m",
            "black",
            "--check",
            "--line-length",
            "100",
            *map(str, python),
            str(Path(__file__).resolve()),
        ],
    ]
    for command in commands:
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
        if result.returncode:
            errors.append(result.stdout + result.stderr)

    sources = cpp + python + [Path(__file__).resolve()]
    report = {
        "comparison_mode": "format_only" if args.format_only else "historical_semantic_baseline",
        "status": "passed" if not errors else "failed",
        "cpp_files": len(cpp),
        "existing_python_files": len(python),
        "new_review_tool": "tools/check_readability.py",
        "errors": errors,
        "limitations": [
            "Lexical/AST comparison supplements builds/tests; it is not a formal proof",
            "The C/C++ lexer is for this source set, not arbitrary raw-string/preprocessor syntax",
            "Preprocessor directive placement and timing still require compiler/target validation",
            "Comments and passing tests do not establish JSF or hardware compliance",
        ],
        "source_sha256": {path.relative_to(ROOT).as_posix(): file_sha(path) for path in sources},
    }
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {key: report[key] for key in ("status", "cpp_files", "existing_python_files", "errors")}
        )
    )
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
