# Movement fun recovery — 18 October 2026

Implemented two scoped changes: enable the existing solid-surface targeting policy, and add one airborne Space boost through CharacterMovement velocity. Swing/reel equations, obstruction handling, release momentum, graph scoring and flood tuning are unchanged.

## Files and ownership

- Changed `GrappleComponent.h`: surface default, three boost designer properties, public boost/query API and transient input/charge state.
- Changed `GrappleComponent.cpp`: fresh Space detection, ordinary grounded Jump fallback, bounded aerial velocity addition, confirmed-floor recharge and existing reticle control text.
- Changed `GrappleComponentTests.cpp`: updated surface-default expectation and four boost regression tests.
- Added this handoff document.
- No Blueprint, Input Mapping Context, config, map, population art, Jam/B asset or package changes in this pass. Verification scripts, screenshots, logs, backups and a task-only patch are under `Saved/MovementFunP0/` (local evidence, not shipping content).

The initial source/hash audit covered 36 module files: only the three files above changed. All 19 inventoried Jam/B files and 18 inventoried map/config files retained their hashes. `BP_GrappleCharacter.uasset` retained its hash. All 47 inventoried existing staged/archive files retained size and write time. See `ProtectionAudit.json`. The working tree already contained other developers' uncommitted changes; they were preserved.

## Actual ownership and behavior

Before the change, the spawned `BP_GrappleCharacter` GrappleComponent had `bAllowSolidWorldSurfaces=False`, SwingAndReel, 2500 cm range and native RMB/LMB input enabled. The native surface default was false. After changing that default, the actual CityFinal PIE component reported true without resaving the BP.

Inspected the BP's IA_Jump -> Jump/StopJumping wiring and IMC_Default's Space/gamepad jump mapping. BP BeginPlay explicitly installs IMC_Grapple. This inspection does not establish every active Enhanced Input mapping at runtime. A short physical Space tap initially failed to jump during rendered verification, so fresh grounded Space now also calls the same Character::Jump (idempotent with IA_Jump), followed by StopJumping on release for that native request. No mappings/bindings were changed or consumed. JumpMaxCount remains 1, JumpZVelocity remains 420.

Grounded Space requests an ordinary jump. A separate fresh Space press while CMC is falling spends the one aerial charge. Holding Space and repeated presses cannot repeat it. A short pressed+released pulse is latched once even when IsInputKeyDown is already false. Recharge requires CMC walking with a confirmed walkable CurrentFloor; detach, reattach and checkpoint-reset calls alone do not recharge it. Fall-retry and round input locks reject boosts.

The boost adds a small horizontal impulse along view yaw while retaining existing horizontal velocity; looking downward cannot turn lift downward. It raises vertical velocity to at least the configured up speed, adds lift to an existing ascent, and bounds the resulting upward target to 1600 cm/s without reducing an already faster ascent before the total speed cap. The existing swing total-speed envelope still applies. CMC collision and the existing rope constraint remain authoritative. No camera kick or travel effect was added.

## Designer tuning / rollback

On BP_GrappleCharacter's GrappleComponent, Details -> Grapple -> Air Boost:

| Property | Default | Runtime bound |
| --- | --- | --- |
| Enable Air Boost | true | Uncheck to disable boost immediately |
| Air Boost Up Speed | 900 cm/s | 0–1200 |
| Air Boost Forward Speed | 250 cm/s | 0–600 |

The existing SwingSpeedLimit (2600 cm/s by default) caps total boost velocity, so momentum can be limited when already near the cap. At 900 upward speed, a falling character receives a noticeable recovery lift rather than another weak falling impulse. Tune with human playtesting before final packaging.

Targeting -> Allow Solid World Surfaces is now true by default. Set false to restore anchor-only targeting. Existing tagged anchors still work. Existing first-visibility-hit validation, WorldStatic/Pawn blocking requirement, size threshold, non-simulating requirement and actor/component NoGrapple veto remain intact. Water and decorations must retain appropriate collision/NoGrapple metadata; meshes are not classified by visual appearance. No geometry or collision asset was changed here.

`Saved/MovementFunP0/Before/` contains the pass-start source snapshots. `MovementFunP0-only.patch` contains only this pass's three source-file diffs; `git apply --reverse --check Saved/MovementFunP0/MovementFunP0-only.patch` passed. If a source rollback is needed, check it again against any later concurrent work before reversing it. Do not restore whole working-tree files from Git or discard other changes.

## Verification

- UE 5.7 Editor Development build succeeded; latest incremental build 11.77 seconds (`Build-final.log`). Used the project's Build.bat/UnrealBuildTool Editor workflow, with Editor closed. No target/build configuration files changed.
- Full SonHat automation: **45/45 passed**, comprising 34 success and 11 success-with-warning results; zero failed/not run (`Tests-verified/index.json`, `Tests-verified.log`). This is not a warning-free suite claim.
- Four new tests: `AirBoost.JumpInputAndOneCharge`, `AirBoost.LandingSafetyAndDisable`, `AirBoost.TetherReelReleaseCompatibility`, `AirBoost.ShortSpacePulseCannotDoubleSpend`.
- Existing native RMB/LMB input, legacy fallback, surface exclusion, obstruction, 30/60/120 Hz CMC swing, terminal, flood, recovery, population and replay tests passed. No historical-crash investigation was performed.

### Actual rendered CityFinal PIE

`CityPIE.json` reports passed=true with no errors; images `01` through `17` and `CityPIE-final.log` are evidence. This used the actual BP pawn, controller and existing Director/Manager in `/Game/Jam/A/Maps/L_SonHat_CityFinal`.

| Observed | Evidence |
| --- | --- |
| Physical desktop Space from the actual starting roof produces an ordinary jump; aerial charge remains available | `02_NormalGroundJump.png`, measured upward velocity ~404 cm/s |
| A second physical Space provides free-air lift; normal gravity restored before observation | `04_DesktopSpaceAirBoost.png`, 900 cm/s vertical; ~365 cm rise over 0.604 s; 12 subsequent public boost attempts rejected |
| Unmarked real city building wall and roof accept attachment | `06`, `11`; B_Building_01 / StaticMeshActor_1, no actor tags; normals (-1,0,0) and (0,0,1) |
| Physical Space while tethered works and retains rope length | `07`, `08`; 900 cm/s vertical; ~203 cm rise over 0.264 s; rope remains 1111.90 cm |
| Reel after boost shortens the rope; release preserves velocity | `09`, `10`; rope 1111.90 -> 1089.94 cm; instantaneous release velocity error 0 |
| Physical E still prepares first SAFE connection | `13_CityDesktopE35.png`; 0 -> 35/160, budget 3 -> 2 |
| Fall requires R and blocks boost; physical R returns to checkpoint without resetting the graph | `14`, `15`; 35/160 and budget 2 retained while time continues |
| Result locks boost; physical R starts a clean round | `16`, `17`; replay 0/160, budget 3, ~90 seconds, no tether/retry state |

A second rendered audit (`CityArtSurfacePIE.json`, passed=true) confirms B_Building_01 is inside the streamed Jam/B L_City level, rather than a newly generated test fixture. Its current mesh is Engine/BasicShapes/Cube. Wall/roof attachment succeeded at ~964/~1070 cm camera trace distance. The read-only inventory contains 60 eligible-size unmarked static mesh actors from the streamed city. This does not certify every modular wall, roof or decoration.

**Method limits:** Test helpers staged viewpoints by moving the pawn, temporarily held gravity at zero while awaiting airborne keyboard input, then restored normal gravity before measuring lift. Grapple attachment, reel and release in the rendered sequence used existing public functions, not physical held mouse buttons. Space/E/R used real desktop key input. The focused result/replay test finalized preparation early through the public manager API; it did not wait for a full natural 90-second round. Helpers did not save levels or change game defaults. Earlier incomplete runs encountered Python inspection-API/quoting errors; only the final passed reports above support the acceptance claims.

## Five-minute human acceptance / remaining limits

1. Open CityFinal and Play. Tap Space on the roof: ordinary jump. Release and tap again in air: strong lift. Hold/repeat Space: no extra boost until an actual landing.
2. Aim at an unmarked building wall/roof in range. Hold RMB, use Space once while attached, hold LMB to reel, then release RMB. Judge vertical freedom, steering and earned trajectory during continuous roof-to-roof travel.
3. Check existing tagged anchors and blocked sightlines. Water/NoGrapple decorations must remain invalid. The automated surface contract passed; every final city asset's collision still needs designer acceptance.
4. Confirm SAFE terminal E grants 35/160. Fall, wait briefly, press R: checkpoint recovery with the same round/score. After a natural timer expiry, R must replay a fresh round.
5. If the boost feels too strong, first lower Up Speed (e.g. 750–800) or uncheck Enable Air Boost. Human-approved RMB/LMB feel must remain intact.

The rendered test establishes functionality, not human approval of fun, uninterrupted mouse-button traversal, all city collision, camera comfort or frame time. System memory-pressure notifications appeared during Editor testing; no performance benchmark is claimed. No updated packaged build was made and existing packages remain untouched. Flood/map pacing remains Composer's responsibility. Stop this movement pass here and hand the defaults to DevB for continuous human playtesting.
