# Engineering basis and work packages

**Updated acceptance:** 75 mph for twenty minutes, Wake County/NC equipment and
registration route. See [VEHICLE_COMPLETION.md](VEHICLE_COMPLETION.md). The schedule
and $3,000–$6,000 worksheet below are the earlier drivetrain/bench scope, not a
whole-car completion budget or proof of vehicle readiness.


Current inventory update: the user reports a full drivetrain and MCU/basic
instruments available. Exact identities, revisions and calibrations are still
unconfirmed. The purchasing worksheet below is an earlier planning baseline,
not an inspected inventory. The loaded fixture remains concept-only; see
[BENCH_LAB.md](BENCH_LAB.md) and [offline tools](BENCH_TESTING.md).

The separate [vehicle brake package](BRAKES.md) now covers 986 base front
calipers/294 mm rotors, a knuckle-specific adapter, planned Integra rear discs
(currently drums, no ABS), hydraulic assessment and independent vacuum assist. Its budget is
additional to the drivetrain allowance below. No larger master or wholesale
hard-line replacement is assumed from the front caliper count alone.

## Mechanical arrangement

Identify the casting/model markings and donor hardware first. A donor described
as 2009 is not sufficient evidence of a P112 in every market.
Use stock windings, resolvers, bearings and lubrication provisions initially.
Retain normal differential operation; locking the power-split device does not
mean welding the differential.

With sun=30 and ring=78 teeth:

`30*nMG1 + 78*nMG2 = 108*nCarrier`.

Locked planetary members: all three rotate together. Restrained engine input:
`nMG1 = -2.6*nMG2`. These arrangements are not interchangeable.
At 70 mph, 0.60 m rolling diameter and 4.113 motor/wheel reduction, the locked
arrangement needs about 4,097 rpm; the restrained-input arrangement forces MG1
to about -10,652 rpm. Check both speed ratings and sign conventions independently.

Before any irreversible modification, produce an as-built drawing and review
lock strength, load cases, weld/material compatibility if used, gear alignment,
distortion, thrust clearances, cleanliness, oil paths and bearing support.
Measure runout and free rotation before/after. Document seals, covers and
engine-side guarding. Do not infer a welding procedure from these equations.

For the Civic installation, measure mounting loads/geometry, axle alignment,
CV plunge/travel, suspension clearance, cooling routing and parking-pawl state.
Retain mechanical brakes and parking brake; drivetrain validation precedes vehicle testing.

## Performance calculations

`tools/drivetrain_analysis.py` consumes `configs/capstone_vehicle.json`.
All example mass, aerodynamic, rolling, efficiency and accessory numbers are
assumptions, not measured Civic characteristics. Use measured rolling circumference
to obtain an effective rolling diameter rather than trusting tire sidewall diameter.

- Wheel rpm = speed*60/(pi*rolling_diameter).
- Motor rpm = wheel rpm*final drive.
- Aerodynamic power = 0.5*rho*CdA*speed^3.
- Rolling power = Crr*m*g*cos(atan(grade))*speed.
- Grade power = m*g*sin(atan(grade))*speed.
- Battery power = wheel road-load power/efficiency + accessory power.
- Ideal no-field-weakening DC requirement = sqrt(2)*line-line RMS back-EMF/utilization.

Back-EMF coefficients must use the stated line-line RMS convention, whether measured
or estimated from published Gen 2 proxy data. The new [physics estimate](PHYSICS_ESTIMATES.md)
uses the latter with uncertainty and a Civic-radiator sensitivity model.
This check omits load-dependent voltage drops and does not establish
available torque. Field weakening requires measured motor parameters, speed limits,
current limits and thermal testing; the tool gives it no credit.
Select pack voltage from minimum loaded voltage and motor needs; check maximum
charged voltage separately against every component and discharge/transient limit.
The DC/DC accessory converter has its own input-voltage envelope. Use a separately
verified supply initially instead of assuming a traction-bus connection is compatible.

OEM boost control, mains charging and winding redesign are outside the baseline.
Do not add the two nameplate peak powers as a prediction of combined continuous output.

## Purchasing worksheet

Amounts below are planning allowances, not supplier quotes or authorization to buy.
The provisional $3,000-$6,000 envelope excludes traction battery, major Civic work,
new lab instruments and dyno rental. Add 25% contingency after quoting the BOM.

| Item | Required information before selection | Status |
| --- | --- | --- |
| Gen 3 inverter | Part number, connector/PCB fit, working MG1/MG2 stages, sensors, capacitor condition | Not selected |
| V1d dual board | Exact revision, schematic, remaining assembly, connectors, supplied firmware | Not ordered |
| Supervisor | ZombieVerter hardware revision, spare I/O, independent watchdog/inhibit interface | Not selected |
| CAN modification | Two suitable transceiver interfaces, supplies, EMC/ESD, grounding, terminations, fail-off behavior | Design review pending |
| HV distribution | Rated fuse, service disconnect, contactors, precharge/discharge, feedback, enclosure | Pack-dependent |
| Current/voltage sensing | Isolated measurements and calibration, signed current convention | Not selected |
| Cooling and fixture | Pump/flow indication, heat rejection, guards, restrained test stand | Lab-dependent |
| Mechanical work | Planetary lock design/inspection, covers, mounts, bearings/seals as required | Review pending |
| Battery/test supply | Loaded min/max voltage, discharge/regen limits, energy, BMS and energy absorption | Unknown |

The researched V1d listing was EUR325, with most SMD parts populated and most
connectors excluded. Recheck supplier price, delivery and revision before ordering:
<https://www.evbmw.com/index.php/evbmw-webshop/toyota-built-and-tested-boards/prius-gen-3-inverter-logic-boardv1d>.

## Provisional 16-week schedule

| Weeks | Work | Gate |
| --- | --- | --- |
| 1-2 | Identify/inspect hardware, model speed/voltage, quote BOM, review schematics | Design baseline recorded |
| 3-5 | CAN hardware and supervisor port, logic-only tests | Reviewed interface, feedback and inhibit measurements |
| 6-8 | Mechanical lock/inspection; individual low-energy FOC commissioning | Separate MG2/MG1 accepted |
| 9-11 | Loaded single/combined tests and current allocation calibration | Added torque exceeds uncertainty |
| 12-14 | Speed, thermal, regen and fault characterization | Measured operating envelope |
| 15-16 | Repeat acceptance, report, archive | Evidence package complete |

Schedule begins when parts and lab resources are available. It is not a promised
completion date. Scope full vehicle installation after the drivetrain acceptance gate.

## Primary evidence

- ORNL gearset/reduction and motor evaluation:
  <https://info.ornl.gov/sites/publications/files/Pub1870.pdf>
- Toyota 2009 specifications (50 kW, 400 Nm traction motor; not conversion output):
  <https://media.toyota.ca/en/specifications/2009/2009-prius-specifications.html>
- Gen 3 board variants and historical corrections:
  <https://openinverter.org/wiki/Toyota_Prius_Gen3_Board>
- Gen 2 one-channel controller and internal deadtime:
  <https://openinverter.org/wiki/Toyota_Prius_Gen2_Inverter_Controller>
- Locked planetary versus held input correction:
  <https://openinverter.org/forum/viewtopic.php?p=86638#p86638>
- Dual board CAN limitation:
  <https://openinverter.org/forum/viewtopic.php?p=35828#p35828>
- Working-build discussion of master/slave firmware changes:
  <https://openinverter.org/forum/viewtopic.php?p=65669#p65669>

User-supplied video transcripts provide context only. The winding-fire clip is
not evidence that stock windings need redesign. Low-voltage spins do not prove
traction capability. Overcurrent trip experiments do not establish operating ratings.

## Battery integration update

The [battery packaging study](BATTERY_PACKAGING.md) prioritizes rear-seat retention
and the removed fuel-tank cavity, with a split enclosure as an alternative. It
compares intact Leaf modules with new cylindrical cells on usable energy, installed
mass limits, power, cost and validation effort. Actual dimensions, donor health,
structural mounts, cooling and axle-weight compliance remain unverified.
