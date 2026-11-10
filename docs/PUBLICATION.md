# Repository publication â€” 10 November 2026

## Included

The capstone C/C++ simulator and supervisor, Mega/Teensy firmware, offline Python
analysis/reporting tools, configurations, tests, engineering guides and historical
review evidence are now collected in one repository. Parking-pawl projects were
moved under `firmware/parking_pawl` without changing their logic. Their original
publication remains available at commit `70506a0`.

The automatic-transmission sketch is isolated under `legacy`; it is not the
manual-Civic baseline. The separately developed JSF checker is an optional external
tool, not part of this source import.

## Fresh checks on the publication tree

| Check | Result |
|---|---|
| Fresh MSVC host build | PASS |
| CTest, including offline bench analysis | 10/10 PASS |
| Clang strict C++03 syntax | All five control implementation files PASS |
| Formatting-only review | 56 C/C++ and 23 Python files PASS |
| Mega engine firmware | PASS |
| Teensy default EV coordinator | PASS |
| Isolated legacy Teensy OBD demonstration | Compile PASS; excluded from defaults |
| Imported source comparison | 81 source/library/test files unchanged after newline normalization |

Firmware builds reported warnings in upstream Arduino/Teensy libraries. MSBuild
reported a temporary-directory incremental-build advisory. A deeply nested build
initially hit the Windows path-length limit; the clean out-of-tree build passed.
The documented commands recommend short Windows paths. No warning is presented
as a completed hardware or JSF review.

The parking-pawl Uno builds were verified in the immediately preceding publication;
their moved source is unchanged. Current source hashes are recorded in
`publication-source-manifest.json`. No flashing or physical testing occurred.

## Public evidence handling

Build directories, virtual environments, binaries, recordings, private Trello
snapshots, receipt images and unrelated website/vehicle projects were excluded.
Machine/user paths in published historical evidence were replaced with explicit
placeholders; trailing whitespace in historical text logs was normalized. These redacted compilation databases are records, not reusable build
commands; generate a new database for fresh JSF analysis. Private board captures
remain local. Public ORNL reports are linked rather than bundled.

Historical source hashes/results refer to their original review snapshots and may
not equal the normalized publication bytes. Fresh checks above apply to this tree;
they do not imply full JSF compliance, physical inhibit timing, ECU compatibility,
continuous power, inspection acceptance or a successful 75 mph endurance run.

## History and maintenance

The original pushed parking-pawl commit is retained. The remaining project is added
as one coherent, verified import; no force push or rewrite of published history is
needed. Further changes should use focused commits with matching tests and updated
evidence. Do not commit generated outputs or local credentials.
