# SON HAT — final player experience release gate

Audit started 18 October 2026 (Asia/Nicosia), current integration/son-hat working tree. Pre-existing Composer/player-experience changes are preserved, not replaced with HEAD. Original source copies and source/asset/config/package manifests are in Saved/ReleaseGate/Before and *-before.json. No existing packaged output will be overwritten. This document is the P0 defect matrix recorded before gameplay edits; runtime findings and final evidence are appended below.

## P0 defect matrix

| Symptom | Confirmed cause / uncertainty | Owner | Smallest correction | Validation |
|---|---|---|---|---|
| First terminal cannot be discovered | Compact mode hides link labels; TerminalMarker is always invisible; prompt requires current focus within 275 cm. Saved playable uses compact mode and empty ConnectionName. No persistent screen pointer exists. | Director/HUD | Small screen markers from actual terminal locations; one useful safe-route objective and concise onboarding; preserve manager checks. | 1280×720 rendered HUD, first terminal before interaction range, native and focused-window E; human 20–30 s discovery pending. |
| Objective clips / technical names | Opening text is a long unwrapped line and hardcodes 90 s; labels fall back to graph names. InteractionPrompt is 280 logical px wide, offset left of center. | Director/HUD | Short staged player-facing text, actual configured time/budget, readable destination labels and centered prompt. | Native widget/wayfinding checks and rendered spawn/interaction/results. |
| Blue blob reported intermittently | Latest code disables travel by default, hides terminals, and suppresses anchor meshes. Exact active component not yet identified; do not claim a cause from screenshots alone. | Grapple/Director cosmetics | Identify rendered Sphere/rope owners and state transitions first. Correct confirmed lifecycle issue only; no physics/targeting changes. | Baseline runtime component inventory, rapid attach/release, failure/obstruction/result/replay and screenshots. |
| First-person body/arms reported | ConfigureFirstPersonOwnerVisibility runs during component BeginPlay; local possession may occur later. It returns early when not locally controlled. Blueprint defaults/runtime inspection pending. | Grapple presentation | Apply owner visibility when local controller becomes available, if baseline confirms it was skipped. | Actual BP pawn component owner-no-see state plus player-camera capture. |
| Fall waiting is unsafe / timer mismatch | A fall calls StopMovementImmediately once but leaves MOVE_Falling. Director repeatedly extends manager deadline while water uses a separate preparation-start clock. RMB may still attach while waiting; R prioritizes checkpoint retry even after results. | Grapple recovery/Director | Hold pawn safely while acknowledgment pending, prevent actions, keep timer/flood coherent without indefinite deadline extension, phase-aware single R branch. | Real CMC wait, raw input, clock/water progression, deadline/result and replay assertions. |
| Result/replay may retain tether/audio | HandleRoundEnded does not detach grapple. ApplyPlayerReplayReset returns before StopGrapple/checkpoint reset when ReturnPlayerToStartOnReplay=false (saved playable default). | Director/Grapple cleanup | Always clean grapple/retry at round boundary; teleport only when configured. | Attached result, both replay modes, one tether and stopped reel loop. |
| Tagged city buildings could disappear | SuppressGrappleAnchorCosmetics hides every StaticMesh on every GrappleAnchor actor, including otherwise valid solid roofs. | Director presentation | Restrict cosmetic suppression to actual small decorative/non-Pawn-blocking anchor meshes or explicit cosmetic tags. | Native solid tagged mesh stays visible; current anchor art suppressed; targeting unchanged. |
| Audio/cook and population integration | 13 runtime-loaded audio paths exist in source; population already has lightweight native humans. Cook inclusion and actual playback pending. | Existing audio/population; audit only | Validate references and lifecycle; avoid art/audio rewrite. Report static-string cook risk precisely. | Asset registry/staged package check, results/replay loop counts, unchanged population collision/states. Human listening pending. |

## Scope

No movement tuning, swing/reel solver, targeting rules, collision or release math will change. No maps, Blueprint assets, population art/code, Jam/B or L_City edits are planned. Localized ownership: Director/HUD/Reticle/Grapple lifecycle and focused tests, this report, ignored audit helpers. Check fingerprints immediately before each edit so concurrent work is not overwritten.

## Automation crash identification — 18 October 2026

The repeated EXCEPTION_ACCESS_VIOLATION at address 0x170 with FSonHatRelativeRecoveryTest::RunTest matches the saved crash UECC-Windows-9ABB75FB4628C9AF6145C8BFE05676C5_0000, October 18 at 00:32:56 local time. Its command-line automation process was PID 16692 (UnrealEditor-Cmd). Both the Engine address 0x00007ffdca482e74 and project frame 0x000002002c446d01 match the captured stack/portable module offsets.

This pass introduced that crash by adding Scene.World->Tick to the synthetic FGrappleTestWorld. The fixture supports directly ticking its CharacterMovement component, but does not establish a complete game world suitable for the normal full-world tick. The test was corrected at 00:33:53 to call Movement->TickComponent(0.25f, LEVELTICK_All, nullptr), retaining the planted-position and pending-retry assertions.

The corrected RelativeRecoveryBelowWorldZero test passed without warning/error entries in the 00:37:16 report. The full report contains 41 successes (30 clean, 11 with warnings), 0 failures and 0 not run. Current module rebuild completed at 00:56:55. No full-world Tick call remains in this test. The latest crash directory observed at 01:07 is still the 00:32:56 report. These facts identify the pasted report; they do not certify every possible gameplay crash.

Evidence: Saved/ReleaseGate/Tests-first-crash.log, Saved/Crashes/UECC-Windows-9ABB75FB4628C9AF6145C8BFE05676C5_0000/CrashContext.runtime-xml, Saved/ReleaseGate/Tests/index.json, and Source/UEIntroProject_1/GrappleComponentTests.cpp. No process was stopped or new Editor/automation instance launched for this diagnosis.

## Resumed P0 delivery — 18 October, 01:16 onward

The historical crash section above is retained as an earlier record. It was not reinvestigated during this resume. This pass resumes the existing player-experience fixes and adds visible physical terminals. Stop here for human PIE acceptance; final-city integration is the next milestone.

### Exact source ownership

Files changed during this resumed pass, relative to Saved/ReleaseGateResume/Before:

- SonHatEvacuationDirector.h / .cpp: restrained runtime terminal consoles, compact SAFE/RISK/E/SET/CLOSED labels and clearer opening objective. Consoles use the engine Cube, no collision, no shadows and no external assets. Default dimensions are 32 x 24 x 90 cm. A single downward WorldStatic trace places each console on a roof within 400 cm; without a floor hit it uses the existing terminal origin as its placement reference. Interaction still uses the existing Link origin, range, line of sight, flood closure and authoritative manager activation.
- SonHatEvacuationDirectorTests.cpp: extends the existing guidance test to check six visible, human-scale Cube terminals without collision. Existing authoritative availability and replay checks remain.
- SonHatEvacuationHUDWidget.cpp: moves a waypoint card beside the aiming point when it would obscure the reticle or grapple feedback. Offscreen guidance and other cards remain.
- SonHatReleaseGate.md: this handoff and evidence record.

The full original player-experience pass also changed GrappleComponent.h / .cpp / Tests.cpp, GrappleReticleWidget.cpp, SonHatEvacuationHUDWidget.h and the Director/HUD files above. Those earlier changes remain in the working tree. This resume did not modify any Grapple file. The earlier work handles late-possession owner visibility, grapple/retry presentation cleanup, phase-aware R and concise HUD/reticle text. MovementPreservation.json records unchanged targeting, swing/reel, attach impulse, release/momentum and legacy movement functions against the original pass baseline.

No Manager/Node/Link scoring source, population source or art, Blueprint asset, map, Jam/B content, root project configuration or staged package was changed by this resumed pass. Pre-existing edits to those paths remain. Audit scripts, reports, original source copies and PNGs are under Saved/ReleaseGateResume; they are verification artifacts, not runtime dependencies. Resume-only and full-pass source patches are retained there for review/rollback without discarding unrelated work.

### Player-facing behavior

At round start the HUD says: “Prepare evacuation routes. Get to a SAFE marker, aim, press E.” It identifies green routes as surviving the flood and orange routes as failing. A screen marker supplies the destination and distance, with an edge arrow when the suggested terminal is offscreen. Suggestions favor an available safe link that extends current hub access; selected, submerged, exhausted-budget and inappropriate phase/retry targets are excluded.

The terminal remains a visible, narrow green/orange console. Its short world label changes to E when focused, SET when a safe route is selected and CLOSED if submerged. The lower screen prompt names the buildings and shows the one-connection cost. Unsafe prompts explicitly say “fails in flood” before activation. Aim within the configured 275 cm range and press E; the manager still authorizes every activation.

Useful safe activation immediately reports the access increase and updates the graph and existing population presentation. The inspected test choice sequence is Hub–A: 0 -> 35, unsafe A–C: stays 35 while spending one connection, then safe A–B: 35 -> 80. Thus the final graph reports 80 with access and 80 without. The population continues to mean 160 residents represented by 24 figures, with no physical evacuation or casualty claim. Other safe choices remain available in the existing graph; this pass does not change their scoring.

### Verification matrix

| Item | Code / automation | Rendered PIE evidence | Human acceptance still needed |
|---|---|---|---|
| Objective, discovery, E | Existing manager authority retained; guidance test checks availability and visible collision-free consoles | Actual 1280 x 720 client, ~5.5 m staged discovery view and near-console E prompt; real desktop E changes 0/160 to 35/160 and budget 3 to 2 | Uncoached player finds and understands the first terminal within 20–30 seconds |
| Safe/unsafe stakes | Unsafe route spends budget without adding safe access; safe chain produces 80 | Both warning before activation and changed HUD/console state captured | Comfortable readability during normal traversal and clear personal understanding of why risk matters |
| Body / arms | Actual BP pawn runtime skeletal mesh components have OwnerNoSee; configuration is reapplied after local possession | No owner body or arms in the inspected player-camera frames | Repeat with the final city pawn/camera and prolonged play |
| Hook / blue objects | Runtime SwingAndReel, hook travel false; one live spline tether, zero visible hook cones; zero tether/head after release, twelve rapid cycles and miss | Ready, attached, reeling, release, neutral and blocked views captured; no extraneous blue sphere in these inspected states | Reported intermittent artifact cannot be ruled out in all circumstances by this bounded audit |
| Fall / checkpoint | Pawn movement held while acknowledgment pending; time/water continue; E/RMB blocked; R preserves manager, budget and score | Actual YOU FELL message, held frame and desktop R return to the last roof | Natural fall while grappling through the complete course, including streamed-city checkpoint support |
| Deadline / replay | Natural 90-second manager deadline, reveal and graph result; round-end grapple/audio cleanup; R creates new manager | Natural flood/result and actual desktop R replay are checked by the final audit | Human repeatability and final city round pacing |

The checked runtime has free-surface grappling disabled by the existing Blueprint default (bAllowSolidWorldSurfaces=false). It uses tagged/approved anchors. This was preserved deliberately. A “Cannot grapple here (RMB)” message on a terminal is consistent: E operates the console; RMB requires a valid grapple target. No cosmetic hook travel was re-enabled.

Visible blue roof/graph/flood geometry remains part of the test scene. The inspected preparation-phase Sphere mesh inventory contains only the sky sphere, not a pawn aim sphere or a terminal sphere. Results may show the existing red/green graph node markers. Do not conflate those known owners with a reproduced intermittent hook defect.

### Build and test evidence

- UE 5.7 Editor Development Win64 build succeeded after the final HUD layout change: Saved/ReleaseGateResume/Build-final.log (15.87 seconds).
- Full SonHat suite after terminal implementation: 41/41 passed, zero failures or not-run tests (30 clean, 11 with warnings). Saved/ReleaseGateResume/Tests/index.json.
- Final focused SonHat.ReleaseGate suite after the HUD adjustment: 4/4 passed (2 clean, 2 with warnings), zero failures or not-run tests. Saved/ReleaseGateResume/Tests-final-focused/index.json. Headless “no game viewport” warnings are present; these tests do not establish UI readability.
- Rendered PIE uses the actual BP pawn and player camera in L_SonHat_Playable, with a 1280 x 720 client (engine SHOWUI files include the native window frame, 1288 x 770). ValidatePIE.py stages positions/camera direction, calls public activation/grapple methods for repeatable inspection and records component ownership. Desktop E, checkpoint R and results R use Windows input through the computer-use skill. It is not a human traversal or mouse-feel playtest.
- Early audit attempts are retained separately: one input wait exceeded the real round, one checked population state before its scheduled refresh, and one aim fixture moved because rapid release correctly retained launch momentum. They did not require gameplay/physics changes. Screenshot phases now pause before changing position or input so a queued capture depicts the named state.
- Final rendered evidence is in Validation.json, RenderedPIE-final.log and PNGs 01–20 under Saved/ReleaseGateResume. The raw script ends with a failed blanket “all population Waiting” assertion after recording replay. It is preserved, not relabeled as a passing run. EvidenceReview.json separately checks the recorded states: all 14 reviewed checks pass, including replay matching the actual fresh-spawn population, 89.4 seconds remaining, 0/160, three connections, 24 figures, six consoles and no tether/head. The helper expectation was corrected for future runs; it was not rerun after that correction.
- Existing map finding: Building A is FloodThreatened at initial spawn and after replay; B/C/D are Waiting in both. Thus the presenter does reset to its initial state, but “all Waiting” is not true for this map. The level/population owner should check the A group roof anchor, projected foot elevation and initial-water warning clearance. Neither its cause nor its correction is asserted by this presentation-only pass.

### Five-minute human PIE checklist

1. Open the existing L_SonHat_Playable. Press Play and click the game viewport. Without coaching, read the goal and find the green SAFE terminal using its world console and screen marker. Check 1280 x 720 readability; the marker must not cover the grapple reticle.
2. Approach and aim until the E prompt shows High ground to Building A and its cost. Press E: access must rise 0 -> 35, connections 3 -> 2, the console becomes SET/green and the existing residents react. Inspect the orange A–C terminal warning before activating it: access stays 35, connections drop to 1. Prepare safe A–B: access becomes 80 and budget 0.
3. On a valid anchor, hold RMB, add LMB, release and repeat rapidly; try an obstruction and a miss. Expect one tether while attached, none after release, readable ready/blocked feedback and no first-person body or persistent hook. Confirm the previously approved feel and momentum personally.
4. Fall off a roof during preparation. Expect YOU FELL and “R: last rooftop (round time keeps running)”, with no automatic teleport. Wait briefly, then press R: return to the last supported roof with the same remaining connections, score and continuing timer.
5. Let the 90-second timer expire and the configured 8-second reveal finish. With the choices above, expect 80 with access, 80 lacking access and the unsafe route failed. Press R as instructed: a full fresh round, 0/160, three connections, initial population states and no leftover tether. In this saved map A starts FloodThreatened; B/C/D start Waiting. ReturnPlayerToStartOnReplay=false, so full replay resets the round at the current position; it does not promise a spawn teleport.

### Final-city integration handoff / remaining gates

- Human first-run understanding, grapple feel and the intermittent visual complaint remain acceptance gates. Audio listening and final city performance were not certified by automation.
- L_SonHat_Playable already renders an engine warning about multiple directional lights; lighting also washes out some surfaces. Its owner should choose the intended light/ForwardShadingPriority. No map/light asset was edited or warning globally suppressed.
- The initial Building A flood-threat state needs a placement/water-settings review by the level/population owner before final-city acceptance. This is retained as a known integration issue rather than changing protected map/population files.
- On the integration-owned city map, use the existing Director and game mode/player setup, five nodes (hub + 35/45/60/20) and six authored candidate links with the correct network tag/references. Keep the existing 90-second/three-connection configuration and at least two useful safe plans. No bootstrap or second Director is needed in that final network.
- Put each Link origin at a reachable standing interaction height, with clear Visibility line of sight and preferably >550 cm separation between independent terminals. The visual console seeks WorldStatic roof support below that origin; verify the result on real geometry. Explicitly configure population rooftop anchors using the existing presenter.
- Keep roofs loaded, solid and Pawn/Visibility-blocking. Use tagged grapple anchors for the current anchor-only Blueprint default; enabling solid-surface targets is a separate deliberate integration choice. Check water elevations, vulnerable/protected terminals, last-safe-ground support and that relative fall recovery triggers before KillZ.
- Rebuild/package and test the integrated city later. The existing successful staged package/archive is preserved and does not contain these latest source changes. Static-string audio references and final cook inclusion still need packaged verification by the release owner.
