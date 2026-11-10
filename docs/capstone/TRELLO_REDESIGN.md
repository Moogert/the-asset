# Trello refresh — 10 November 2026

[Project board](https://trello.com/b/K1aXCHuO/)

**Version one:** owned 2009 Prius inverter/transaxle, MG2 propulsion only.
Acceptance remains **75 mph for 20 minutes with hills**, subject to measured
power, energy, braking, thermal and fault evidence. First movement is an
intermediate milestone. Independent MG1 propulsion is the later upgrade.

## Board changes

- 110 cards: the previous 103 plus six MVP cards and one tools card.
- Every title has one unique leading emoji and a short name; priority/software
  prefixes have been removed.
- Technical descriptions use Goal, Next steps, Done when and Dependencies.
  Inherited notes and reference links remain beneath historical headings.
- Personal-card descriptions, existing checklists, comments, attachments,
  completion history, workflow lists and due dates were left unchanged.
- Eleven new checklists contain 35 unchecked acceptance/equipment items.

## Labels and filtering

| Label | Cards | Meaning |
|---|---:|---|
| P0 | 37 | Critical prerequisites |
| P1 | 43 | Required follow-on work |
| P2 | 28 | Optional enhancements |
| MVP · MG2 | 6 | Tasks specific to version one |
| Upgrade · Dual MG | 4 | Additional independent MG1/MG2 controls and evidence |
| Shared foundation | 59 | Common vehicle, electrical, mechanical and review work |

Labels overlap. Personal cards have no new labels. Existing unnamed labels were
not deleted. Use **MVP · MG2** for the six-card path; include **Shared foundation**
with “Any match” to see common dependencies. Existing priorities were preserved;
the seven new cards received P0, except the P1 tools card.

## MVP path

| Card | Acceptance focus |
|---|---|
| [MG2 MVP](https://trello.com/c/hlYPSBRG) | Overview and shared dependencies |
| [Gen 2 controls](https://trello.com/c/Em6AVHMi) | Actual controller, interfaces, firmware and one-channel permissions |
| [MG1 safe state](https://trello.com/c/siReijA5) | Back-drive speed, generated voltage, inactive stage, lubrication and stop energy |
| [MG2 commissioning](https://trello.com/c/JMLfiRBF) | Current/resolver calibration and physical fault inhibition |
| [MG2 load test](https://trello.com/c/YZqqhSZo) | Torque-speed, loaded voltage, current and thermal evidence |
| [MG2 endurance](https://trello.com/c/2JxDYXz1) | Progressive vehicle release and reviewed hill mission |

Gen 3/V1d hardware, dual-channel dispatch and combined-torque results are not
version-one prerequisites. The existing dual-MG supervisor is host-tested upgrade
work, not a verified Gen 2 hardware driver. Exact controller identity and safe
behavior of the uncommanded MG1 remain gates, not assumed capabilities.

## Tools and evidence

[Tools & lab gear](https://trello.com/c/CNCl5bQS) groups mechanical,
harness, electrical, controls and performance equipment. It links procurement and
funding work, specifies an item-record format, and leaves ownership, ratings,
calibration and buy/borrow/lab access unverified until recorded. See the editable
[equipment schedule](TOOLS_AND_LAB.md).

Verification found 110 unique title emojis, no priority/software prefixes and no
missing intended labels. Live filtering returned six MVP cards and 37 P0 cards.
Thirteen representative descriptions rendered all four required sections; the
MVP cards link to shared dependencies without requiring dual-only hardware.
No physical tests were performed or marked complete.

- Board screenshot (local-only evidence)
- MVP description screenshot (local-only evidence)
- Tools screenshot (local-only evidence)
- Structured results (local-only evidence)
- Before/after journal (local-only evidence)

The journal preserves descriptions captured before edits. The previous
[5 November reconciliation](TRELLO_RECONCILIATION.md) remains a historical record.
