# Physics estimates: Gen 2 back-EMF and Civic radiator

The user accepts physics estimates and plans to use a standard Civic radiator.
The tools now support design estimates without waiting for donor measurements.
They distinguish published proxy data, first-principles calculations and unknown
as-built parameters. No estimate becomes a controller current/voltage/temperature setting.

## Back-EMF

Published Gen 2 laboratory results provide a useful proxy: MG2 about 540 V line RMS
and 850 V line peak at 6,000 rpm; MG1 about 195 V RMS and 320 V peak at 6,420 rpm.
Use E=K*n to scale these to the target speed, with +/-10% sensitivity for donor,
temperature and coefficient assumptions (not a statistical confidence bound).

Sources:
<https://info.ornl.gov/sites/publications/Files/Pub57399.pdf> (tables 3.4, 3.5, 5.2),
<https://digital.library.unt.edu/ark:/67531/metadc899892/m2/1/high_res_d/921782.pdf>
(revised ORNL/TM-2004/185, figure 13 explicitly labels line-line voltage).
The waveforms have harmonics, so their peaks exceed sqrt(2)*total RMS. Do not
use the ideal sinusoidal screening calculation as an open-circuit peak-voltage bound.

At the example 70 mph gearing (~4,097 rpm), MG2 is approximately 369 V RMS and
580 V open-circuit line peak; MG1 is approximately 124 V RMS and 204 V peak.
At 90% voltage utilization, the ideal no-field-weakening SVPWM screening requirement
for MG2 is approximately 579 V DC. This does **not** justify applying 579 V to a
Gen 2 motor system: select component limits independently and evaluate controlled
field weakening at a compatible pack voltage. At 400 V DC the screening model
requires about 31% reduction in effective back-EMF/flux; it does not calculate a
safe negative-Id command. Inductance, saliency, saturation, magnet temperature,
current rating and loaded control stability are still needed for that command.

High-speed inhibit or loss of field weakening deserves a separate bus-energy/
overvoltage analysis because open-circuit back-EMF can exceed the battery voltage.
Normal low-speed contactor sequencing alone is not proof of a safe high-speed stop.

## Thermal model and radiator assumption

`configs/civic-radiator-estimate.json` assumes a dedicated Civic radiator loop,
working fan/shroud and 10 L/min flow. It **does not** claim a measured radiator rating.
The model sweeps 80/250/500 W/K coolant-mean-to-air conductance to represent a
wide low/nominal/high airflow sensitivity. Core size alone cannot determine UA.
Pump flow, thermostat/bypass arrangement, fan performance, shrouding, plumbing,
mounting and hot-air recirculation determine the real result.

Equations:

- Motor heat = Pshaft*(1/eta_motor-1).
- Inverter heat = Pshaft/eta_motor*(1/eta_inverter-1).
- Gear heat = Pshaft*(1-eta_gear).
- Total heat = electrical input - wheel mechanical output (energy-balance checked).
- Mean coolant = ambient + total heat/UA.
- Coolant temperature rise = total heat/(mass_flow*cp).
- Conservative local hot coolant = mean coolant + half that rise.
- Winding steady temperature = hot coolant + motor loss*R_winding_to_coolant.
- Single-node transient = Tsteady+(Tinitial-Tsteady)*exp(-t/(R*C)).

The example splits shaft output 25% MG1/75% MG2. Sweep that share for different
torque allocations. All internal thermal resistances, effective heat capacities,
efficiencies and planning temperature limits are stated assumptions. The model
assigns all motor losses to winding nodes and all drive losses to the coolant
loop. It does not resolve magnet hotspots, speed-dependent oil cooling or motor
efficiency maps. Transient coolant rises immediately to its modeled steady value,
so transient durations are screening estimates rather than a full coolant transient.

The reported thermal-only shaft ceiling is the intersection of the assumed
winding, coolant and inverter-case limits at the configured split. It is not
the machine's continuous power rating or proof that this power can be produced
at the requested speed/voltage/current. Constant-efficiency estimates are especially
inappropriate near stall or at high-current peak torque.

ORNL's thermal investigation found only about 15-21 kW continuous capability near
base speed across its tested coolant-temperature cases; these results do not
transfer directly to ~4,100 rpm or to this modified dual-motor arrangement.
They are a useful warning that internal heat transfer matters even with a large
external radiator. Source: <https://info.ornl.gov/sites/publications/Files/Pub57487.pdf>.

## Run and improve

For the default level-road 70 mph example at 35 C ambient, the computed wheel
road load is about 17.0 kW and nominal battery demand including accessories is
about 20.1 kW. These use the assumed 1,300 kg vehicle, 0.65 m2 drag area, 0.012
rolling coefficient and 500 W accessories. They are not a measured Civic road load.

| Cooling sensitivity | Total drivetrain heat at cruise | Hot coolant | MG2 steady winding | Assumed steady limits |
| --- | --- | --- | --- | --- |
| Pessimistic low airflow | 3.74 kW | 84.8 C | 192.2 C | Exceeded |
| Nominal airflow | 2.64 kW | 47.6 C | 93.3 C | Within |
| Optimistic high airflow | 1.82 kW | 40.1 C | 57.4 C | Within |

The nominal result supports feasibility under the model assumptions; the
pessimistic case demonstrates why fan/flow and internal oil cooling matter.
These cases are deliberately broad sensitivities, not statistical confidence limits.

```powershell
python tools/drivetrain_analysis.py --output build-capstone/vehicle-analysis.json
python tools/thermal_analysis.py --output build-capstone/thermal-analysis.json
```

Best next details are the actual tire size/rolling circumference, radiator core
dimensions/material/row count, fan/shroud, intended pump, cooling plumbing, likely
pack voltage range and whether the 70 mph duty includes a sustained grade. These
refine the estimates; they are not prerequisites for running the current examples.
