# Loaded bench: fixture concept, instruments and test sequence

**Concept and measurement procedure only. No fixture, wiring or energized test
has been approved or performed by this software work.** Exact hardware models,
pinouts, ratings and lab procedures must be recorded before physical integration.

## Fixture concept

Retain the differential's normal function. Support and couple **both output
shafts** to instrumented, commercially rated absorbers. Coordinate their speed
control so both outputs turn at the same road-equivalent speed. One system manages
the common mechanical operating point; MG1/MG2 remain independently current/torque
controlled. Avoid competing motor speed loops.

Use `bench plan` to generate the speed/grade sizing table. It reuses the current
205/45R15, 4.113 reduction and road-load assumptions. The selected 70 mph/6% grade
case determines a continuous road-load requirement, not maximum motor torque.
The default 25% study margin is not a structural safety factor. Peak torque,
inertia, emergency stopping energy and actual ratings remain unknown inputs.

The concept requires supported CV/output adapters, alignment control, suitable
bearings, reviewed mounts, coupling containment, guarding and independent speed
monitoring. Do not lock one output and apply the equal-speed calculations.
Faculty/lab mechanical review must address thrust, fatigue, balance, reaction loads,
stored energy and the consequences of losing either absorber.

## Purchasing specification and absorber limits

| Item | Evidence required before selection |
| --- | --- |
| Each commercial absorber | Continuous/intermittent torque-speed-power envelope at the actual output speed; thermal duty and maximum speed |
| Low-speed holding | Rated low-speed/standstill capability or separately reviewed holding arrangement |
| Load controller | Stable equal-speed coordination, torque measurement interface and documented fault behavior |
| Torque sensing | Calibrated shaft sensor or engineered reaction load cell with verified lever arm, tare, alignment, bandwidth and uncertainty |
| Speed sensing | Independent measurement of both outputs; range and resolution appropriate to the fixture |
| Cooling | Manufacturer-required flow, temperature, heat rejection and loss-of-flow response |
| Mechanical hardware | Rated couplings/supports, drawings, materials, tolerances and reviewed containment/mounting |

Eddy-current brakes develop torque with speed and require attention to low-speed
capability; a powder/hysteresis or other suitable system may be required for the
selected duty. Use manufacturer curves rather than selecting by advertised peak
kW alone. [Magtrol's manufacturer overview](https://www.magtrol.com/wp-content/uploads/tsp_brochure_EN.pdf)
illustrates the distinct absorber families and instrumentation arrangement;
it is not a selection or endorsement of a particular model.

The generated water-flow estimate is an energy-balance study using a declared
water temperature rise, not a vendor cooling specification. Absorbers require
their own heat-rejection provision. The Civic radiator is not assumed to absorb
all mechanical test power in addition to drivetrain losses.

## Instrumentation and interfaces

| Signal | Required acquisition | Presentation evidence |
| --- | --- | --- |
| Per-motor phase currents / d-q currents | Calibrated current chain; PWM-synchronized controller capture or suitable isolated DAQ; record raw versus derived quantities | Reference/actual current, offsets, polarity, noise, saturation and bandwidth |
| Resolver excitation/sine/cosine | Suitable differential probing and the actual resolver interface; not a direct MCU analog connection | Direction, angle progression, pole relationship and sync-offset records |
| DC-link voltage and pack current | Appropriately isolated calibrated measurement; signed current and defined boundary | Input power, current limiting and regeneration |
| Individual channel DC current | Separate sensors or validated estimates; one pack sensor cannot resolve circulation | Both machines' electrical contribution and opposing-power screening |
| Output torque and both speeds | Calibrated torque plus independent speed acquisition; synchronized when computing power | MG1-only/MG2-only/combined output and mechanical power |
| Trigger, physical inhibit and current | Scope/DAQ on a known common timebase; identify the actual inhibit measurement point | Detection/action latency, uncertainty and current decay |
| Oil, winding, inverter, coolant in/out, flow | Verified sensor curves and installed-flow measurement | Temperature rise, final slope and cooling conditions |

Choose sample rates from the response being measured, not the display refresh
rate. A 100 Hz CAN log cannot resolve a millisecond current transient. Aim for at
least ten samples across the response rise; record bandwidth, filters, clock
alignment, quantization, clipping and dropped samples. Buffer fast captures locally
and transfer them after the event; avoid serial printing in control interrupts.

A modern MCU can sequence tests, acquire suitable slow signals and exchange CAN
through proper transceivers. It is not automatically a precision DAQ, resolver
emulator or independent protection system. The V1d external-supervisor CAN
modification still requires review of the exact schematic. The pictured coolant
pump's protocol/pinout also remains unverified; follow [PUMP_TEST.md](PUMP_TEST.md).

## Staged test procedure

| Gate | Work and release evidence |
| --- | --- |
| 1. De-energized | Identify all parts; review planetary lock, lubrication, fixture, HV protection, precharge/discharge, grounding/isolation and cooling; inspect guards and sensors |
| 2. Logic-only | Verify node identities, calibrated command mapping, counters, watchdog, independent inhibit, scheduler stop and no restart into torque; scope the actual inhibit |
| 3. MG2 then MG1 | Use bounded current/energy and controlled loading; verify current offsets/signs, phase correspondence, resolver direction/offset and current-loop response |
| 4. Tune one variable | Archive original parameters; apply bounded references under the lab's approved procedure; record before/after gains with actual firmware scaling; verify saturation/anti-windup and disturbance recovery |
| 5. Combined load | Verify equal signed speeds, torque signs, circulation and pack limits; collect five independent matched baseline/combined pairs plus MG1-only data |
| 6. Endurance | Hold a documented load; record temperatures/flow, final slope, environmental conditions and actual limits; stop at approved thresholds |

Fault injection starts logic-only. Loaded fault tests require an approved
energy-management and stopping procedure. Removing battery contactors does not
remove spinning permanent-magnet back-EMF or automatically make the DC link safe.
The energy source/test system must safely absorb returned energy; an ordinary
bench supply may not accept regeneration. No software analysis substitutes for
these physical provisions.

Calibrate before tuning. Keep the current loops on the OpenInverter processors;
outer speed/pack-limit loops run slower. PI starting equations in SI units cannot
be copied into unverified firmware gain fields. Do not tune by deliberately driving
the traction system into sustained oscillation. Use the selected firmware's
[FOC commissioning guide](https://openinverter.org/wiki/Using_FOC_Software), actual
hardware documentation and lab supervision. Manual FOC mode has no built-in
rotor-speed limit according to that guide.

## Final presentation standard

Show calibration and time alignment before performance claims. Present reference
and response together, repeated-run uncertainty, matched operating conditions,
physical fault timing and thermal duration. Explain that greater torque with
greater battery input is propulsion evidence, not proof of higher efficiency.
For repeated comparisons, alternate or randomize baseline/combined order where
the approved procedure allows, and re-establish voltage/temperature conditions
between trials. This helps expose heating and order drift rather than hiding it
inside a large number of samples from one run.

Use the offline report's raw-data hashes, settings index and evidence labels.
Demonstration figures may explain the workflow, but must stay labeled synthetic.
Replace them with actual recordings only through a new measured run manifest.
