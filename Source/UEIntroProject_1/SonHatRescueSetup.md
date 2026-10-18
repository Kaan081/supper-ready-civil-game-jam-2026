# Four-beacon rescue — Source handoff

Source verification completed 18 October 2026, 15:34 TRT. **Editor/build/cook ownership is released to Composer.** No active Unreal jobs were present at the release check. See `Saved/RescueCandidate/UnrealOwnershipRelease.json`. No more Unreal jobs will be started by this pass.

The rescue Source is implemented and compiled. **CityFinal still has its old six-link placement and has NOT been migrated or gameplay-accepted.** No map, Blueprint, material, population art, project configuration, Jam/B asset or package was changed. Updated city placement must wait for Recep's exact new commit and Composer's walkable-roof/capsule/footprint checks.

## Rules and implementation

The existing `ASonHatEvacuationManager` is unchanged and remains the only activation, budget and population authority. Its direct SAFE Hub-to-group links represent completed signal registrations. There is no separate score counter. E still belongs to the Director's native input polling; RMB/LMB/Space and their existing ownership are unchanged.

Enable `bRescueSignalMode` only on the final integrated Director. The default is false to preserve legacy test levels and the one-actor fallback. Rescue mode requires exactly four unique direct SAFE links and four groups: A=35, B=45, C=60, D=20. It uses exactly three charges and `RescuePreparationDuration` (default 150 seconds). Legacy `ConnectionBudget` and `PreparationDuration` do not set rescue-mode budget/duration.

Activation goes through `Director.Interact()` and the existing Manager `TryActivateLink`. Duplicate E cannot score or spend twice. The Director rejects underwater activation before calling the Manager. A completed registration remains counted when its beacon later submerges. A group that was never registered remains unregistered; no physical rescue or death is modeled.

Three-group totals are A+B+C=140, B+C+D=125, A+C+D=115, A+B+D=100. The maximum is deliberately 140/160.

The Director validates topology, unique IDs, populations, placement references, finite bounds/heights and direct Hub endpoints before starting. An extra legacy link with the same NetworkTag is a setup error even when explicit link references are used. Another started Manager is also rejected. This prevents a second usable legacy network; do not hide old terminals as a migration strategy.

## Editor integration after the geometry gate

1. Import the exact new Recep commit through the agreed staging workflow. Record that full SHA and the actual world transforms. Current A/C actor positions and reported heights are leads, not safe spawn/placement coordinates.
2. In `L_SonHat_CityFinal`, retain one Director, one zero-population Hub node and the four population nodes. Replace the six old links with exactly four native `SonHatEvacuationLink` actors. Each link has NodeA=Hub, NodeB=its group's node and FloodSafe=true. Position each link's origin near a clear standing capsule center on that group's actual roof so native E distance/aim works.
3. Keep the complete network loaded. Use the same NetworkTag for its five nodes and four beacons. Set ExpectedNodeCount=5 and ExpectedLinkCount=4. Explicit Nodes/CandidateLinks arrays must match if autodiscovery is disabled. Do not place a standalone Manager or demo bootstrap beside the Director.
4. Enable RescueSignalMode. Fill exactly four `RescueGroups` rows. Each row has GroupId=A/B/C/D, PopulationNode, Beacon, PopulationAnchor, PopulationHalfExtents and FloodClosureElevation. Counts come from the node, never the visual figure count.
5. PopulationAnchor is a verified **feet-origin** gathering point; its yaw supplies facing. PopulationHalfExtents is the verified local XY gathering region in cm, not a building's actor bounds. FloodClosureElevation is that region's actual roof floor world Z, not the actor pivot, beacon top or capsule-center stand Z. Anchor and floor Z must agree within 50 cm. Check the whole region against collision before accepting it.
6. Assign the existing PopulationPresenter's Director reference. Keep six representatives per group and floor projection enabled. In rescue mode its old BuildingPlacements offsets/anchors/count overrides are ignored; the four Director rows own group placement. Figures outside the supplied region (including a 25 cm margin) or projected onto a floor more than 50 cm from the recorded roof floor are omitted, rather than placed on the street below. Existing poses/materials are reused. Missing figures are a placement failure to fix, not an accepted result.
7. Put PlayerStart and ReplaySpawnAnchor at the verified clear standing capsule point. Enable ReturnPlayerToStartOnReplay. Keep the existing absolute level recovery bounds and automatic acknowledgement, with a lower bound appropriate to the final geometry. Do not derive recovery from the last rooftop height. Confirm no obstacle at the spawn or checkpoint.
8. Set the actual water start/final heights and exponent only after roof traces. Use GetTerminalSecondsUntilFlood for each beacon, then verify real traversal and registration windows in rendered PIE. All four groups must be individually reachable with the approved grapple/boost.

No code changes are needed to move the four groups to other verified roofs. There are no hardcoded city XY coordinates in the new Source.

## Beacon, HUD and population presentation

Each Link owns one lightweight five-part runtime beacon: a slim cylindrical post, base/ring and cross-shaped emergency signal. Existing Jam/A materials are reused. Every part has NoCollision, no overlaps and no navigation influence, so it cannot block grapple, E rays or the player. Rescue mode creates no route spline meshes or old console shapes.

Pending is amber, closing is orange, registered is green with a bounded 1.2-second pulse, and unavailable is dim purple. World labels name the roof/population and state. Registered/flooded/no-charge targets offer no usable E prompt. Screenspace guidance names one closest available unregistered group and its distance. It is a distance suggestion, not a navigation/pathfinding proof; the city's actual reachability remains a rendered/human test requirement.

The existing HUD widget layout is reused unchanged. Its content is supplied by the Director: time, signals, registered population, not-yet-reached population and a persistent next objective. Opening instructions explain rooftop people, rescue signals, flood and RMB/LMB/Space/E. Activation feedback names the group and the exact population gained. When no targets or no signals remain, guidance explains that state instead of pointing to a finished/flooded beacon. Short feedback expires after the existing bounded interval.

Results show RESCUE OPERATION COMPLETE, registered/not-reached counts, signals used and PRESS R TO TRY AGAIN, with a registration-only clarification. Replay uses a new owned Manager and recreates beacon presentation, water, cues and population state. Existing audio paths and bounded cue channels are reused. Automatic genuine OOB recovery preserves the current Manager, registrations, budget and running clock.

## Flood integration note for the new city

Latest human report: A `Roof_A` at actor (4570,-5570,650), reported floor Z1300; B `B_Building_02`, floor Z1100; C `Roof_C` at actor (-2140,3210,1380), reported floor Z2780; D `B_Building_04`, floor Z1200. Renaming A/C and a new commit are pending. **These are not applied placements or verified capsule points.**

The previous water maximum Z1800 cannot close C at Z2780. A possible starting curve for later balance is start150/final3000/exponent2.8 over150 seconds. Mathematically, using the *reported, not yet accepted* floor heights, it closes B around101.3s, D105.0s, A108.5s and C145.8s. This gives lower roofs earlier pressure and keeps high C viable longer. **This curve has not been applied or playtested.** Recalculate from the new pin's actual traces and tune against real travel time; do not merely multiply the old 90-second timing. Registered people do not lose their registration when water rises.

Retain the helicopter/rotor payoff in the final design, but implement it only after a working rescue package has been produced. No helicopter, new rotor event or rope change was added in this Source pass.

## Verification and exact files

Changed native files:

- `SonHatEvacuationDirector.h`: opt-in rescue mode, centralized group data and public presentation helpers.
- `SonHatEvacuationDirector.cpp`: rescue topology, 150-second/three-charge configuration, floor-based closure, beacon visuals, objectives, feedback and HUD/result content.
- `SonHatEvacuationDirectorTests.cpp`: reusable rescue fixture and five focused rescue tests.
- `SonHatPopulationPresenter.cpp`: centralized rescue placement/bounds and registration wording; existing mesh/pose animation code retained.

New documentation: this file. `SonHatRescuePreflight.md` retains the earlier gated assessment, with a status update. No HUD widget implementation or Manager/Grapple source changed; their hashes match the preflight baseline.

Editor build: **Succeeded**, `Saved/RescueCandidate/Build-Editor-Complete.log` (last incremental build 3.88s). Initial own-code compile errors were corrected. A sandbox temporary-path linker failure was resolved by regenerating UBT's makefile in the normal build environment; no project configuration was changed.

Final full SonHat suite: **55/55 passed**, 35 clean successes and20 successes with warnings, zero failed/not-run/in-process. Five rescue tests cover all individual contributions, all three-group totals, duplicate E, charge cap, unavailable beacons, legacy topology rejection, HUD/objectives, native E/R, population registration/replay and bounds recovery preserving35/160 plus2 charges while time continues. Existing grapple/boost and legacy suites are included. See `Saved/RescueCandidate/FullTests/index.json` and `Tests-Full.log`.

Warnings include missing headless game viewport and deliberately invalid setup fixtures. Two generic startup `Condition failed` log lines are also present, as in the existing baseline; do not represent this as a completely warning/error-free engine log. There was no crash or failed test.

These are geometry-independent automation results, including synthetic supported-floor recovery. They are **not** real keyboard/mouse gameplay, rendered 1280x720 readability, actual-city collision, sustained movement feel, asset cooking or human acceptance. No rendered screenshot or Windows RescueCandidate package was produced. FinalRevision and every previous package remain untouched.

## Rollback and next gate

`Saved/RescueCandidate/Before/` contains the exact dirty Source baseline. `RescueSource.patch` contains only this pass's four native-file changes; `git apply --reverse --check Saved/RescueCandidate/RescueSource.patch` passed. The patch was not reversed. Recheck before any rollback after further integration edits; never overwrite later work or reset the Git tree. Source hashes/protected-file checks are in `SourceVerification.json`.

Composer can now perform the exact-pin roof/capsule/footprint probe with exclusive Unreal ownership. After accepted geometry: replace the actual six-link placement, configure the four rows, tune flood, test a full rendered round and genuine fall/replay, cook to a new `SonHat_Packaged/Win64_Development_RescueCandidate` directory, then test its real EXE and obtain uncoached human acceptance. No final-city/package success is claimed until those steps finish.
