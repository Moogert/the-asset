# Commissioning and evidence procedure

Record run ID, operator, date, actual hardware revisions, firmware/source hashes,
parameter hashes, instruments/calibration, fixture details and observed faults.
No step below is recorded as performed on hardware.

## A. Design and de-energized inspection

- Confirm P112 identification, stock winding/insulation condition, phase continuity,
  bearings, resolver connections, temperature sensors, lubrication and cooling.
- Complete the planetary lock design and independent mechanical review. Inspect
  dimensions, thrust clearances, free rotation, seals, oil paths and guarding.
- Complete actual-revision CAN/inhibit schematics and inspect physical modifications.
- Select HV fuse/contactors/precharge/discharge from the actual pack and DC-link data.
  Calculate capacitor energy E=0.5*C*V^2, RC time and resistor pulse dissipation.
  Validate voltage feedback and contactor auxiliary-feedback truth tables.
- Define fault-specific shutdown sequencing; a spinning PM machine can energize
  its DC bus through the inverter even after the battery contactors open.

## B. Logic-only tests

- Verify separate MCU identities, firmware and CAN mappings with HV absent.
- Verify command counters/CRC using captured frames compared to offline encoding.
- Confirm the physical watchdog inhibits both channels within 100 ms of loss of
  valid supervisor/feedback operation, including a stopped MCU scheduler.
- Test either MCU reset, brownout, bus-off, unplugged cable, unpowered transceiver,
  malformed frames, BMS withdrawal and hardware emergency stop.
- Confirm shared power-stage fault paths and no restart into torque after recovery.

## C. Separate low-energy motor commissioning

Start with MG2 only, then MG1 only. Keep the other machine electrically supervised
and consider its generated voltage while mechanically back-driven.

1. Establish a guarded/loaded fixture, external speed measurement and independent
   stop mechanism. Verify DC-link discharge before wiring/programming changes.
2. Calibrate DC voltage and both phase currents, including polarity. Determine
   correct phase/current correspondence and per-channel pinswap.
3. Verify resolver excitation and a clean angle sweep by hand. Determine electrical
   versus resolver pole-pair relationship. Do not energize based on a noisy angle.
4. Use the official FOC procedure with bounded energy/current and external speed
   control to determine syncofs for this exact motor/resolver pairing.
5. Confirm smooth controlled operation in both directions and correct torque sign.
6. Return manualid/manualiq to zero, leave manual mode, verify zero command and stop.
7. Archive actual parameter exports separately; fill the commissioning worksheets
   with measured values and their test references.

Official guide: <https://openinverter.org/wiki/Using_FOC_Software>.
Manual mode has no rotor-speed limit. fmax is not protection in manual tuning.
Never copy another unit's syncofs or Gen 2 deadtime into the Gen 3 inverter.
Stop immediately on heating, unexplained phase-current imbalance, feedback loss,
uncontrolled acceleration, abnormal noise, overvoltage or unexpected trip.

## D. Loaded combined-drive measurement

- Validate signs and equal signed motor speeds for the locked planetary configuration.
- Measure MG1-only, MG2-only and combined shaft torque over a conservative speed range.
- For the added-torque comparison hold output speed and MG2 command constant; use
  matched pack voltage and temperatures and the same calibrated torque fixture.
- Verify the shared battery limiter does not reduce MG2 command during the comparison.
  If it does, lower the operating point or use a different explicitly documented test.
- Log channel DC currents and reject sustained unexpected motor/generator circulation.
- Log at least ten steady samples per condition in `dyno-template.csv` with the same
  torque direction, positive output speed, and declared instrument uncertainty.
- Use `tools/analyze_dyno.py` on one matched baseline/combined test pair per file.
  It requires mean speeds within 1%, stable speed within 2% peak-to-peak, matching
  MG2 command within 0.1 percentage point, and positive torque/uncertainty.
- The tool compares torque increase to summed conservative instrument+variability
  bounds. A true result is a numerical comparison, not automatic approval of the
  experiment: review calibration, correlated drift, operating conditions and fixture.

MG1-only data remains separately recorded. Retain raw time-series logs; do not
discard unfavorable runs. Add voltage, winding temperatures and independent speed
to the full lab capture even though the compact comparison CSV omits them.

## E. Operating envelope and vehicle gate

Characterize commanded versus measured current, loaded torque/speed, bus voltage,
thermal rise/stabilization, coolant flow, lubrication and regeneration rejection.
Test pack charge limit zero and discharge limit zero separately. Verify rated
components absorb stored/returned energy and survive the approved fault sequence.
Test exact 100 ms stale boundary and measure physical inhibit latency with a scope.
Software regression timing is not a hardware latency measurement.

Only measured continuous cooling and loaded performance, plus completed mounts,
axles, brakes, battery packaging and parking controls, can support the updated 75 mph vehicle
claim. Record remaining limits if the result is a bench drivetrain demonstration.

The [Porsche vehicle-brake package](BRAKES.md) is a separate vehicle gate. Verify
the planned Integra rear-disc/non-ABS hydraulic configuration, suitable master and
proportioning, sound hard lines, wheel/hose clearance and independent vacuum
assist. Establish friction-only braking with regeneration unavailable before
assessing regen interaction. Passing drivetrain/dyno software tests does not
validate the service brakes or resolve the supplier's road-use restrictions.

Battery installation is a separate gate: follow [BATTERY_PACKAGING.md](BATTERY_PACKAGING.md)
for seat-retaining mock-ups, tank-cavity clearance, structure, loaded axle weights,
module restraint, protection, service isolation, monitoring and thermal validation.
Enclosure installation and cell rearrangement require their own engineering review.

The current vehicle requirement is **75 mph for twenty minutes**, with the grade
and acceptance conditions still to be agreed. Use [VEHICLE_COMPLETION.md](VEHICLE_COMPLETION.md)
for the whole-car, NC equipment and controlled-course gates. The earlier 70 mph
bench study is retained for comparison and is not the new acceptance threshold.
