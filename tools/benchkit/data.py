"""Manifest, import, provenance and synchronized trace validation."""

from dataclasses import dataclass
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil

import numpy as np

CLASSES = {"measured", "simulated", "synthetic_demo"}
UNITS = {
    "A": ("A", 1.0),
    "mA": ("A", 0.001),
    "V": ("V", 1.0),
    "mV": ("V", 0.001),
    "Nm": ("Nm", 1.0),
    "rpm": ("rpm", 1.0),
    "rad/s": ("rpm", 60 / (2 * math.pi)),
    "degC": ("degC", 1.0),
    "L/min": ("L/min", 1.0),
    "%": ("%", 1.0),
    "bool": ("bool", 1.0),
    "W": ("W", 1.0),
    "kW": ("W", 1000.0),
}
COLUMNS = ("time_s", "source", "signal", "value", "unit", "valid")
TYPES = {"current_step", "disturbance", "torque", "battery_limit", "fault", "thermal"}


def loadjson(path):
    return json.loads(
        Path(path).read_text(encoding="utf-8-sig"),
        parse_constant=lambda x: (_ for _ in ()).throw(ValueError(f"Nonfinite JSON: {x}")),
    )


def writejson(path, value):
    Path(path).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def sha(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def number(value, name, low=-1e12, high=1e12):
    if isinstance(value, bool):
        raise ValueError(f"{name}: boolean is not a measurement")
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f"{name}: finite number required") from error
    if not math.isfinite(result) or not low <= result <= high:
        raise ValueError(f"{name}: finite number in [{low}, {high}] required")
    return result


def identifier(value, name):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,79}", value):
        raise ValueError(f"{name}: use 1-80 letters, digits, underscores or hyphens")
    return value


def window(value, name):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"{name}: [start_s, end_s] required")
    start, end = (number(v, name) for v in value)
    if start >= end:
        raise ValueError(f"{name}: end must follow start")
    return start, end


# Reject ambiguous evidence classes, channel definitions, timing and analysis windows before import.
def validate_manifest(manifest, require_captures=True):
    if not isinstance(manifest, dict):
        raise ValueError("Manifest must be a JSON object")
    if manifest.get("schema_version") != 1:
        raise ValueError("Manifest schema_version must be 1")
    identifier(manifest.get("run_id"), "run_id")
    if manifest.get("evidence_class") not in CLASSES:
        raise ValueError("evidence_class must be measured, simulated or synthetic_demo")
    for field in ("hardware", "firmware", "controller_settings", "operating_conditions"):
        if not isinstance(manifest.get(field), dict):
            raise ValueError(f"{field}: object required; unknown values must be null")
    if not isinstance(manifest.get("fixture_id"), (str, type(None))):
        raise ValueError("fixture_id must be a string or null")
    timebase = manifest.get("timebase", {})
    if (
        not isinstance(timebase, dict)
        or not isinstance(timebase.get("reference"), str)
        or not timebase["reference"]
    ):
        raise ValueError("timebase.reference required")
    timebase["max_alignment_uncertainty_s"] = number(
        timebase.get("max_alignment_uncertainty_s"), "alignment limit", 0, 1
    )
    sources = manifest.get("sources", {})
    channels = manifest.get("channels", {})
    if (
        not isinstance(sources, dict)
        or not isinstance(channels, dict)
        or not sources
        or not channels
    ):
        raise ValueError("sources and channels are required")
    for name, source in sources.items():
        identifier(name, "source")
        if not isinstance(source, dict):
            raise ValueError("Each source must be an object")
        if source.get("kind") not in {"scope", "daq", "can", "software"} or not source.get(
            "clock_id"
        ):
            raise ValueError(f"{name}: kind and clock_id required")
        if not isinstance(source.get("synchronized"), bool):
            raise ValueError(f"{name}: synchronized must be boolean")
        for key in ("offset_s", "alignment_uncertainty_s"):
            if source.get(key) is not None:
                source[key] = number(
                    source[key], f"{name}.{key}", 0 if key.startswith("alignment") else -1e12
                )
    for name, channel in channels.items():
        identifier(name, "signal")
        if not isinstance(channel, dict):
            raise ValueError("Each channel must be an object")
        if channel.get("source") not in sources or channel.get("unit") not in {
            v[0] for v in UNITS.values()
        }:
            raise ValueError(f"{name}: declared source and canonical unit required")
        if channel.get("measurement") not in {"physical", "command", "estimated"}:
            raise ValueError(f"{name}: measurement must be physical, command or estimated")
        channel["max_gap_s"] = number(channel.get("max_gap_s"), f"{name}.max_gap_s", 1e-9, 1e6)
        calibration = channel.get("calibration")
        if calibration is not None:
            if not isinstance(calibration, dict):
                raise ValueError("calibration must be an object or null")
            calibration["expanded_uncertainty"] = number(
                calibration.get("expanded_uncertainty"), f"{name}.uncertainty", 0
            )
            calibration["coverage_factor"] = number(
                calibration.get("coverage_factor"), f"{name}.coverage_factor", 1, 10
            )
            if not calibration.get("reference"):
                raise ValueError(f"{name}: calibration reference required")
    experiments = manifest.get("experiments", [])
    if not isinstance(experiments, list) or not experiments:
        raise ValueError("Nonempty experiments list required")
    seen = set()
    for experiment in experiments:
        if not isinstance(experiment, dict):
            raise ValueError("Each experiment must be an object")
        name = identifier(experiment.get("id"), "experiment.id")
        if name in seen:
            raise ValueError("Duplicate experiment id")
        seen.add(name)
        if experiment.get("type") not in TYPES:
            raise ValueError(f"{name}: unknown experiment type")
        start, end = window(experiment.get("window_s"), name)
        experiment["window_s"] = [start, end]
        if not isinstance(experiment.get("signals"), dict):
            raise ValueError(f"{name}: signals object required")
        if any(signal not in channels for signal in experiment["signals"].values()):
            raise ValueError(f"{name}: unknown signal")
        if "event_s" in experiment and not start < number(experiment["event_s"], name) < end:
            raise ValueError(f"{name}: event must be inside the analysis window")
        if "event_s" in experiment:
            experiment["event_s"] = number(experiment["event_s"], "event_s")
        for key in ("baseline_s", "steady_s"):
            if key in experiment:
                left, right = window(experiment[key], key)
                if left < start or right > end:
                    raise ValueError(f"{name}: {key} outside analysis window")
                experiment[key] = [left, right]
    captures = manifest.get("captures", [])
    if not isinstance(captures, list):
        raise ValueError("captures must be a list")
    if require_captures and not captures:
        raise ValueError("No capture files declared")
    for capture in captures:
        if not isinstance(capture, dict):
            raise ValueError("Each capture must be an object")
        if capture.get("evidence_class") != manifest["evidence_class"]:
            raise ValueError("Mixed evidence classes in run captures")
        if not capture.get("path") or not capture.get("sha256"):
            raise ValueError("Capture path and sha256 required")
    return manifest


@dataclass
class Trace:
    time: np.ndarray
    value: np.ndarray
    valid: np.ndarray


class Run:
    def __init__(self, path):
        self.path = Path(path).resolve()
        self.manifest = validate_manifest(loadjson(self.path))
        collected = {name: [] for name in self.manifest["channels"]}
        self.provenance = {str(self.path): sha(self.path)}
        imported = self.manifest.get("import_provenance", {})
        for item in imported if isinstance(imported, list) else [imported]:
            for path_key, hash_key in (
                ("raw_copy_path", "original_sha256"),
                ("mapping_path", "mapping_sha256"),
            ):
                if item.get(path_key):
                    original = self.path.parent / item[path_key]
                    if sha(original) != item[hash_key]:
                        raise ValueError(f"Raw input/mapping provenance mismatch: {original}")
                    self.provenance[str(original.resolve())] = item[hash_key]
        last = {}
        for capture in self.manifest["captures"]:
            path = self.path.parent / capture["path"]
            if sha(path) != capture["sha256"]:
                raise ValueError(f"Capture hash mismatch: {path}")
            self.provenance[str(path.resolve())] = capture["sha256"]
            with path.open(newline="", encoding="utf-8-sig") as stream:
                reader = csv.DictReader(stream)
                if tuple(reader.fieldnames or ()) != COLUMNS:
                    raise ValueError(f"{path}: canonical columns must be {COLUMNS}")
                for row in reader:
                    if None in row:
                        raise ValueError("Malformed canonical CSV row")
                    name = row["signal"]
                    if name not in collected:
                        raise ValueError(f"Undeclared signal: {name}")
                    channel = self.manifest["channels"][name]
                    if row["source"] != channel["source"] or row["unit"] != channel["unit"]:
                        raise ValueError(f"{name}: source/unit differs from manifest")
                    time = number(row["time_s"], "sample time")
                    if name in last and time <= last[name]:
                        raise ValueError(
                            f"{name}: duplicate/regressing timestamp; input is not silently sorted"
                        )
                    last[name] = time
                    if row["valid"] not in {"0", "1"}:
                        raise ValueError("Canonical valid must be 0 or 1")
                    valid = row["valid"] == "1"
                    value = number(row["value"], name) if row["value"] != "" else None
                    if valid and value is None:
                        raise ValueError(f"{name}: valid sample has no value")
                    if value is not None and row["unit"] == "bool" and value not in (0, 1):
                        raise ValueError(f"{name}: boolean channel must be 0 or 1")
                    source = self.manifest["sources"][channel["source"]]
                    offset = source.get("offset_s")
                    collected[name].append(
                        (
                            time + (offset if offset is not None else 0),
                            value if value is not None else np.nan,
                            valid,
                        )
                    )
        self.traces = {}
        for name, samples in collected.items():
            array = np.asarray(samples, dtype=float).reshape((-1, 3))
            self.traces[name] = Trace(array[:, 0], array[:, 1], array[:, 2].astype(bool))

    def problems(self, names, bounds, minimum=10, synchronize=True):
        issues = []
        for name in names:
            channel = self.manifest["channels"][name]
            source = self.manifest["sources"][channel["source"]]
            trace = self.traces[name]
            mask = (trace.time >= bounds[0]) & (trace.time <= bounds[1])
            t = trace.time[mask]
            if len(t) < minimum:
                issues.append(f"{name}: insufficient samples")
            if len(t) and (
                t[0] - bounds[0] > channel["max_gap_s"] or bounds[1] - t[-1] > channel["max_gap_s"]
            ):
                issues.append(f"{name}: recording does not cover the window")
            if not np.all(trace.valid[mask]):
                issues.append(f"{name}: invalid samples inside window")
            if len(t) > 1 and np.max(np.diff(t)) > channel["max_gap_s"] * (1 + 1e-9):
                issues.append(f"{name}: sample gap exceeds declared limit")
            if synchronize and (
                not source["synchronized"]
                or source.get("offset_s") is None
                or source.get("alignment_uncertainty_s") is None
                or source["alignment_uncertainty_s"]
                > self.manifest["timebase"]["max_alignment_uncertainty_s"]
            ):
                issues.append(f"{name}: timing alignment not established")
        return issues

    def section(self, name, bounds):
        trace = self.traces[name]
        mask = (trace.time >= bounds[0]) & (trace.time <= bounds[1])
        return trace.time[mask], trace.value[mask]

    def mean(self, name, bounds):
        t, y = self.section(name, bounds)
        if len(t) < 2:
            raise ValueError(f"{name}: at least two samples needed for time average")
        return float(np.trapezoid(y, t) / (t[-1] - t[0]))

    def aligned(self, names, bounds):
        # Call problems first. This never bridges invalid samples/gaps or extrapolates.
        sections = [self.section(name, bounds) for name in names]
        left = max(t[0] for t, _ in sections)
        right = min(t[-1] for t, _ in sections)
        if right <= left:
            raise ValueError("No overlapping time interval")
        time = np.unique(np.concatenate([t[(t >= left) & (t <= right)] for t, _ in sections]))
        return time, [np.interp(time, t, y) for t, y in sections]


# Copy original capture and explicit mapping, then normalize units without modifying the source file.
def import_capture(raw_path, mapping_path, template_path, destination):
    raw_path, destination = Path(raw_path), Path(destination)
    mapping = loadjson(mapping_path)
    if not isinstance(mapping, dict) or not isinstance(mapping.get("channels"), dict):
        raise ValueError("Mapping must be an object with a channels object")
    manifest = validate_manifest(loadjson(template_path), require_captures=False)
    source = mapping.get("source")
    if source not in manifest["sources"]:
        raise ValueError("Mapping source must be declared")
    factor = {"s": 1, "ms": 0.001, "us": 1e-6}.get(mapping.get("time_unit"))
    if factor is None or not mapping.get("channels"):
        raise ValueError("Mapping needs time_unit=s/ms/us and channels")
    rows = []
    mapped = set()
    with raw_path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        fields = reader.fieldnames or []
        if not fields or len(set(fields)) != len(fields):
            raise ValueError("CSV has missing/duplicate column names")
        if mapping.get("time_column") not in fields:
            raise ValueError("Time column missing")
        for column, entry in mapping["channels"].items():
            signal = entry.get("signal")
            if column not in fields or signal not in manifest["channels"] or signal in mapped:
                raise ValueError("Missing column, undeclared signal or duplicate signal mapping")
            mapped.add(signal)
            if entry.get("valid_column") and entry["valid_column"] not in fields:
                raise ValueError("Validity column missing")
            if entry.get("unit") not in UNITS:
                raise ValueError("Unsupported input unit")
            channel = manifest["channels"][signal]
            if UNITS[entry["unit"]][0] != channel["unit"] or channel["source"] != source:
                raise ValueError("Mapping unit/source incompatible with manifest")
            number(entry.get("scale", 1), "mapping.scale")
            number(entry.get("offset", 0), "mapping.offset")
        last_time = None
        for record in reader:
            if None in record:
                raise ValueError("Malformed raw CSV row")
            time = number(record[mapping["time_column"]], "raw time") * factor
            if last_time is not None and time <= last_time:
                raise ValueError("Raw timestamps duplicate/regress; not sorted automatically")
            last_time = time
            for column, entry in mapping["channels"].items():
                flag = record[entry["valid_column"]] if entry.get("valid_column") else "1"
                if flag not in {"0", "1"}:
                    raise ValueError("Raw validity must be 0 or 1")
                valid = flag == "1"
                value = record[column]
                canonical_unit, conversion = UNITS[entry["unit"]]
                if value != "":
                    value = (
                        number(value, column) * number(entry.get("scale", 1), "scale")
                        + number(entry.get("offset", 0), "offset")
                    ) * conversion
                    number(value, "converted sample")
                elif valid:
                    raise ValueError("Valid raw sample has no value")
                rows.append((time, source, entry["signal"], value, canonical_unit, int(valid)))
    if not rows:
        raise ValueError("Empty capture")
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "raw").mkdir()
    shutil.copy2(raw_path, destination / "raw" / raw_path.name)
    shutil.copy2(mapping_path, destination / "mapping.json")
    canonical = destination / "capture.csv"
    with canonical.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(COLUMNS)
        writer.writerows(rows)
    manifest["captures"] = [
        {
            "path": "capture.csv",
            "sha256": sha(canonical),
            "evidence_class": manifest["evidence_class"],
        }
    ]
    manifest["import_provenance"] = {
        "original_sha256": sha(raw_path),
        "original_path": str(raw_path.resolve()),
        "raw_copy_path": "raw/" + raw_path.name,
        "mapping_path": "mapping.json",
        "mapping_sha256": sha(mapping_path),
    }
    writejson(destination / "manifest.json", manifest)
    Run(destination / "manifest.json")
    return destination / "manifest.json"


# Combine declared synchronized captures while retaining per-file hashes and independent provenance.
def import_captures(raw_paths, mapping_paths, template_path, destination):
    """Combine independently mapped files without guessing their clock alignment."""
    if not raw_paths or len(raw_paths) != len(mapping_paths):
        raise ValueError("Provide one mapping for each CSV, in matching order")
    if len(raw_paths) == 1:
        return import_capture(raw_paths[0], mapping_paths[0], template_path, destination)
    destination = Path(destination)
    combined = validate_manifest(loadjson(template_path), require_captures=False)
    destination.mkdir(parents=True, exist_ok=False)
    combined["captures"] = []
    combined["import_provenance"] = []
    for index, (raw, mapping) in enumerate(zip(raw_paths, mapping_paths), 1):
        prefix = f"source{index:02d}"
        child_path = import_capture(raw, mapping, template_path, destination / prefix)
        child = loadjson(child_path)
        for capture in child["captures"]:
            combined["captures"].append({**capture, "path": prefix + "/" + capture["path"]})
        provenance = child["import_provenance"]
        for key in ("raw_copy_path", "mapping_path"):
            provenance[key] = prefix + "/" + provenance[key]
        combined["import_provenance"].append(provenance)
    path = destination / "manifest.json"
    writejson(path, combined)
    Run(path)
    return path
