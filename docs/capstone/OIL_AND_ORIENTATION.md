# Build details, magnetic orientation and transmission oil

## Build input record

Fill `configs/build-inputs.json`. Unknown values remain null. The current example
remains runnable, and the refinement tool changes a copy rather than overwriting
the baseline or claiming measurements have been made.

| Input | Best useful detail | Why it matters |
| --- | --- | --- |
| Tires | Actual sidewall size; preferably loaded rolling circumference | Shaft speed, back-EMF, gearing and speed limit |
| Radiator | Core width/height/thickness, material, rows, part number | Identifies core; dimensions alone cannot establish cooling conductance |
| Fan/shroud | Fan model/count, shroud coverage, operating direction | Low-speed airflow and recirculation; unshrouded area can bypass the core |
| Coolant pump | Model and flow measured through the installed hoses/components | Free-flow pump rating is not installed flow |
| Plumbing | Series/parallel routing, hose sizes/lengths, high points, bleed/degas and bypass | Branch flow, air locking, pressure drop and coolant temperatures |
| Oil system | Pump/drive, pickup/return, oil-to-water cooler, filter/bypass | Winding cooling and bearing/gear lubrication must both be preserved |
| Battery | Chemistry and cell count, minimum loaded/nominal/full voltage, current limits | Minimum voltage drives field weakening; maximum drives insulation/component limits |
| Duty | Level/3%/6% sustained grade, duration, ambient | Continuous heat rejection and pack demand |
| Mounting | Actual P112, pitch/roll relative to original installation | Oil pickup, slinger/splash patterns, fill level, venting and axle geometry |

For a tire code, unloaded geometric diameter is rim diameter plus two sidewalls.
185/65R14 gives 0.5961 m geometrically; this is an arithmetic example, not a tire
fitment recommendation or measured rolling diameter. The tool prefers measured
loaded circumference when available and does not invent a tire-deflection correction.

Commands:

```powershell
python tools/refine_build.py --output build-capstone/build-refinement.json
python tools/oil_cooling_analysis.py --output build-capstone/oil-analysis.json
```

The default refinement compares level, 3% and 6% grades at 70 mph and prints
illustrative pack-current requirements at 250/300/350/400 V loaded. These voltages
are comparison columns, not approved pack choices. No measured grade duration is
invented. Radiator dimensions are never converted into a fictitious UA rating.

## What orientation means

**Whole-transaxle mounting:** preserve the original installed attitude as the
starting point. There is no useful north/south compass orientation for propulsion.
The motor's own magnetic circuit determines the forces. Gravity matters to its
oil system: rotating the housing can change sump level, pickup submergence, splash
coverage, drainback and vent behavior. A different mounting attitude needs a
lubrication design and inspection under pitch/roll/acceleration. This work does
not establish an allowable tilt angle.

**Stator clocking inside the housing:** retain the OEM indexing, concentricity,
air gap, retention and thermal contact. Physically rotating a stator does not
create a continuous torque advantage; the rotor keeps turning through its field.
It may change the rotor/resolver electrical offset and disturb oil paths or lead
routing. MG1 and MG2 do not need their magnetic poles mechanically lined up with
each other to add torque. Calibrate each resolver offset independently after
final assembly; equal shaft speeds do not imply equal electrical zero positions.

**Electrical stator-field angle relative to the rotor:** this is the useful
optimization variable. In the common amplitude-invariant dq convention:

`T = 1.5*p*[lambda_PM*iq + (Ld-Lq)*id*iq]`.

The rotor magnet axis is d; q is 90 electrical degrees from it. For a simple
surface-PM machine below base speed, id near zero is a common starting point.
The Prius is an interior-PM/salient machine: reluctance torque means that a
universal 90-degree current angle is not optimal. MTPA chooses the d/q split
for the requested torque and motor parameters. With the usual Lq>Ld convention,
negative id can add useful reluctance torque. Above base speed/at a voltage
limit, field weakening usually requires more negative id and uses some of the
current capacity that would otherwise produce torque. Parameter saturation and
magnet temperature change the optimum; electrical sign conventions must be verified.

Source for MTPA/negative-id distinction:
<https://onlinedocs.microchip.com/oxy/GUID-94F279C6-1342-4E97-B4A0-AEBFA48CEF3D-en-US-3/GUID-5E1206A2-1AA7-4E09-AE74-0803155D72B5.html>.
Commissioning procedure: <https://openinverter.org/wiki/Using_FOC_Software>.
Resolver offset is a coordinate calibration, not a knob to advance arbitrarily
for more power. Do not change it to hide a voltage/current limitation.

## Oil temperature and proposed cooling architecture

Use the donor's specified fluid, with Toyota Genuine ATF WS as the P112 baseline;
do not substitute CVT belt fluid, heavy gear oil or additives to pursue viscosity.
Toyota's repair instructions warn against insufficient/excess fluid and specify
checking level with the vehicle level. A tilted installation changes that geometry.
Source: <https://toyotamanuals.gitlab.io/PZ471-Z01S0-CA/htmlweb/rm/rm1076e/m_22_0001.pdf>.

Oil lubricates gears/bearings and participates in motor heat removal. ORNL
identified direct stator-to-case and winding-to-splashed-oil cooling paths and
recommended better oil cooling because coolant and oil were weakly coupled.
Source: [saved thermal report](https://info.ornl.gov/sites/publications/Files/Pub57487.pdf), printed pages 30-37.
The high oil temperatures used as test limits in that report are not desirable
everyday targets for this conversion.

**Provisional engineering target:** investigate 70-90 C bulk oil at sustained
load; begin a commissioning derate review at 100 C and use 110 C as a conservative
planning stop threshold pending hardware review. These are deliberately proposed
project targets, not Toyota limits, proven optimum viscosity or flashable settings.
Cooler-than-70 C oil is not automatically a fault or a reason to install a heater.
Startup, bearing lubrication and winding temperature remain separate constraints.

A candidate arrangement uses the Civic radiator for a dedicated EV coolant loop,
with a properly shrouded fan and an electric coolant pump. Preserve transaxle
water-jacket flow and inverter flow, then investigate a separate ATF-compatible
oil circuit and oil-to-coolant exchanger if internal oil temperatures require it.
The oil and water circuits must remain physically separated. The radiator does
not carry ATF directly in this proposal, and the radiator's original automatic
transmission cooler, if present, is not assumed suitable without checking it.

Do not assume removing the engine or locking the gearset preserves oil-pump
drive and distribution. Trace the actual oil-pump drive, inspect oil paths and
verify lubrication at low speed, reverse and after the mechanical modification.
An external cooler pump is not automatically a replacement for lubrication feeds.
Keep OEM pickup/feeds functional unless a replacement is specifically engineered.
In the locked-together planetary arrangement the carrier rotates with the machines;
that differs from a stationary engine-input restraint. Trace which shaft actually
drives the oil pump rather than concluding that engine removal alone stops the pump.

For an added oil circuit, establish pickup submergence, return placement below
the aeration-prone surface where appropriate, temperature/viscosity-compatible
pump behavior, pressure relief, low-restriction filter/bypass, cooler pressure
rating and leak monitoring. Avoid foaming, starving OEM feeds or excessive oil
level. Cold-viscosity pressure drop must be evaluated before selecting a pump.
No drilling locations, pressure values or oil-jet changes are authorized by this model.

A thermostatic bypass can limit unnecessary cold-flow restriction, but its
setpoint follows the selected fluid/pump/cooler and measured oil response.
Do not use an engine-temperature thermostat assumption for the lower-temperature
inverter loop. Provide flow and temperature sensing and a means to detect fan/pump
failure. Series routing is easier to balance but heats downstream coolant;
parallel routing needs verified branch flows. Final routing awaits component details.

## New oil model and optimization limits

`configs/transaxle-oil-estimate.json` gives a separate bulk-oil node and two
winding nodes with both direct-to-casing and oil paths. These resistances replace,
not augment, the old lumped winding-to-coolant resistances for this calculation.

For winding i: `Qi = Gic*(Twi-Tc) + Gio*(Twi-Toil)`.
For oil: `Qgear + Qpump + sum[Gio*(Twi-Toil)] = Goil,c*(Toil-Tc)`.
The solver balances both equations. Candidate exchanger conductance is limited
by counterflow effectiveness and both fluids' capacity rates; zero external oil
flow gives zero added cooler conductance. Added pump heat reaches the radiator.

All conductances and fluid properties remain sensitivity assumptions. There is
no viscosity-versus-temperature map, oil-pressure/flow map, temperature-dependent
motor loss map or local bearing/magnet model. Consequently the tool compares
thermal outcomes; it does not claim a mathematically optimal energy temperature.
Too-cold oil can increase drag/pumping losses, while excessive oil/winding
temperatures reduce durability. Measure both electrical input and output power
across several stabilized temperatures before claiming an efficiency optimum.

## Current screening results

With the retained example tire/gearing, 1,300 kg mass, 35 C ambient, 70 mph and
nominal radiator/loss assumptions, the separate oil network predicts:

| Thermal arrangement | Bulk oil | Peak winding |
| --- | --- | --- |
| Assumed existing paths, no added cooler | 90.6 C | 113.2 C |
| Candidate exchanger at full assumed conductance | 61.4 C | 97.7 C |
| Candidate exchanger modulated to an illustrative 80 C target | 80.0 C | 107.6 C |

The controlled case uses roughly 16% of the modeled added exchanger conductance.
This is **not** 16% valve opening, duty cycle or a selected pump speed. Actual
thermostatic behavior requires flow/pressure and mixing measurements. These
temperatures are illustrative, not measurements or a calibrated prediction for
the existing P112. The older single-resistance winding result and this more
explicit oil-network result are different model assumptions, not two independent
measurements that should agree exactly.

At the same speed, the default road-load calculation gives about 20.1 kW battery
demand on level road, 33.9 kW on a 3% grade and 47.7 kW on a 6% grade. This
includes the configured 500 W accessories and nominal efficiency assumption.
It does not demonstrate that the motors can deliver those loads continuously.
Coolant, oil and winding constraints must all pass under the selected grade.

Instrument bulk sump oil, a representative hot return/case location, winding
temperatures, coolant inlet/outlet, flow and pack power. Validate sensor mounting
and electrical isolation. A cool sump does not prove every bearing or winding is
receiving oil. Sweep load and temperature in controlled steps; terminate on
lubrication, pressure/flow, temperature, vibration or electrical faults.
