# Budget extension: complete Civic EV demonstration

**Draft for faculty review | 28 October 2026 | Not a purchase authorization.**

## Requested outcome

Deliver a functioning 1996 Civic EV capable of **75 mph for twenty minutes on an
approved closed course**, with documented thermal/endurance evidence and the
applicable North Carolina safety/registration route. Registration will be in Wake
County. The user requires hills. Until the course is known, budget against 3%/6%
uphill screens, with 6% sustained for twenty minutes as a conservative stress
case; the actual grading duty still needs faculty confirmation. Rear seating remains preferred where packaging permits.

## Why the budget needs to change

The earlier $3,000–$6,000 allowance covered drivetrain/bench development and
explicitly excluded the traction battery, major Civic installation, purchased
instruments and dyno rental. The graded whole-vehicle outcome adds battery
containment, mounts/axles, independent friction braking and assist, charging,
12 V power, body functions, chassis rehabilitation, qualified review and a venue.

Software development has produced tested supervisor and diagnostic prototypes,
but software results do not establish installed power, braking, insulation or
thermal capability. Funding should close these physical acceptance gaps.

## Whole-vehicle planning envelope

**USD engineering allowances only; no current vendor quotations.** These are
gross remaining-scope candidates before ownership/in-kind credits, not the
requested extension amount. Some items may already be available. The worksheet
must remove them or price only refurbishment/integration.

| ID | Work package | Allowance |
| --- | --- | --- |
| V01 | Traction electronics, supervisor, reviewed CAN interfaces and signal harness | $1,400–$3,000 |
| V02 | Transaxle modification, inspection, seals and lubrication work | $500–$1,200 |
| V03 | Drivetrain mounts/axles/CV hardware and dedicated fabrication subcontract | $1,500–$3,500 |
| V04 | Tested Leaf donor, 40 kWh-class allowance for hilly endurance; final capacity unselected | $3,000–$6,000 |
| V05 | Battery enclosure/mounts/restraint/insulation | $850–$2,250 |
| V06 | BMS and battery sensing | $500–$1,000 |
| V07 | HV fuse/contactors/precharge/disconnect/interlock/insulation monitoring | $700–$1,600 |
| V08 | Protected HV/LV traction wiring, glands and connectors | $400–$900 |
| V09 | Drivetrain coolant/ATF circulation and heat rejection | $400–$1,000 |
| V10 | Battery thermal provisions | $200–$700 |
| V11 | Compatible charger, charge inlet/pilot and plug-in drive interlock | $700–$1,600 |
| V12 | Traction DC/DC, 12 V buffer battery and LV distribution | $350–$850 |
| V13 | Porsche front/Integra rear/vacuum-assist PARTS, existing detailed BOM | $2,667–$4,615 |
| V14 | Chassis/steering/bearing/tyre service and alignment allowance | $800–$2,000 |
| V15 | Body electrics, visibility/defog and cabin utility work | $400–$1,000 |
| V16 | Honda/Mega/Teensy diagnostic PARTS, existing detailed BOM | $622–$1,425 |
| V17 | Engineering review, calibration, dyno/instrument hire and controlled tests | $1,200–$3,000 |
| V18 | Course access, recovery/transport and test logistics | $700–$2,000 |
| V19 | Integration/brake installation labor not already quoted under V03 | $1,500–$4,000 |
| V20 | Title/inspection/admin and insurance-related allowance | $250–$750 |
| | **Before contingency** | **$18,639–$42,390** |
| | **With 25% contingency, rounded up** | **$23,299–$52,988** |

The Porsche/Integra and Honda-electronics rows use their existing **parts totals
before contingency**. Common HV wiring/protection appears once. Dedicated
mount/axle fabrication is in V03; V19 covers other integration/brake labor.
Do not add these totals to the earlier battery/brake/drivetrain package totals.

This range is not a spending cap or a supplier-backed bid. Large structural
repairs, different wheels/master cylinder, revised battery/driveline after testing,
tax/freight, full dyno purchase or unusually costly insurance can change it.
The brake bracket's road-use acceptability must be resolved before ordering.

## Convert the envelope into an extension request

1. Inventory the owned transaxle/inverter/controller/MCUs/instruments and verify
   their exact identity, condition and required refurbishment. Apply no speculative credits.
2. Obtain itemized quotes and confirm school contributions: fabrication, qualified
   electrical/mechanical review, dyno/instruments, transport and course access.
3. Identify already-paid/committed work and the actual unspent approved funding.
   Do not ask for funds again for completed purchases.
4. Price the remaining scope and a documented risk reserve, then calculate:

`extension = remaining verified cost + risk reserve - unspent approved funds - confirmed new contributions`

**Current approved amount, spending/commitments, final deadline and requested
extension amount remain pending.** The editable source is
[vehicle-completion-budget.json](../../configs/vehicle-completion-budget.json).

## Milestone-based release of funds

| Gate | Evidence purchased/produced | Funding purpose |
| --- | --- | --- |
| A: feasibility | Loaded highway motor envelope; donor battery test; seating/axle-load layout; grading/inspection route | Prevent an incompatible battery or unworkable driveline purchase |
| B: physical integration | Reviewed mounts/enclosure; installed brakes/assist; protected HV/LV and charging; basic functions | Produce a complete, controllable vehicle |
| C: independent verification | Braking/alignment/insulation/fault/thermal results and inspection-equivalent review | Establish readiness before high-speed testing |
| D: final demonstration | Approved course, recovery plan, progressive speed tests and 20-minute records | Produce defensible final-grade evidence |

The 75 mph level model needs about **23.8 kW and 7.9 kWh for twenty minutes**;
a sustained 6% grade needs about **53.3 kW and 17.8 kWh** before mission reserve.
Actual capability remains unmeasured. Agree the duty before choosing pack capacity.
The aged 24 kWh donor example is inadequate for the conservative hill screen.
The revised energy-source allowance evaluates 40 kWh-class donors, with actual
health, sustained power, packaging and axle weights as purchase gates. This is
not a supplier quote. Downhill braking must also work when regeneration is unavailable.
A twenty-minute run alone does not prove thermal stabilization.

## Suggested faculty-facing justification

> The extension funds the transition from validated software and bench planning
> to a complete vehicle with measured performance. The graded 75 mph endurance
> requirement requires hardware that earlier estimates excluded: a qualified
> traction battery and enclosure, drivetrain installation, braking and assist,
> charging and auxiliary power, and controlled independent testing. We propose
> releasing funds against documented acceptance gates and deducting verified
> owned equipment and school contributions before establishing the request.

For the legal/engineering distinction and complete test gates, see
[VEHICLE_COMPLETION.md](VEHICLE_COMPLETION.md). The applicable NC route should be
confirmed using the vehicle's records; a simulated gasoline ECU is not the basis
for inspection eligibility. No agency, venue, insurer or professor has been
contacted or asked to approve this draft.
