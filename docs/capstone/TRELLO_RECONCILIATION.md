# Trello reconciliation — 5 November 2026

> Historical record. See the [10 November refresh](TRELLO_REDESIGN.md) for the
> current MG2-only MVP, readable titles, labels and tools checklist.

[Project board](https://trello.com/b/K1aXCHuO/)

The inherited project controlled one machine. Its notes remain historical reference;
the current scope independently controls MG1 and MG2, demonstrates combined loaded
torque, and targets 75 mph for 20 minutes on a hilly closed course.

## Changes made

- Reviewed the 93 inherited cards; updated 52 technical/project descriptions.
- Added 10 cards: four software-evidence summaries and six integration/review tasks.
- Added eight acceptance checklists with 28 initially unchecked items.
- Reopened battery selection and restored essential mechanics, cooling, brakes,
  instrumentation, inspection and testing tasks from the deferred backlog.
- Deferred paint, powder coating, ceramic coating and trailer-hitch work.
- Preserved inherited comments, attachments and procurement checkmarks. Personal
  cards were not edited. Due dates remain unset at the user's request.

Updated technical cards state status, next actions, acceptance evidence and
dependencies. Inherited descriptions appear beneath a historical-reference heading;
their specifications and suggested procedures are not accepted design requirements.

## Workflow

| List | Cards | Meaning |
|---|---:|---|
| Next actions | 24 | Decisions, identification and prerequisites |
| In progress | 2 | Supervisor integration and vehicle risk review |
| Planned / dependencies | 42 | Required work awaiting inputs or preceding gates |
| Inherited complete — verify | 5 | Previous completion claims requiring inventory confirmation |
| Deferred / legacy ideas | 26 | Alternatives and optional work outside the immediate baseline |
| Software verified / hardware pending | 4 | Existing software/calculation evidence with physical limitations |
| **Total** | **103** | **No physical acceptance implied by software results** |

## Immediate priorities

1. Photograph hardware labels and board revisions, including full Honda ECU numbers.
2. Verify wheel dimensions and Porsche front/Integra rear brake compatibility.
3. Resolve Leaf pack health, loaded voltage, mission energy and packaging while
   retaining rear seats where feasible.
4. Review planetary modification, lubrication, mounts and the two-output fixture.
5. Implement and measure target CAN, inhibition, HV sequencing and fault behavior.
6. Confirm inspection/course requirements, reviewers and staged vehicle acceptance.

## New integration tasks

- [HV protection and energy sequencing](https://trello.com/c/PRjqSYUn)
- [Budget extension and ownership credits](https://trello.com/c/VKZIbDT0)
- [Manual JSF review](https://trello.com/c/f9OxKWit)
- [DC/DC and low-voltage load validation](https://trello.com/c/mYRpxIel)
- [Course and endurance logistics](https://trello.com/c/nBu7WM41)
- [Bluetooth diagnostic transport](https://trello.com/c/3LGXxFp6)

## Evidence and open inputs

Planning follows the repository requirements, controller release, Honda installation,
bench-testing, vehicle-completion, battery, brake and budget documents. Existing
software results were summarized; no tests were rerun during this board update.

Physical measurements, actual ECU fault mappings, brake acceptance, continuous
thermal capability and the final drive remain pending. Exact ECU suffix, hardware
revisions, donor battery, wheel specifications, course profile, approved budget,
spending and final dates still require confirmation. Budget figures are engineering
allowances before ownership credits, not quotations or a finalized funding request.

The Honda ECU retains OBD/MIL ownership. Detailed EV records use Teensy USB and,
after hardware selection, Bluetooth. Honda detection delay is outside the traction
shutdown timing budget.

Board screenshot (local-only evidence) and
machine-readable change record (local-only evidence)
capture this reconciliation. The JSON details changes made after browser-session
recovery; earlier priority/software changes are summarized here and on the board.

For future updates, mark completion only against linked evidence, keep software and
physical results separate, and revise current actions above the inherited notes.
