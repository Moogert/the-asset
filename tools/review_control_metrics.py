"""Conservative source-review metrics for the scoped C++ implementation.

Counts branches AND short-circuit operators in Clang's AST. Physical function
lines are an upper-bound aid, not a certified logical-SLOC counter. These metrics
support manual JSF rules 1/3 review; they do not establish overall compliance.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FUNCTIONS = {"FunctionDecl", "CXXMethodDecl", "CXXConstructorDecl", "CXXDestructorDecl"}
BRANCHES = {"IfStmt", "ForStmt", "WhileStmt", "DoStmt", "CaseStmt", "ConditionalOperator"}


def complexity(node):
    added = int(node.get("kind") in BRANCHES)
    added += int(node.get("kind") == "BinaryOperator" and node.get("opcode") in ("&&", "||"))
    return added + sum(complexity(child) for child in node.get("inner", []))


def definitions(node):
    if node.get("kind") in FUNCTIONS and not node.get("loc", {}).get("includedFrom"):
        bodies = [child for child in node.get("inner", []) if child.get("kind") == "CompoundStmt"]
        for body in bodies:
            yield node, body
    for child in node.get("inner", []):
        yield from definitions(child)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, type=Path)
    args = parser.parse_args()
    compiler = args.compiler.resolve(strict=True)
    sources = json.loads((ROOT / "configs/control-source-scope.json").read_text())["files"]
    results = []
    hashes = {}
    for name in sources:
        if not name.endswith(".cpp"):
            continue
        source = ROOT / name
        data = source.read_bytes()
        hashes[name] = hashlib.sha256(data).hexdigest()
        text = data.decode("utf-8")
        command = [
            str(compiler),
            "-std=c++03",
            "-fno-exceptions",
            "-fno-rtti",
            "-I",
            str(ROOT / "include"),
            "-Xclang",
            "-ast-dump=json",
            "-fsyntax-only",
            str(source),
        ]
        run = subprocess.run(command, check=True, capture_output=True, text=True)
        for declaration, body in definitions(json.loads(run.stdout)):
            start = declaration["range"]["begin"].get("offset")
            end = declaration["range"]["end"].get("offset")
            if start is None or end is None:
                raise ValueError("AST lacks source offsets")
            results.append(
                {
                    "file": name,
                    "function": declaration["name"],
                    "physical_lines": text[start : end + 1].count("\n") + 1,
                    "branch_count_with_short_circuit_plus_one": 1 + complexity(body),
                }
            )
    report = {
        "status": "manual_review_aid_not_compliance",
        "source_sha256": hashes,
        "compiler": subprocess.run(
            [str(compiler), "--version"], check=True, capture_output=True, text=True
        ).stdout.splitlines()[0],
        "definition": "1 + if/for/while/do/case/conditional + short-circuit && and ||",
        "functions": results,
    }
    (ROOT / "docs/capstone/evidence/control-metrics.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(
        json.dumps(
            {
                "functions": len(results),
                "max_physical_lines": max(row["physical_lines"] for row in results),
                "max_branch_metric": max(
                    row["branch_count_with_short_circuit_plus_one"] for row in results
                ),
            },
            indent=2,
        )
    )
    if any(
        row["physical_lines"] > 200 or row["branch_count_with_short_circuit_plus_one"] > 20
        for row in results
    ):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
