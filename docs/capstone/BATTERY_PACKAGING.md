# Battery selection and packaging

**First-review design study | 28 October 2026 | Calculated examples; no donor or enclosure approved.**

## Updated mission: hills at 75 mph for twenty minutes

The current vehicle requirement is more demanding than the 24 kWh comparison
examples below. Hills are required; use the [current 3%/6% study](VEHICLE_COMPLETION.md)
and its conservative full-duration 6% screen until the course is identified.
That screen needs **17.77 kWh**, or **21.33 kWh with the study's 20% mission
allowance**, at the current assumed mass and losses. The 13.44 kWh aged-24-pack
example is not a suitable procurement baseline for that duty.

Evaluate a **40 kWh-class donor** as a candidate: 40*0.70*0.80 = 22.4 kWh in the
same simplified aging/window model. This is not verified usable energy or power.
A healthy smaller pack may also qualify; larger/heavier packaging must trigger
new mass/axle-load and road-load calculations. Retaining rear seats remains a
preference, subject to those checks. Do not apply early 48-module geometry or
voltage/BMS settings to a later donor without identification.

The cylindrical examples below are historical equal-energy comparisons, not the
new sizing result. For perspective, even the new 96s25p P28A example's 19.35 kWh
window is below the 21.33 kWh mission screen. All architectures must be resized
and quoted against the same updated load, reserve, aging and thermal conditions.
The [whole-car budget](BUDGET_EXTENSION.md) now uses a separate $3,000–$6,000 donor
allowance for the larger capacity class; it supersedes the old energy-source
allowance for current vehicle completion planning. No purchase is authorized.

## Recommendation and design priorities

Start with a **tested Leaf donor, intact modules and the original 96-series
electrical architecture**. Design a custom enclosure around those modules if the
original pack cannot fit. Prefer retaining the rear seats; investigate the removed
fuel-tank volume first. This is a provisional choice based on development effort,
not proof that Leaf is cheapest, lightest or geometrically feasible.

“Leaf” means Nissan lithium-ion modules here, not lead-acid batteries. Moving
intact modules is different from opening modules and rearranging their individual
pouch cells. Opening modules introduces cell restraint, tab joining, insulation
and thermal design work that this capstone has not scoped or validated. Retain
the module construction and required support/restraint; verify the exact donor's
handling, mounting and orientation requirements.

**Decision rule:** choose Leaf if a healthy donor meets energy/current requirements
and fits within axle-load and enclosure constraints. Reconsider cylindrical cells
or another automotive module if packaging or donor condition fails those gates.
Battery design must not consume the schedule needed to demonstrate dual-motor control.

## Where the battery can go

| Candidate | Assessment and required evidence |
| --- | --- |
| Removed fuel-tank space | First packaging candidate. Measure the actual body cavity, suspension envelope, exhaust-tunnel adjacency and load paths after tank removal. The tank's fuel capacity is not usable battery volume. A new enclosure needs its own structural mounts, protection and service access; the original fuel-tank straps are not assumed adequate. |
| Space below the retained rear-seat cushion | Preserve the original seating, belt anchors and occupant separation. Check cushion/seat structure, enclosure height, module restraint and vent routing. Do not route a pressure-relief outlet into the passenger compartment. |
| Split tank-area and forward cargo-area enclosures | Alternative when the full module set cannot fit below the seats. Keep cargo enclosure forward and low where feasible; maintain hatch/service access and a fixed protective barrier. Additional HV routing, covers and interlocks add cost and failure points. |
| Full-width original Leaf underfloor assembly | Useful dimensional reference, not assumed to fit the Civic. Requires a vehicle-specific clearance and structural study. |
| Rear-seat deletion | Last alternative to discuss only if the preferred layouts fail. It is not the baseline. |

Nissan lists its 24 kWh assembly at approximately **1,547 x 1,188 x 264 mm and
273 kg**, with 48 modules and 360 V nominal/398.4 V upper voltage. These describe
the complete donor assembly, not a Civic installation or bare-module mass.
[Nissan first-responder guide, section 2-1.1](https://owners.nissanusa.com/content/techpub/ManualsAndGuides/LEAF/2016/2016_LEAF-first-responders-guide.pdf).

Make full-size, nonconductive mock-ups using the **actual donor module dimensions**
before purchasing or fabricating the enclosure. Include terminal covers, busbar
bends, sensing leads, insulation, restraint, cooling paths, lid removal and tool
clearance. Do not place loose live modules in the car to check fit.

### Packaging acceptance worksheet

Record cavity length/width/height at several sections, ground clearance at loaded
ride height, full bump/droop clearance, departure/ramp clearance, distance to the
road and rear body structure, and proposed mounting points. Record available
space with rear seats occupied and belts/anchors functional. Module dimensions,
orientation and allocation to each enclosure remain **unknown**.

Weigh the car by axle before conversion and after major assemblies are located.
For a mass `m` positioned `x` behind the front axle over wheelbase `L`, its static
rear-axle contribution is `m*x/L`; its front contribution is `m*(1-x/L)`. Apply
this separately to removed fuel/tank/engine and added drivetrain, battery,
enclosures and occupants. Loads behind the rear axle can unload the front axle.
Check the actual vehicle's GVWR/GAWR, tires and suspension with passengers/cargo;
the current 1,300 kg road-load assumption is not an allowable weight rating.

Mount-load calculations use `F=m*a` and include vertical, longitudinal and lateral
cases, fasteners, enclosure deformation and body load paths. Acceleration criteria
and required approval depend on the installation and applicable requirements;
they have not been selected. Sheet-metal thickness alone does not establish
crashworthiness. Obtain a structural review before manufacturing mounts.

## Leaf versus a custom cell pack

| Approach | Benefit | Cost or uncertainty |
| --- | --- | --- |
| Tested used Leaf modules | Automotive module construction, fewer new cell connections, known donor topology; preserves focus on controls | Fixed module shapes, aging/imbalance, uncertain resistance and charge acceptance; custom enclosure loses original whole-pack integration |
| New 18650 cylindrical cells | Flexible module shapes and traceable new-cell specification | Thousands of cells and connections; engineered carriers, joining process, parallel fault protection, cooling and propagation control needed |
| New 21700 cylindrical cells | Larger cell energy can reduce cell count for a chosen energy target | Same pack-development tasks; actual energy/power/temperature datasheet and geometry decide suitability |
| New prismatic cells, including LFP | Fewer large cells and connections; alternative chemistry/life tradeoffs | Cell restraint, swelling allowance, mass/volume, cold-charge limits and a different series count must be designed |
| Other intact automotive modules | Potential compromise between packaging and reduced cell assembly work | Donor-specific BMS, cooling, isolation, voltage and health validation still required |

Cell format is not chemistry or a pack rating. Do not use reclaimed laptop cells,
mixed ages or a cell's advertised maximum current to establish a traction-pack limit.

### Quantitative comparison at a common energy target

The following is a **design calculation**, not measured capacity. The Leaf example
is the early 24 kWh, 48-module 2s2p/module architecture only. Later donors must be
identified separately. Nissan’s [24 kWh specification](https://www.nissan-cdn.net/content/dam/Nissan/za/brochures/LEAF.pdf) lists 192 cells. Existing study assumptions are 70% remaining capacity and
an 80% operating energy window: `24*0.70*0.80 = 13.44 kWh` available.

For an illustrative new-cell alternative, the Molicel P28A specifies 2.8 Ah,
3.6 V nominal, 4.2 V charge and 46 g typical mass. We use `Vnom*Ah` for a simple
energy estimate rather than the datasheet's integrated discharge energy.
[Manufacturer datasheet](https://www.molicel.com/wp-content/uploads/INR18650P28A-V2-80093.pdf).

| Calculated quantity | Used Leaf example | New P28A example, similar usable energy | New P28A example, similar original nameplate energy |
| --- | --- | --- | --- |
| Arrangement | Original 96s2p, 48 intact modules | 96s18p | 96s25p |
| Individual cells | 192 pouches within modules | 1,728 cylinders | 2,400 cylinders |
| Nominal/full voltage | 360 / 398.4 V | 345.6 / 403.2 V | 345.6 / 403.2 V |
| Nominal energy before aging/window | 24 kWh nameplate | 17.418 kWh estimate | 24.192 kWh estimate |
| Energy after stated assumptions | 13.44 kWh | 13.935 kWh, assuming new capacity and 80% window | 19.354 kWh, same assumption |
| Cell mass alone | Not established for selected donor | 79.5 kg | 110.4 kg |
| Complete installed mass | Unknown; original assembly reference above | Unknown: add carriers, conductors, cooling, enclosure and protection | Same limitation |

A cylinder pack may have a meaningful mass/shape advantage. Comparing its cell-only
mass with a complete Leaf pack would overstate that advantage. New-cell usable
energy also assumes delivery capacity and must be verified. Equal energy does
not establish equal life, power, temperature performance or installed cost.

At the existing model's 70 mph/6% grade case, 47.72 kW at 300 V requires about
159 A. Ideal equal sharing gives 8.84 A/cell at 18p or 6.36 A/cell at 25p.
These are allocation calculations, not allowed continuous currents. Measure
temperature, voltage sag and current distribution; derive limits from the weakest
cell/group, interconnects, cooling and operating conditions. Regen limits require
separate cold/full-pack tests. A 15-minute climb consumes about 11.93 kWh before
allowing for approach travel or reserve, so the 13.44 kWh Leaf example has little
mission margin. Specify hill duration and required range before choosing capacity.

The cylindrical example's 403.2 V full voltage also differs from Leaf's 398.4 V.
Verify inverter, charger, DC/DC, contactor and insulation compatibility including
regen/transient headroom. Preserving 96 series groups does not make cell settings
interchangeable. Stock Leaf voltage still requires verified highway field weakening;
raising pack voltage is not a substitute for respecting inverter ratings.

## Enclosure, electrical and thermal specification

- **Retention:** secure modules/cells with their required restraint and electrically
  insulated supports. Prevent terminal contact under vibration or enclosure deflection.
  Separate structural fasteners from current-carrying connections where required.
- **Occupants and environment:** fixed barrier to the cabin, protected underside,
  sealed cable entries, water/debris resistance, corrosion isolation and a designed
  pressure-relief/vent path away from occupants. A fully sealed unvented box is not
  automatically an acceptable battery enclosure. Verify water ingress, vibration,
  ground clearance and accessibility with the actual vehicle.
- **HV protection:** appropriately rated main fuse, contactors, precharge,
  service disconnect, touch-safe terminals, interlocks and insulation monitoring.
  A service disconnect does not make every module or split enclosure de-energized.
  Route/protect any inter-box HV cable and specify service isolation explicitly.
- **Monitoring:** all series groups, representative hottest/coldest locations and
  pack current; charger and supervisor use verified charge/discharge permission.
  Repackaging does not prove the OEM Leaf LBC operates unchanged. Confirm its
  sensing topology, temperature coverage, communication and failure behavior.
- **Thermal:** do not connect battery cooling to the drivetrain loop by default.
  The battery needs its own temperature limits and heat-flow analysis. Estimate
  `Q=I^2*Rpack` using measured resistance at relevant SOC/temperature, then check
  transients and heat rejection. Illustratively 159 A and an assumed 0.10 ohm
  give 2.53 kW heat; this resistance is not a donor measurement or cooling rating.
  Added enclosure insulation and lost underbody airflow change thermal behavior.

For temporary workshop storage, use the exact donor/cell manufacturer's storage
conditions and an agreed lab handling plan. Keep identified, protected terminals
and segregate damaged/suspect material for qualified assessment. Storage SOC,
temperature limits and damaged-pack disposition remain donor-specific inputs.

## Preliminary battery BOM and cost comparison

**USD engineering allowances, 28 October 2026; no supplier quotes.** Both options
require the following common functions. Actual architectures can change costs.

| Common item | Allowance |
| --- | --- |
| Custom enclosure(s), mounts and protected service access | $600–$1,600 |
| Module/cell restraint, insulation, terminal barriers | $250–$650 |
| Compatible BMS and temperature/current sensing | $500–$1,000 |
| Contactors, fuse, disconnect, precharge and interlocks | $500–$1,000 |
| HV/LV harnesses, glands and connectors | $250–$650 |
| Battery thermal provisions, design not selected | $200–$700 |
| Inspection/commissioning supplies and test allowance | $200–$600 |
| **Common subtotal** | **$2,500–$6,200** |

| Architecture addition | Leaf, 24 kWh donor example | Cylinders, 96s25p example |
| --- | --- | --- |
| Energy source | $1,000–$3,000 donor allowance | 2,400 cells at assumed $3–$5 = $7,200–$12,000 |
| Architecture-specific busbars/joining/carriers beyond common restraint | $150–$400 | $500–$1,500 |
| **Total before contingency** | **$3,650–$9,600** | **$10,200–$19,700** |
| **Total with 25% contingency, rounded up** | **$4,563–$12,000** | **$12,750–$24,625** |

These estimates are separate from the drivetrain and brake budgets. They exclude
labor, charger/DC-DC, freight/hazardous-goods fees, tax, dedicated welding/test
equipment, extensive structural work and certification. Do not add the original
whole-pack enclosure cost again when it is replaced. For the 96s18p comparison,
cell cost is $5,184–$8,640 and the same allowance structure gives $8,184–$16,340
before contingency. A smaller pack is a different mission/life tradeoff.

The cost comparison is sensitive to donor health, actual cell quotes, fabrication
and test labor. It is not evidence that every used Leaf pack is economical.
Use [the editable decision worksheet](../../configs/battery-packaging-study.json)
to replace assumptions with measured dimensions, health records and quotations.

## Next decisions and presentation justification

1. Measure the tank cavity, seat clearance, structural mounting locations and axle
   loads; make module mock-ups and draw a seat-retaining layout.
2. Obtain donor identity, history, module geometry, measured capacity, group spread,
   insulation condition and resistance/temperature evidence. Reject unsuitable donors.
3. Set range, sustained grade/duration, passengers/cargo and charge requirements.
   Verify highway motor voltage/field weakening and battery current together.
4. Compare installed mass, usable energy, cost and validation effort on the same
   mission. Review enclosure/thermal/electrical design before assembly.

**Presentation statement:** “Leaf modules are our provisional choice because they
retain an automotive module design and reduce cell assembly work, allowing the
capstone to focus on coordinated motor control. We will retain rear seating if the
fuel-tank-area/split layout passes clearance, structure and axle-load checks. A
custom cylindrical pack remains an alternative if verified packaging or donor
performance outweighs its additional fabrication and validation effort.”
