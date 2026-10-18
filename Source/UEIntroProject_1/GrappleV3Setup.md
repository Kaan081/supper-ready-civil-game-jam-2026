# Grapple V3: swing + independent reel

Historical pre-P0 behavior is described below. The current assisted takeoff, course, telemetry and exact rollback are documented in `GrappleP0Recovery.md`.

V3 defaults to `SwingAndReel`; `LegacyPull` retains the pre-V3 pull, camera release boost and landing assist. No assets, input contexts, maps, project configuration or evacuation code were edited.

## Use the existing level

Open `/Game/Jam/A/Maps/L_RooftopTest`, press Play (Selected Viewport), and click the viewport. Its existing generator configures `BP_JamGameMode`, whose pawn is `BP_GrappleCharacter` with this component. For another level, use that existing GameMode/pawn. No new actor or Blueprint wiring is required.

The read-only asset audit found `IA_Grapple` Started -> TryStartGrapple, Completed/Canceled -> StopGrapple, but `IMC_Grapple` currently maps the action to **None**. LMB was unused in the inspected project contexts and active character/controller graphs. Native input therefore supplies RMB/LMB without editing assets. Both native input readers and their keys are configurable. Existing Blueprint calls remain valid and repeated attach calls do not reset the rope.

- Hold RMB over a green target: attach. Release RMB: detach.
- Hold LMB while attached: reel. Release LMB: keep swinging at the current rope length.
- WASD: existing world-space movement input supplies tangent steering; jump and mouse look stay unchanged.
- Existing E selects evacuation links, F ends preparation, R replays from results. These controls were not modified.
- Reticle: gray no hit; orange blocked/untagged surface; green valid anchor; cyan attached; orange with small chevron while reeling. Only an active tether is drawn. No world-space targeting sphere.

Anchors still require the **actor** tag `GrappleAnchor`, Visibility-blocking collision, clear camera line of sight and <=2500cm range. The existing generator preserves this tag while adding its own tags and raises anchors 140cm above walk surfaces. It was inspected, not rerun or edited.

## Physics

For inward unit direction D, split velocity into `dot(V,D)*D` and its tangent. RMB captures the current capsule-center-to-impact distance plus slack. Slack permits free motion; near full extension, bounded inward tension anticipates the next movement step while preserving tangent velocity. There is no unconditional RMB pull. CMC integrates gravity and collision as usual. WASD adds bounded acceleration projected onto the tangent plane; its input is already world-space and is not rotated twice.

LMB ramps a winch and shortens the radius, requesting inward motion without replacing the tangent. Available speed favors tangent motion over the winch; the winch slows when its radial budget is exhausted. Reeling never brakes already faster inward motion. Rope retraction stops at minimum length, but keeping the rope attached does not impose a minimum distance to the anchor.

After CMC movement, a swept, bounded correction recovers excess extension and removes outward radial velocity at the taut boundary. A blocked correction, obstructed rope, destroyed/teleported anchor or extreme stretch releases safely. Landing detaches without a boost. Air control and falling friction/braking are saved, disabled while V3 owns steering, then restored on every detach. CMC walking, gravity, wall sliding and floor detection remain responsible for character motion.

Release copies actual velocity unchanged by default. Optional assist follows earned velocity, never the camera. Legacy mode still uses its original release/landing settings.

## Defaults (cm, seconds)

All gameplay tuning is on the existing Grapple component in Details.

| Setting | Default |
|---|---:|
| Movement Style | SwingAndReel |
| Native attach / reel keys | RMB / LMB; both enabled |
| Grapple Range | 2500 cm |
| Minimum Swing Rope Length | 350 cm |
| Initial Rope Slack | 10 cm |
| Swing Speed Limit | 2600 cm/s |
| Swing Steering Acceleration | 1400 cm/s² |
| Reel Speed | 650 cm/s |
| Reel Acceleration | 2400 cm/s² |
| Rope Constraint Acceleration | 12000 cm/s² |
| Rope Recovery Speed | 1400 cm/s |
| Rope Position Correction Speed | 1000 cm/s |
| Max Rope Correction Per Frame | 50 cm |
| Maximum Rope Stretch | 500 cm |
| Break When Rope Obstructed / Detach On Landing | true / true |
| Swing Release Assist | 0 cm/s |

Control/retraction time steps cap at 0.05s during hitches; CMC retains its own collision substeps. Tension/correction tolerate 5cm near the boundary. Anchor jumps over 600cm between updates release. For gentler reeling start by reducing Reel Speed, not the speed cap. Increase steering cautiously for easier starts from rest. Legacy settings under Movement, Release and Landing Assist affect only LegacyPull.

## Verification completed

- Pre-change snapshot at branch `integration/son-hat`, HEAD `4e47b89d03b03a18276cfe4caf55b426f7391a05`.
- Last committed playable V1 identified as `d39aedc` (`feat: implement first playable grapple`); current uncommitted feel-pass changes were preserved separately. Acquisition/release baseline passed before V3.
- UE5.7 Development Editor build succeeded. `Saved/Logs/GrappleV3Build.log`.
- Final `Automation RunTests SonHat`: 20 passed, 0 failed. Six grapple tests pass without warnings; five existing evacuation Director tests carry headless "No game viewport was found" warnings. `Saved/Automation/GrappleV3Final/index.json`.
- Coverage: tagged acquisition and obstruction; raw RMB/LMB without mappings; decomposition, slack, tension, reel/tangent preservation, bounded steering/speed, zero distance/time, hitch retraction, exact release, CMC setting restoration, swept wall collision, landing, anchor loss and legacy pull.
- Actual CharacterMovement stepping at 30/60/120Hz over three seconds with swing/reel: ~44-45m traveled, lowest Z ~-6m, speeds below 2020cm/s, bounded rope extension and exact release continuity. This tests native movement, not human traversal feel.
- **Manual PIE, three rooftop transitions, rendered reticle/tether and packaged gameplay have not been verified.** The Editor was not terminated. No cook/package was run.

## Exact three-minute manual PIE check

Start with the already existing L_RooftopTest and its grapple pawn. Do not regenerate the map for this check. Start a stopwatch when pressing Play; click the viewport.

1. **0:00-0:30:** Aim at the nearby raised anchor. Verify green reticle, hold RMB, jump from the edge and hold A/D. Leave LMB up. Expect gravity and an arc around the anchor, not steady tractor pull; cyan reticle/tether only while attached.
2. **0:30-1:00:** While moving laterally, hold LMB for 1-2 seconds. Expect a shorter rope and inward/upward travel while retaining lateral movement. Release LMB and keep RMB held; radius should stop retracting. Release RMB during the arc; expect the earned trajectory even when looking sideways. No camera-facing kick.
3. **1:00-2:00:** Reacquire a clear green anchor and chain three releases/attachments across rooftops (candidate route A -> Hub -> C -> D; choose anchors within range and clear sight). Keep WASD active and pulse LMB as needed. Count three transitions; check speed remains controllable. If the round has reached results, press R once to restart, then continue.
4. **2:00-2:30:** Approach a rooftop and land. Expect normal CMC floor contact and automatic detach, with no repeated upward landing boost. Walk to an evacuation terminal, read its existing risk label, aim/approach and press E. Confirm its existing budget/HUD changes.
5. **2:30-3:00:** Press F if preparation is active, wait through the existing reveal, and press R from results. Confirm replay, movement and grapple acquisition still work. Esc stops PIE. Record any failed transition, snag or reticle issue before tuning.

Passing compilation/automation does not certify these manual acceptance checks. If traversal is worse, use LegacyPull immediately.

## Rollback

Fast designer fallback: on BP_GrappleCharacter's Grapple component, set **Movement Style = LegacyPull**. The original pull, release boost (420 default) and landing assist remain intact; the restrained reticle and native input remain. Switch while detached, or V3 will safely detach on the next tick. This setting change is for you to make in the Editor; no Blueprint asset was changed by this task.

Exact source rollback preserves your pre-existing uncommitted feel pass. With Editor/Live Coding closed, from project root:

```powershell
git apply --check Saved/GrappleV3Rollback_4e47b89_20261017/V3-only.rollback.patch
git apply Saved/GrappleV3Rollback_4e47b89_20261017/V3-only.rollback.patch
```

Then rebuild Editor. The patch restores the two exact pre-V3 component contents and removes only the four new V3 source files plus this guide. It does not change maps or other developers' work. If later grapple edits make the check fail, do not force it: the original exact component files are also backed up in that same directory. `BeforeV3.uncommitted.patch` records the earlier feel pass against HEAD, and the `V1_d39aedc_*` files preserve the committed V1 for comparison. Do not use a Git reset for this rollback.

## Remaining limits

Single local player; no network prediction/replication or rope wrapping. Rope obstruction releases instead of wrapping around roofs, so use the raised anchors with clear sight. Very large hitches or moving-anchor teleports may detach safely. This is an acceleration/constraint hybrid around CharacterMovement, not a full inextensible rope simulation. The existing map, collision placement and player physics settings still need the manual three-minute test. The native screen reticle uses UMG/Slate dependencies already present in the project; rendering/cook compatibility is implemented but not package-tested.
