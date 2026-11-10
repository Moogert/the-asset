"""Validate/export project netlist and Mermaid sources; no vehicle connection or pin inference."""

import argparse
import csv
import io
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def exports():
    data = json.loads((ROOT / "configs/emulator-harness-connectors.json").read_text())
    rows = []
    seen = set()
    for connector in data["connectors"]:
        assert connector["id"] not in seen, "Duplicate project connector"
        seen.add(connector["id"])
        positions = [row[0] for row in connector["cavities"]]
        assert positions == list(range(1, len(positions) + 1)), "Missing/duplicate cavity"
        for cavity, net, internal in connector["cavities"]:
            rows.append(
                {
                    "connector": connector["id"],
                    "cavity": cavity,
                    "net": net,
                    "internal": internal,
                    "honda_connector": "",
                    "honda_cavity": "",
                    "wire_gauge": "",
                    "terminal_part_number": "",
                    "verification": "pending",
                }
            )
    stream = io.StringIO(newline="")
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    result = {ROOT / "docs/capstone/emulator-harness-netlist.csv": stream.getvalue()}
    guide = (ROOT / "docs/capstone/ENGINE_SIMULATOR_INSTALL.md").read_text(encoding="utf-8")
    charts = re.findall(r"```mermaid\n(.*?)\n```", guide, re.S)
    names = ["engine-install-topology", "engine-interface-gating", "engine-simulator-states"]
    assert len(charts) == len(names), "Expected three Mermaid diagrams"
    for name, chart in zip(names, charts):
        result[ROOT / "docs/capstone/diagrams" / (name + ".mmd")] = chart + "\n"
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for path, text in exports().items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                raise SystemExit("Export missing or stale: " + str(path))
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8", newline="\n")
    print(
        "Project connector schedule and three Mermaid sources "
        + ("checked" if args.check else "exported")
    )


if __name__ == "__main__":
    main()
