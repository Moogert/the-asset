# Porsche brake conversion for the Civic EV

**Vehicle work package | 28 October 2026 | Planning, not an installed or approved system.**

The existing `car/brembo_guide.md` is preserved as an
[archived reference](references/legacy-brembo-guide.md). This
document adds the EV requirements, current kit options, hydraulics decision and
costed BOM. It supersedes conflicting assumptions in that earlier note.

## Selected direction and confirmed vehicle

Use **986 Boxster base FRONT calipers and the specified 294 mm Mini JCW rotor
package**, with the bracket version matched to the actual Honda knuckle.
The user confirms a **1996 Civic DX hatchback, stock-size front brakes, rear drums
and no ABS**. Tires are 205/45R15. Master bore, wheel model/offset and measured
knuckle/hub dimensions remain unknown.

The likely path is the supplier's small-hub **242 mm/9.5-inch** kit. Some small
Honda rotor catalogues use **240 mm** nominal diameter; do not reject or approve
fitment from the rounded kit name alone. Verify the hub register and original
rotor/application. A 262 mm knuckle swap is **not automatically necessary**:
Honed now lists separate small- and large-knuckle kits. Their observed USD prices
were $256.64 and $264.19 respectively. Select one, not both.
[Small-knuckle kit](https://honeddevelopments.com/product/honda-porsche-242-brake-caliper-mounting-kit/),
[large-knuckle kit](https://honeddevelopments.com/product/honda-porsche-brake-caliper-mounting-kit/),
[Brembo small-rotor specification](https://www.bremboparts.com/europe/en/catalogue/disc/09-5285-11?compatibility=true).

**Add a planned Integra rear-disc swap**, replacing the currently installed drums.
The rear BOM below includes parking cables, hoses and a hydraulic-matching
allowance. The front kit supplier's balance statement describes OE rear discs;
it does not by itself validate this conversion, its proportioning valve or changed
EV axle loads. The complete combination still needs a brake-system assessment.

The public **HDP23-01 large-knuckle** instructions identify race use only. Obtain
the current instructions and use classification for the selected small-knuckle
kit as well. Availability and mechanical fitment do not establish road-use approval.
Resolve the applicable restriction before treating this as the road-car solution.
[Supplier instructions](https://honeddevelopments.com/wp-content/uploads/2019/07/Porsche-Caliper-Mounting-Kit-HDP23-01-Instructions-V2.pdf).

## Master cylinder, hoses and hard lines

| Component | Project decision |
| --- | --- |
| Master cylinder | **No automatic larger-bore purchase.** Identify bore/condition and review stroke, effort, front/rear caliper displacement, proportioning and assist for the planned rear discs. Final suitability is unresolved. |
| Fixed hard lines | **No automatic upsizing for four-piston calipers.** Retain sound, compatible original lines after inspection; replace corrosion, kinks, damage or incompatible fittings/routing. |
| Flexible hoses | Budget a correct front pair. Existing hoses are reusable only if serviceable and long/articulated enough throughout steering and suspension travel. |
| Proportioning valve | Record the installed valve; budget a matched non-ABS disc-system provision. Do not assume the drum valve or any generic “40/40” valve is suitable. A larger master alone does not correct bias. |
| Booster | Retain a suitable serviceable booster and add an engine-independent vacuum supply. Booster deletion is not the baseline for this road-oriented project. |
| ABS and regeneration | No ABS is present. Bigger calipers do not add anti-lock protection. Friction braking must satisfy the complete stopping duty when regeneration is unavailable. |

Master sizing follows `pressure = pushrod force / master area` and fluid-volume
requirements. For example, increasing a hypothetical 7/8-inch master to 15/16-inch
adds 14.8% area but reduces pressure to 87.1% at the same pushrod force. It trades
travel for effort, rather than creating free braking power. Those are examples,
not identified DX bore sizes. [Wilwood hydraulic guidance](https://wilwood.com/PDF/DataSheets/ds1025.pdf).

For illustration, 36/40 mm opposed pistons give **2,274.5 mm2 on one side** and
an equivalent single sliding-caliper piston diameter of **53.81 mm** for clamp-force
comparison. Do not compare all four fixed pistons against one sliding piston and
infer twice the required master size. The donor piston sizes and the DX's actual
front/rear hydraulics still need confirmation; this is not a full displacement or
bias calculation. [Piston-kit manufacturer reference](https://racingbrake.com/bp-402p/).

If a master replacement changes ports or seating, adapt/reform the affected ends
using the correct brake-rated fittings, flare seats and tubing; that does not imply
replacing every hard line. Preserve the split circuits and sound supports/routing.
Do not substitute plumbing/compression repair fittings or assume thread sealant
fixes a mismatched flare. [Brake-line flaring guidance](https://shop.wilwood.com/blogs/news/brake-line-flaring-and-how-to-do-it).

## Engine-free brake assist

Engine removal eliminates the original manifold vacuum source. Specify a
**standalone-rated electric vacuum system** with reservoir, non-return valve,
vacuum-rated hose, demand control, pressure sensing, low-vacuum warning, fused
LV wiring and suitable mounts. Verify reserve, repeated applications, recovery,
leaks and behavior on loss of pump power. Thresholds and fuse values remain unset.

HELLA identifies UP30/UP32/UP50 variants as standalone-capable, while UP28 is an
add-on application. Do not select a cheap UP28 solely from a generic EV listing.
A UP30 pump-only reference was $330.99; it is not a complete control/assist kit.
[HELLA manufacturer information](https://www.hella.com/forvia-us/assets/documents/BI_Vacuum_Pumps_12V_2026.pdf),
[observed UP30 listing](https://www.summitracing.com/search/department/brake-systems/section/vacuum-pumps/product-line/hella-up30-electric-vacuum-pumps/part-type/vacuum-pumps-street?SortBy=Default&SortOrder=Ascending&fr=part-type&gnview=Horizontal&outofstock=true&ptr=vacuum-pumps-street&ptr_oos=1).

This is a separate pump from the BMW coolant pump. Brake assist must not depend
on the capstone PC, USB link or motor-control task continuing to run. An assist
warning/control function does not replace the hydraulic brakes or an independent
mechanical parking brake.

## BOM and projected cost

**USD planning allowances, updated 28 October 2026.** Only the kit and pump prices
above are dated observations; the ranges below are budgets, not supplier quotes.
Used calipers are assumed to be serviceable rebuild candidates. All prices exclude
shipping, tax/import charges and credit for anything already owned.

| ID | Item / purchase quantity | Projected line cost |
| --- | --- | --- |
| F1 | One correct Honed mounting kit, including its brackets/hardware/hose adapters; small-hub centering rings as supplied | $257-$265 |
| F2 | One pair used 986 **base front** calipers, casting references 986.351.421 / 986.351.422 | $350-$650 |
| F3 | Seal/boot and pad-retaining service parts for both calipers | $120-$250 |
| F4 | One matching front axle pad set, compound to be selected | $75-$150 |
| F5 | Two matching 294 mm JCW rotors, $90-$130 each | $180-$260 |
| F6 | One pair conversion-compatible front flex hoses | $100-$180 |
| F7 | Fresh compatible brake fluid for flush/bleed | $30-$60 |
| F8 | Cleaning/caps/minor consumables; included kit hardware not counted twice | $30-$60 |
| | **Front conversion parts subtotal** | **$1,142-$1,875** |
| E1 | One standalone-rated electric vacuum pump | $300-$400 |
| E2 | Reservoir, non-return valve and vacuum hose | $80-$180 |
| E3 | Sensor/switch, demand control and low-vacuum warning | $100-$200 |
| E4 | Fused LV wiring, driver/relay, connectors and mounts | $50-$100 |
| | **EV-assist parts subtotal** | **$530-$880** |
| | **Front conversion + assist, before contingency** | **$1,672-$2,755** |
| | **Parts budget with 25% contingency** | **$2,090-$3,444** |
| L1 | Installation, bleed and basic validation labor allowance | $800-$1,500 |
| | **Parts + labor, then 25% contingency** | **$3,090-$5,319** |

Rotor cross-references listed by the kit maker include **Centric 125.34092,
DBA2524 and RDA7974**. Confirm the chosen manufacturer's dimensions. A matching
Brembo JCW reference is 294 x 22 mm with 44 mm overall height; do not substitute
a generic Mini rotor or transfer its catalog torque values to Honda hardware.
[Brembo manufacturer specification](https://www.bremboparts.com/america/en/catalogue/mini-mini-r50-r53-works/000017793-1).

New genuine calipers, replacement pistons/crossover pipes, defective-booster
replacement, engineering/road approval and test-venue fees are outside these base
totals. The caliper service line is parts only; extensive professional rebuilding
needs a separate quote. No purchases have been made by this work.

| Conditional addition | Parts allowance | Trigger |
| --- | --- | --- |
| Matched replacement master | $150-$350 | Bore/stroke/condition or compatibility assessment requires it |
| Hard-line tubing/fittings | $50-$300 | Condition or changed connections/routing; extensive labor extra |
| Spacers and longer studs | $100-$250 | Actual template/mock-up requires them and chassis fitment permits |
| Replacement wheel set, excluding tires | $600-$1,200 | Existing 15-inch barrels/spokes fail clearance |
| Knuckle/hub/bearing changes | $300-$700 | Inspected brake/axle fitment requires them, not simply because fronts are small |
| Replacement caliper piston set | $250-$350 | Inspection requires it; not included in the basic seal/boot allowance |
| Rear hub/bearing pair | $150-$300 | Donor hubs fail inspection; serviceable hubs are included in R1 below |

These are alternatives/conditionals, not an instruction to add every row. The
previous generic rear-disc allowance is replaced by the detailed rear BOM below;
do not add both. Brake spending is additional to the drivetrain-only allowance.
The editable cost and unknown-input worksheet is
[porsche-brake-conversion.json](../../configs/porsche-brake-conversion.json).

## Integra rear-disc package

Plan around **1994–2001 Integra DC2/DC4 standard 4x100 disc assemblies**, excluding
Type R/5-lug parts. Prefer a matched non-ABS donor pair, identified by VIN/market
and actual components. A specialist supplier lists non-ABS DC4 assemblies for
1996–2000 Civic applications; this supports the candidate route, not approval of
every donor or the complete Porsche-front combination.
[Supplier application reference](https://typerperformance.com.au/collections/civic-ek/products/rear-disc-brake-conversion-trailing-arms-4x100-honda-civic-eg-ek-92-00-integra-dc2-94-3).

Use complete matched disc trailing-arm/spindle/carrier assemblies as the planning
baseline. Confirm arm geometry, track, hub fit, toe/camber adjustment, bushings,
shock/control-arm connections and hose brackets against this Civic. Retain only
serviceable parts. Do not combine a spindle, carrier and rotor from unidentified
variants or assume the stock drum backing plates accept disc calipers.

| ID | Rear purchase quantity / contents | USD allowance |
| --- | --- | --- |
| R1 | Left/right disc trailing-arm assemblies, spindles, backing plates, caliper carriers and serviceable 4x100 hubs; excludes R2–R4 | $350–$650 |
| R2 | One rebuilt left/right caliper pair, including working mechanical parking mechanisms | $180–$320 |
| R3 | Two matching standard non-Type-R rear rotors, $40–$70 each | $80–$140 |
| R4 | One rear axle pad set | $35–$70 |
| R5 | One correct rear flex-hose pair | $60–$100 |
| R6 | One disc parking-cable pair matched to Civic body routing and equalizer | $80–$140 |
| R7 | Trailing-arm bushings/minor suspension service parts | $80–$160 |
| R8 | Specified nuts/bolts, hose clips, washers and small mounting hardware | $50–$100 |
| R9 | Matched non-ABS disc-system proportioning provision; exact valve pending review | $80–$180 |
| | **Rear parts subtotal** | **$995–$1,860** |
| L2 | Additional rear installation, parking adjustment and alignment labor | $400–$800 |

These are engineering allowances, not current supplier quotations. A complete
donor purchase may already include calipers, rotors, hubs or cables: credit those
items after inspection instead of buying/counting duplicate parts. R1 assumes
usable hubs; replacements are conditional above. R9 reserves cost but does not
authorize an arbitrary valve or alteration of the diagonal split circuits.

The donor parts catalogue distinguishes left/right calipers, carriers, rotors,
hub components and service hardware. Use the selected donor's catalogue for
part numbers; an ABS hub catalogue entry is not a non-ABS procurement specification.
[Example Integra parts diagram](https://www.acurapartswarehouse.com/parts-list/1997-acura-integra--4dr_ls-ka4at/chassis/rear_brake_disk.html).

**Parking brake:** the original drum cables are not the procurement baseline.
Verify cable sheath length, end fittings, chassis clips, equalizer adjustment,
lever travel and full suspension travel for the disc arrangement. Donor cables
being supplied does not prove Civic routing. Preserve mechanical parking function
with traction power off; test holding and complete release without drag.

**Hydraulics:** front Porsche plus rear Integra discs require a complete volume,
pressure and bias review. A donor Integra master is not automatically compatible
with the Civic booster, pushrod, ports or circuit layout. Retain sound original
hard lines where connections/routing permit; adapt only using correct brake-rated
fittings and flare seats. Final master/valve selection follows measurements and
converted axle loads, including the proposed fuel-tank-area battery installation.

### Revised complete brake budget

| Scope | Before contingency | With 25% contingency, rounded up |
| --- | --- | --- |
| Porsche front + Integra rear + EV vacuum-assist parts | $2,667–$4,615 | **$3,334–$5,769** |
| Same parts + L1/L2 labor | $3,867–$6,915 | **$4,834–$8,644** |

These totals supersede the front-only totals for the planned complete conversion.
Conditional master, hard-line, bearing, wheel and major repair costs remain extra,
as do shipping/tax and engineering/approval/test fees. Request one consolidated
installation quote to avoid overlapping labor allowances.

## Fitment, assembly and validation gates

1. Identify the front hub/rotor and original master/valve; identify the matched Integra rear assemblies, caliper bores, cables and final hydraulic configuration. Obtain system-level compatibility guidance for the non-ABS conversion.
2. Verify full rotor/pad sweep, centering, hub support, fastener engagement and wheel clearance. The tire code establishes a 15-inch rim diameter, not spoke/barrel clearance. Use the supplier's actual-size template and inspect the intended wheel, spacer/stud stack and suspension travel.
3. Inspect/rebuild the exact base **front** calipers. Follow the selected kit's instructions for handedness, crossover-pipe/bleeder relocation and bleed order. The archived note's generic orientation/outer-first suggestions are not the procedure for every kit. Do not infer torque values from rotor catalogues or another bracket revision.
4. Verify hoses, original/replaced hard lines, split circuits, pedal reserve and hydraulic pressure/effort. Test with the electric vacuum system operating; the archived guide's engine-running assist check is inapplicable to this EV.
5. Record converted axle weights, tires, pedal effort/travel, pressure if instrumented, deceleration, stopping distance, wheel locking order, pull and temperatures. Begin under a reviewed controlled-site procedure and increase duty progressively.
6. Establish full friction stopping and fade performance with regeneration disabled/unavailable, including a full/charge-limited pack and controller loss. Then assess regen/friction interaction. No ABS or validated anti-lock regen function is added by this conversion.

At the provisional 1,300 kg mass, a 70-to-zero stop sheds **636.5 kJ** of
translational kinetic energy before rotational energy and road losses are resolved.
This is a demand calculation, not a temperature or fade rating. Battery placement
changes axle load/weight transfer; larger discs/calipers do not guarantee shorter
tire-limited stops. Measure the complete vehicle's performance before road release.

The dyno absorbers described in [BENCH_LAB.md](BENCH_LAB.md) are separate from the
vehicle service brakes. The offline torque/tuning report does not certify braking.
