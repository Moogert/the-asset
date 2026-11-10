# Confirmed build inputs, BMW pump and Leaf pack screening

For practical confirmation steps, use the short [pump test guide](PUMP_TEST.md).

## User-supplied inputs

- Tires: 205/45R15; actual loaded rolling circumference not measured.
- Aluminum Civic radiator, standard Civic fan; core dimensions/rows/shroud unknown.
- Shared inverter/transaxle coolant loop selected unless testing identifies a reason to split it.
- Pump listing: replacement for BMW 11517597715, pictured with four-pin connector.
- Battery: likely used Leaf donor; capacity, year, health and final arrangement undecided.
- Sustained hills required; grade and duration remain unspecified.

These are recorded in `configs/build-inputs.json`. The default vehicle model now
uses the tire's 0.5655 m unloaded geometric diameter. Earlier documents' 0.60 m
examples remain historical comparison cases, not the updated tire calculation.

## Pump identification and rating confidence

The manufacturer's catalog PI 1833 maps **BMW 11 51 7 597 715 to Pierburg
7.07223.01.0, type CWA200**. This is the strongest part-specific evidence:
<https://www.ms-motorservice.com/MediaAssets/elektrische-wasserpumpen-cwa-gesamtuebersicht-aftermarket_1294781.pdf>.
Its type nomenclature identifies electrical input-power class, so use roughly
200 W as a preliminary pump electrical-budget class, not an aftermarket-unit
measurement. At 13.8 V, 200 W corresponds to about 14.5 A; starting/fault currents,
actual voltage and fuse/wire requirements must come from the selected unit.

A general BMW N20 training reference lists a 400 W pump. Do not apply that
general engine description over the exact CWA200 cross-reference. Do not use
CWA400 pinout, PWM frequency or hydraulic curves for this pump merely because
another pump looks similar or its part number starts with 7.07223.

The screenshot establishes the seller's claimed application, not actual maker,
controller revision or electrical/hydraulic performance. The physical pump label,
supplier datasheet and connector cavity numbering are needed before a wiring
diagram can be issued. An aftermarket replacement need not reproduce every
standalone-control behavior of a genuine Pierburg example.

CWA-family integration is version-dependent. Controller manufacturer documentation
distinguishes CWA200 and CWA400 harnesses, while current ECU documentation notes
additional interface revisions. Sources:
<https://www.tecomotive.com/download/manual_tinyCWA_V3.5a.pdf>,
<https://maxxecu.com/webhelp/output_functions-electric_waterpump_control_pwm.html>.
Use a controller confirmed for the exact pump version, or develop the appropriate
signal interface after verifying whether that unit accepts PWM, BSD or LIN.
No frequency, duty cycle, pin assignment or unattended-run fallback is assumed here.

### Review of the subsequently supplied pinout

The user supplied a four-wire description with pin 1 ignition, pin 2 BSD,
pin 3 main power, pin 4 ground, a 1200-baud BSD/LIN description, 40-50 A fuse,
and full-speed operation without communication. This is recorded as a proposed
interface, not a verified measurement or exact-part datasheet.

MaxxECU documents pin 1 ignition, pin 2 BSD, pin 3 fused main power and pin 4
ground, plus no-BSD full-speed fallback, specifically under **E9x N54 from 9/2007**.
Its listed compatible pump numbers include VDO units and BMW 11519455978 and
11517632426; the page does not establish that interface for 11517597715.
Source: <https://www.maxxecu.com/webhelp/settings-inputs-bsd_protocol.html>.

Tecomotive's CWA200 instructions instead identify **pins 1 and 4 as the main
current paths**, with a 20 A fuse in its referenced kit. That directly conflicts
with treating pin 3 as the main supply in the proposed BSD wiring. Source:
<https://www.tecomotive.com/download/manual_tinyCWA_V3.5a.pdf>, printed page 3.
Neither generic family document alone approves the aftermarket unit's pinout.

BSD and LIN are different protocols; sharing a single-wire electrical concept
does not make message framing, baud rate, addressing, status or transceivers
interchangeable. Do not assume 1200 baud or select a generic LIN/UART interface
for BSD without its actual protocol definition. TinyCWA, TinyCLR and a raw
microcontroller board are also not interchangeable pump-controller products.

The diagram's left-to-right numbers require the latch orientation and the actual
molded cavity numbers; a wire-side view can mirror a pump-side view. No live
pinout or fuse rating is released from this schematic sketch. Confirm the exact
supplier/maker diagram or measured matching harness first, then perform a wet,
protected bench test of current, command response, flow, command loss and restart.
Do not substitute assumed no-signal emergency operation for monitored cooling.

## Shared coolant-loop decision

Keep a shared, lower-temperature EV coolant loop as the baseline. Both inverter
and transaxle need heat rejection, and one radiator/fan/pump can serve them if
flow, pressure drop and temperatures are acceptable. Splitting the loop is justified
if one component needs a conflicting temperature, branch flow cannot be maintained,
or an oil-cooler branch heats inverter supply beyond its envelope.

Suggested first plumbing study: radiator cold outlet -> low-mounted pump -> inverter
and transaxle cooling circuit -> radiator hot return, with a properly placed
expansion/degas path. Final component order and series/parallel branches follow
port sizes, allowed pressure drop and measured temperatures. Pump suction must
remain flooded; the pump is a coolant pump, not the ATF lubrication pump.

This family is much larger than the analysis's assumed 10 L/min coolant flow.
Do not assume its free-flow capacity reaches the Prius passages, and do not run
maximum speed merely because it is available. Measure installed flow and inlet/
outlet temperatures, then command only enough speed to satisfy both components.
At 10 L/min and the model's fluid properties, 4 kW coolant heat gives about 6.35 C
temperature rise. Fan/shroud/core performance still determines rejection to air.

Provide a dedicated protected power circuit and a compatible low-current command
interface. Do not power the motor from an MCU output or apply generic supply PWM
to an internally controlled pump. Establish wet startup/bleed, command loss,
stalled/blocked flow, brownout, fan loss and after-run behavior in a fixture first.
The existing 500 W total accessory assumption is still provisional: separately
budget pump, fan and other loads so that full-power operation is not hidden.

## Tires and voltage implications

205/45R15 geometric diameter = 0.381 + 2*0.205*0.45 = **0.5655 m**.
At 70 mph with 4.113 reduction and locked planetary members, both machines turn
about **4,347 rpm**. Using the published Gen 2 back-EMF proxy, the MG2 no-field-
weakening screening requirement is about **615 V DC** with the assumed 90%
utilization. This is a screening result, not permission to exceed motor/inverter
voltage ratings. A stock-voltage Leaf configuration still requires field weakening
at this operating point. The model estimates about 41% effective flux reduction
at 360 V; it does not calculate a safe negative-Id command.

## Leaf architecture preference

See [battery packaging and cell-format comparison](BATTERY_PACKAGING.md) for the
seat-retaining fuel-tank-area layout study, enclosure requirements and battery BOM.


**Retain the donor's original 96-series cell-group arrangement as the starting
point**, preserving series monitoring topology where possible. Choose the donor's
capacity and condition separately. Repackaging physical modules while preserving
the electrical layout still requires enclosure, compression/restraint, cooling,
insulation, interlocks, contactors, service disconnect and crash-load engineering.

Nissan lists a 360 V nominal pack with a 240-398.4 V usable range in its 2016
first-responder guide. That broad usable range is not a proposed operating cutoff
for this conversion. Its worst-case loaded design voltage remains unknown.
Source: <https://owners.nissanusa.com/content/techpub/ManualsAndGuides/LEAF/2016/2016_LEAF-first-responders-guide.pdf>.
Nissan's early 24 kWh data identifies 48 modules and 192 cells. Later 30/40/62 kWh
module structures differ; do not transplant the early module arithmetic onto them.
Source: <https://faq2.nissan.co.jp/faq/show/6447?site_domain=default>.

For the early 24 kWh, four-cell 2s2p/module pack only:

| Arrangement | Modules used | Nominal/full voltage | Example current at 70 mph on 6% grade |
| --- | --- | --- | --- |
| 96s2p original | 48 | 360/398.4 V | 159 A at assumed 300 V loaded |
| 72s2p subset | 36 | 270/298.8 V | 212 A at proportionally assumed 225 V loaded |
| 48s4p rearrangement | 48 | 180/199.2 V | 318 A at proportionally assumed 150 V loaded |

These currents follow the same assumed ~47.7 kW road-load demand, not verified
pack/motor limits. Reducing voltage increases inverter/DC cabling current and
field-weakening demand. The 48s4p arrangement keeps all cells/energy but changes
BMS topology and raises bus current; it is not an integration simplification.
Changing parallel count also changes pack resistance, so bus-current ratios alone
must not be interpreted as battery-cell heating ratios.

For an example 24 kWh donor with 70% remaining capacity and an 80% operating
energy window, available energy is 13.44 kWh. That gives roughly 47 energy-only
miles of level-road 70 mph cruise under the model; a 15-minute 6% climb needs
about 11.9 kWh. This is why donor health and hill duration matter as much as
series count. No range, sag or continuous-current rating is claimed for a donor
that has not been identified and tested.

A healthier/higher-capacity compatible Leaf donor may be preferable to a very
degraded bargain pack, but packaging mass, actual capacity, cell resistance,
balance and thermal performance decide that choice. Higher kWh does not inherently
mean higher pack voltage or eliminate field weakening. Do not raise series count
beyond the donor layout or rely on OEM peak traction power as a continuous rating.

`tools/leaf_pack_analysis.py` creates this reproducible comparison. It leaves
battery heat unknown because measured resistance and cooling are not supplied.
The existing Leaf LBC simulator profile remains synthetic; it is not an operating
BMS or a proven adapter for a repackaged donor.

Next highest-value details: actual pump label/datasheet and connector numbering;
Leaf donor year/capacity/SOH or capacity and resistance measurements; sustained
grade and duration; radiator dimensions and hose/port sizes. No new part purchase
or physical modification has been made by this analysis.
