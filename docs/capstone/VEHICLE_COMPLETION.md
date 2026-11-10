# Vehicle completion: 75 mph for 20 minutes and NC inspection readiness

**Current acceptance target | 28 October 2026 | Wake County, North Carolina.**

The user now requires a functioning Civic capable of **75 mph for 20 minutes on
a closed course**, as well as satisfying the applicable NC safety requirements.
“20m” is interpreted as minutes. This is **25 miles at the target speed**.
Rear-seat retention remains preferred. The user confirms that the design must
assume hills. Pending a measured course profile, retain a 3% sustained comparison
and use **6% uphill for the full twenty minutes as a conservative stress screen**.
This is a provisional engineering bound, not a known Piedmont course or an agreed
grading profile. Actual grades, repeated climbs, descents and duration still need
confirmation; hills steeper than the assumed screen require recalculation.

The existing software tests and synthetic figures are useful evidence, but no
traction hardware, original ECU interface or complete vehicle has been accepted.
The earlier 70 mph studies remain historical baselines. The current scenario is
[vehicle-75mph-study.json](../../configs/vehicle-75mph-study.json).

Reproduce the conservative 6%-uphill calculation without changing the historical defaults:

```powershell
python tools/drivetrain_analysis.py --config configs/vehicle-75mph-study.json --output output/vehicle-75mph-analysis.json
```

The [grade/energy calculation record](evidence/vehicle-75mph-study.json) retains
all three study scenarios and the mission assumptions.

## Three separate acceptance gates

1. **Administrative/inspection:** correct vehicle records, applicable inspection
   route, insurance and a documented review of road equipment.
2. **Engineering:** an integrated vehicle with verified mechanics, brakes,
   controls, electrical protection, energy capacity and measured cooling.
3. **Demonstration:** a faculty/venue-approved progressive test ending in the
   agreed 75 mph endurance run, with synchronized records and repeatable results.

Passing one gate does not establish the others. A routine safety inspection is
not a certification of the custom battery structure or high-voltage installation.

## NC route: resolve early

| Issue | Current primary-source finding / action |
| --- | --- |
| Emissions | The 1996 model is outside the current 20-year emissions window. Verify the actual title/model year; Wake registration does not override that age condition. |
| Converted EV classification | Do not assume the statutory plug-in exemption automatically covers this conversion: its definition includes original-powertrain conditions. Obtain the classification/record-correction route from DMV. |
| Safety inspection age | The statute's historic-vehicle exception references a definition based on at least 30 years from manufacture. Obtain the build date and confirm applicability; model-year subtraction alone is insufficient. |
| Grade evidence if exempt | Agree with the professor on a signed inspection-equivalent report by a licensed station/mechanic. Label it accurately; do not promise a required electronic inspection transaction for an exempt vehicle. |
| Modified vehicle records | Ask DMV which corrected-title/modification documents apply, including its published MVR-5/MVR-28 process. Keep VIN, donor receipts, build photographs and component records. Do not assume every EV conversion receives a new VIN or assembled-vehicle classification. |

Sources checked 28 October 2026:
[inspection applicability](https://www.ncleg.gov/EnactedLegislation/Statutes/HTML/BySection/Chapter_20/GS_20-183.2.html),
[plug-in definition, subsection 28b](https://www.ncleg.gov/EnactedLegislation/Statutes/HTML/BySection/Chapter_20/GS_20-4.01.html),
[historic definition, subsection b(94)](https://library.ncleg.gov/EnactedLegislation/Statutes/HTML/BySection/Chapter_20/GS_20-79.4.html),
[NCDMV emissions page](https://www.ncdot.gov/dmv/title-registration/emissions-safety/Pages/emissions-inspections.aspx),
[NCDMV modified-title information](https://www.ncdot.gov/dmv/title-registration/pages/default.aspx).

Discuss the converted configuration with a licensed inspection station and the
[NC State Highway Patrol Investigative Services Unit](https://ncshp.gov/investigative-services-unit)
before purchasing final brake or structural hardware. Its published Safety/Emissions
Inspections contact is **919-757-0752**. Ask which equipment checks apply to the
engine-free car and whether a separate vehicle examination is required. This
research has not obtained a vehicle-specific determination or contacted an agency.

The relevant equipment review includes brakes/parking brake, lighting/signals,
horn, steering, wipers, tyres and mirrors, plus applicable exhaust/emissions items.
Use the current station rules for rejection criteria rather than an informal
internet checklist. [NC inspection scope](https://library.ncleg.gov/EnactedLegislation/Statutes/HTML/BySection/Chapter_20/GS_20-183.3.html).

The Honda engine emulator remains a separately graded electronics project.
Retain truthful EV fault reporting; simulated gasoline readiness is not the
administrative route for this conversion.

## The updated physics and battery requirement

Assumptions retained from the existing model: 1,300 kg, 205/45R15 geometric
diameter 0.5655 m, 4.113 reduction, CdA 0.65 m2, Crr 0.012, efficiency 0.866,
500 W accessories and no wind. Actual weight, loaded circumference, drag,
accessories and efficiency remain unverified.

| 75 mph duty | Wheel power | Battery-terminal power | Energy for 20 min | Current at 300 V loaded |
| --- | --- | --- | --- | --- |
| Level | 20.13 kW | 23.76 kW | 7.92 kWh | 79.2 A |
| Continuous 3% grade | 32.95 kW | 38.56 kW | 12.85 kWh | 128.5 A |
| Continuous 6% grade | 45.73 kW | 53.32 kW | 17.77 kWh | 177.7 A |

These are calculated demands, not demonstrated capability. Add acceleration,
approach travel, wind, reserve and battery-internal loss effects as appropriate.
An illustrative **20% mission-energy allowance** gives 9.50, 15.43 and 21.33 kWh
respectively; it is separate from the battery's cell/SOC operating window and
must be agreed for the actual mission.

Both locked-planetary machines turn approximately **4,657 rpm**. The MG2 Gen 2
back-EMF proxy gives a **659 V no-field-weakening screen**. This is not an allowed
bus voltage or a recommendation to build a 659 V pack. A stock-voltage Leaf design
needs verified field-weakening torque/current/temperature capability at the
minimum loaded voltage. Test this before finalizing mounts and battery purchase.
Alternative gearing/pack choices require review if the loaded envelope fails.

The aged 24 kWh Leaf example provides only 13.44 kWh under its assumed 70% capacity
and 80% energy window. It does not meet the hilly mission examples with the stated
allowance. Evaluate a **40 kWh-class Leaf donor** as a candidate: the same arithmetic
gives 22.4 kWh, just above the 21.33 kWh 6% screen. Nissan documents a 40 kWh pack
for the 2018 Leaf. This is a capacity-class comparison, not selection of that exact
year or a validated current rating. A healthier smaller pack can also qualify if
measured usable energy and power suffice. [Nissan specification](https://usa.nissannews.com/en-US/releases/overview-2018-nissan-leaf).

The 6% case needs about 38.1 kWh nameplate under the particular 70%/80% assumptions.
Use measured terminal energy at load and temperature instead of treating SOH as
a complete test. Check cell spread, resistance, continuous-current capability and
regen acceptance. A larger pack changes axle loads and total mass; iterate the
road-load model rather than holding 1,300 kg fixed. Later Leaf module geometry,
voltage limits, restraint and BMS architecture must be identified separately; the
early 24 kWh/48-module layout is not a packaging template for a 40 kWh donor.

Moving from 70 to 75 mph increases aerodynamic power by about **23%** and
translational braking energy by **14.8%**. At 1,300 kg, a 75-to-zero event begins
with approximately **731 kJ** before rotating masses and road losses are resolved.
Do not extrapolate acceptable braking or cooling from an unloaded motor spin.

## Remaining work packages and acceptance evidence

| Priority / work package | Work to finish | Evidence before 75 mph |
| --- | --- | --- |
| **P0: hardware and power envelope** | Identify actual transaxle, inverter/board revision, machines, pump and MCU interfaces; commission each FOC channel and measure loaded field weakening | Calibrated current/position, useful torque at 4,657 rpm, stable DC-link voltage and temperature within verified limits |
| **P0: battery mission and packaging** | Select donor by tested health; mock up intact modules in tank cavity/split enclosures while retaining seats if feasible | Usable energy at test load, current/regen limits, restraint/venting/service isolation, enclosure review and passenger/cargo clearance |
| **P0: chassis structure and axle loads** | Inspect rust/cracks; design mounts and reviewed planetary lock; preserve lubrication; verify axle length, CV plunge/angles, support bearings and full suspension travel | As-built drawings, fabrication inspection, mounting/load-path review, wheel/corner weights and GVWR/GAWR/tyre compliance with occupants |
| **P0: brakes and assist** | Complete hydraulic/bias review, independent parking function and vacuum supply; prove wheel/hose clearance | Pedal reserve/effort, repeat friction-only stops/fade/holding, assist-loss response and no rear-first instability at the test conditions |
| **P0: HV protection and charging** | Install rated fuse, contactors, precharge/discharge, service disconnect, interlocks, insulation monitoring, protected routing and compatible charger/inlet | Measured precharge/fault/discharge behavior, insulation checks, no exposed live terminals, no drive while connected to charging equipment |
| **P0: actual traction control** | Port the supervisor to its real MCU/drivers; calibrate pedal/effort; verify direction/park and zero-torque startup | Measured inhibit timing, independent watchdog, no restart into torque, brake override and park/reverse rejection while moving |
| **P1: cooling/lubrication** | Prove actual pump protocol, installed coolant flow, radiator/fan/shroud behavior and ATF circulation; evaluate battery heat separately | Temperature/flow/current logs, no leaks/cavitation/pressure issues, reviewed limits, stable hot operation and pump/fan-loss response |
| **P1: 12 V power and retained functions** | Select DC/DC and buffer battery for coincident pump/fan/vacuum/lights/wipers/defog loads; review grounding and fusing | Loaded voltage/current budget, start/brownout tests; braking warning/assist reserve and essential lights remain available after traction loss |
| **P1: suspension/steering/wheels** | Inspect joints, bearings, bushings, dampers, steering/rack and tyres; align at converted ride height | Correct tyre load/speed capability and condition, verified wheel/spoke/stud clearance, balanced wheels and no vibration/pull at each speed step |
| **P1: body and occupant equipment** | Preserve seats/belts/anchors, mirrors, horn, wipers/washers, lamps, glazing/visibility and applicable restraint systems; provide defogging | Licensed equipment review, no chafing/leaks or battery intrusion into seating/anchors; speed display calibrated from real vehicle speed |
| **P1: diagnostics and instruments** | Commission the original Honda/gateway interfaces without making essential EV functions depend on virtual engine state | Real EV warnings/codes and accurate speed/SOC/voltage/temperatures; diagnostics cannot rearm traction or conceal missing feedback |
| **P1: venue and independent review** | Obtain faculty/venue permission, suitable course, recovery arrangements and insurance confirmation; arrange EV-qualified electrical/mechanical review | Written test plan, named reviewers/driver, controlled access/runoff, abort criteria and transport plan |

**Brake purchase gate:** the public large-knuckle Porsche bracket manual says
race use only. The selected small-knuckle kit's current use classification remains
unresolved. Obtain supplier/inspector/insurer review; if it is unsuitable for the
required road-car use, obtain a supported alternative before committing funds.
The [existing brake BOM](BRAKES.md) is a planned package, not proof of road approval.
NC also requires effective braking and separate means of application; regeneration
does not establish the required friction/parking performance.
[NC brake statute](https://www.ncleg.gov/EnactedLegislation/Statutes/HTML/BySection/Chapter_20/GS_20-124.html).

The DIY parking-pawl Arduino references are not an independent vehicle safety
system. Validate real motion/park interlocks separately. If the Civic has hydraulic
power steering, engine removal requires an appropriate replacement supply or a
reviewed alternative; first identify whether assist was fitted.

## Downhill control is a separate hill requirement

Assume the battery can be full, cold, hot or fault-limited, with **zero regen
acceptance**. The friction brakes must control speed and stop the loaded vehicle
under the reviewed descent duty. Do not count on a dashboard BMS permission or a
calculated motor current to establish that the battery will absorb energy.

As one calculation, a 6% descent at 75 mph releases about **25.6 kW** of gravitational
power. With this model's aerodynamic/rolling losses, roughly **5.5 kW** remains
for braking at that speed, or **6.6 MJ over twenty minutes** without regen. This
is not a brake rating, and cannot be transferred to every speed: lower-speed
operation, tailwind, curves and repeated stops change heat/airflow and may produce
a more severe friction-brake duty. Establish reviewed brake temperatures, fade,
bias, assist reserve and stopping performance across the actual speed/load range.

A rolling loop can recover some downhill energy, but mission sizing should not
credit it until the profile, efficiency, current limits and full-pack behavior
are demonstrated. Keep a no-regen mission/fault case in the acceptance evidence.

## Test progression and thermal interpretation

1. **Freeze the grading conditions.** Agree grade, actual speed tolerance, continuous
   time, payload, start SOC, ambient/wind, interruption rules, temperature limits,
   evidence format and inspection-equivalent documentation if the car is exempt.
2. **Static/bench acceptance.** Finish structure, HV/LV, interlocks and individual/
   combined loaded drive tests. Use a dyno to establish highway torque and thermal
   headroom before relying on course speed for tuning.
3. **Low-speed vehicle acceptance.** Check direction, steering, full-lock/travel,
   braking, park, assist and shutdown at bounded low speeds. Correct all vibration,
   leaks, rubbing, loose fasteners and wiring faults before increasing speed.
4. **Progressive closed-course steps.** Use reviewed steps such as 20/35/50/60/70/75
   mph with inspections and controlled stops between gates. These are planning
   steps, not an energized-test authorization or a prescribed brake-test standard.
5. **Endurance.** Record a continuous twenty-minute target-speed segment under
   the agreed conditions. The hilly-route run needs an appropriate circuit or
   other approved venue; elevation, turning/slowing and changing loads must be
   documented. A flat dyno heat run alone does not prove course hill capability.
6. **Post-test evidence.** Repeat relevant brake/fault checks after heat soak,
   inspect mounts/CVs/fluids/wiring, record faults and retain unfavorable results.

Log independent vehicle speed, both motor speeds/currents and FOC status, pack
voltage/current/cell extrema, LV voltage, inverter/motor/ATF/coolant/battery
temperatures, coolant flow, fan/pump state and ambient conditions. Use bandwidth
and timing appropriate to each measurement; scope captures establish fast shutdown
events. An OBD-reader screen alone is not the endurance evidence.

The twenty-minute requirement proves that duration only under the measured
conditions. For thermal stabilization, reuse the offline analysis with a declared
final window, slope and span. Its existing defaults are **0.1 degree C/min and
1 degree C span**; propose a final five-minute window for review. These are study
criteria, not component ratings. Every relevant measured temperature must also
remain below its verified limit with reviewed margin. Winding/rotor hot spots may
not be observable from an OEM case/oil sensor; explain the measurement boundary.

If temperatures are still climbing, report the result as endurance without proven
thermal equilibrium. Extend or revise testing only under the approved test plan.
Failure of a limit, protection, braking, steering or containment gate stops speed
escalation; do not use the public road as the fallback test venue.

## Funding and schedule implication

The original $3,000–$6,000 allowance was for drivetrain/bench work and excluded
the battery, major Civic installation and test resources. It cannot represent this
whole-car acceptance scope. Use the [budget-extension memo](BUDGET_EXTENSION.md)
and [non-overlapping worksheet](../../configs/vehicle-completion-budget.json).

Request funding against measurable completion gates: highway loaded power,
battery/enclosure, installed chassis/brakes/HV, then independent review and course
testing. Freeze expensive packaging only after the power/energy feasibility gate.
Exact remaining budget and schedule need the owned-parts inventory, quotations,
school-provided resources, final deadline and the agreed hill condition.
