# Verified rooftop course recovery — 2026-10-17

The saved `/Game/Jam/A/Maps/L_RooftopTest` has been regenerated, reopened and traversed through **A → B → D → C** in rendered, automated UE 5.7 PIE with the actual `BP_GrappleCharacter_C` and CharacterMovement. This is engine runtime evidence, not a claim of human-tested game feel.

## Findings and changes

The original saved first anchor was **not outside range**: untouched PlayerStart `(2100, 0, 1575)` produced character Z 1573.15, camera `(2087.96, -6.02, 1642.52)`, pitch **78.20°**, a **1455.77 cm** Visibility hit on the intended tagged anchor, and successful acquisition without scripted aim. Its center was 1466.38 cm from the camera / 1531.52 cm from the character; native range was 2500 cm and useful minimum 500 cm. No initial roof obstruction or incorrect movement style was reproduced. The exact earlier human failure cannot be identified from that log: it did not record camera/spawn coordinates or detach telemetry. Do not describe it as a proven maximum-range failure.

The course was difficult to read: small, almost-overhead rings hid the direction toward the landing roof. Earlier automation forced aim and therefore did not test discoverability. The revised course puts targets 1200 cm forward and 600 cm above launch origins, enlarges ONLY their map instances fourfold, widens B/C/D to 18 m square, and adds numbered unlit instructions. PlayerStart is now `(2400, -500, 1575)`, facing target 1 at about **24.07°** pitch, clear of the nearby node marker. Native sky/daylight actors are generated too. Because lit surfaces remained dark in the offscreen run, colored unlit roof material instances are embedded inside the map using an existing engine debug material; no external material assets are created. The inspected final landing image clearly shows the blue landing surface and its markers. Graph node/link/director locations and settings remain unchanged.

Forward targets revealed a separate, demonstrated movement fault: the actual Blueprint pawn has **BrakingDecelerationFalling=1500**, even though falling friction is 0 and AirControl is 0.6. Restoring this immediately on release erased 900 cm/s of horizontal travel in about 0.6s. A recorded B→D attempt stopped short and fell to the safety deck. This was falling braking, not rope collision.

The component now restores air steering immediately on release and defers saved falling braking/friction until landing. Reattachment retains the original saved settings; repeated native/Blueprint release is idempotent. Landing, style changes, other detach reasons and EndPlay restore settings. LegacyPull, launch/rope/steering solver, speed cap, collision sweeps and input mappings retain their implementation. No new release impulse, teleport, graph manipulation or map coordinates were added to the component.

## Exact files changed in this pass

- `Source/UEIntroProject_1/GrappleComponent.h`
- `Source/UEIntroProject_1/GrappleComponent.cpp`
- `Source/UEIntroProject_1/GrappleComponentTests.cpp`
- `Source/UEIntroProject_1/GrappleP0Recovery.md` (historical-report notice)
- `Source/UEIntroProject_1/GrappleCourseVerification.md` (this new report)
- `Saved/SonHatTools/build_l_rooftop_test.py`
- `Content/Jam/A/Maps/L_RooftopTest.umap`, saved only through Unreal.

Verification helpers created under Saved: `GrappleCourseSpawnAudit.py`, `GrappleCourseGeometryAudit.py`, `GrappleCourseSaveValidate.py`, `GrappleCourseRuntime.py`, `GrappleCourseCandidate.py`, `GrappleCourseMaterialAudit.py`, `GrappleCourseFinalizeReport.py`. Saved is ignored by Git; preserve/copy these explicitly if sharing the harness. No changes to Blueprint assets, project config, GrappleTest/EvacTest maps, evacuation/flood source, or Content/Jam/B. Existing unrelated uncommitted changes were preserved.

## Measured runtime results

All distances and positions below are cm; speeds cm/s. Each attach began standing at zero velocity, without jump/run setup. First acquisition uses the untouched spawn camera; later transitions walk normally to a launch marker and aim once. Test calls the same `TryStartGrapple`, `SetReeling`, `StopGrapple` methods used by gameplay; no pawn teleports or direct launch-velocity assignments. RMB release is ~0.73s; LMB reel pulse ~0.32–0.46s. Looking toward the landing after release does not alter the earned trajectory.

| Transition | Camera hit | Initial speed | Travel at ~0.3s | Reel shortening | Peak attached speed | Landing center |
|---|---:|---:|---:|---:|---:|---|
| A→B | 1232.3 | 1237.9 | 342.7 | 24.0 | 1237.9 | (3290.2, 471.2, 1775.6) |
| B→D | 1295.1 | 1237.9 | 342.6 | 24.0 | 1237.9 | (2318.5, 2539.0, 1370.6) |
| D→C | 1302.7 | 1237.9 | 342.6 | 24.0 | 1237.9 | (1242.0, 2211.3, 1673.1) |

### A → B

- Character start: (2400.0, -500.0, 1573.1); camera: (2390.9, -509.9, 1642.5); anchor center: (3210.9, 384.6, 2175.0).
- Camera/character to anchor CENTER: 1325.1/1342.5 cm. Actual Visibility hit: 1232.3 cm on `Anchor_Swing1_A_to_B`; acquisition succeeded.
- Actual initial velocity: (608.2, 663.4, 850.0) cm/s; rope initially 1260.2 cm; started Walking and entered Falling.
- Position at 0.3001s: (2582.5, -300.9, 1784.1); at 1.0002s: (3008.3, 163.5, 1933.1).
- Release velocity: (608.2, 663.4, 131.2) cm/s; velocity discontinuity 0.0000 cm/s. Detach reason `Released`. Direction is this actual velocity normalized.
- Landed on B at (3290.2, 471.2, 1775.6), 1.469s after attachment. Real capsule center within roof bounds and at roof top plus capsule half-height.

### B → D

- Character start: (3303.8, 892.4, 1775.6); camera: (3310.7, 880.9, 1845.0); anchor center: (2661.4, 1966.0, 2377.5).
- Camera/character to anchor CENTER: 1372.1/1388.3 cm. Actual Visibility hit: 1295.1 cm on `Anchor_Swing2_B_to_D`; acquisition succeeded.
- Actual initial velocity: (-462.1, 772.3, 850.0) cm/s; rope initially 1321.4 cm; started Walking and entered Falling.
- Position at 0.3000s: (3165.2, 1124.1, 1986.6); at 1.0000s: (2841.7, 1664.7, 2135.6).
- Release velocity: (-462.1, 772.3, 131.3) cm/s; velocity discontinuity 0.0000 cm/s. Detach reason `Released`. Direction is this actual velocity normalized.
- Landed on D at (2318.5, 2539.0, 1370.6), 2.133s after attachment. Real capsule center within roof bounds and at roof top plus capsule half-height.

### D → C

- Character start: (2314.0, 2546.5, 1370.6); camera: (2326.9, 2550.5, 1440.0); anchor center: (1111.6, 2170.5, 1972.5).
- Camera/character to anchor CENTER: 1380.2/1396.2 cm. Actual Visibility hit: 1302.7 cm on `Anchor_Swing3_D_to_C`; acquisition succeeded.
- Actual initial velocity: (-859.0, -268.6, 850.0) cm/s; rope initially 1328.8 cm; started Walking and entered Falling.
- Position at 0.3000s: (2056.3, 2465.9, 1581.6); at 1.0000s: (1455.0, 2277.9, 1730.6).
- Release velocity: (-859.0, -268.6, 131.3) cm/s; velocity discontinuity 0.0000 cm/s. Detach reason `Released`. Direction is this actual velocity normalized.
- Landed on C at (1242.0, 2211.3, 1673.1), 1.250s after attachment. Real capsule center within roof bounds and at roof top plus capsule half-height.

## Evidence and build

- `Saved/GrappleCourseRecovery_20261017/BeforeSpawn.json`: original saved-map actual spawn, camera, bounds, LOS and unassisted acquisition.
- `Saved/GrappleCourseRecovery_20261017/FailedRestoredBraking.json`: failing pre-fix course runtime with actual Blueprint braking and per-tick release velocities.
- `Saved/GrappleCourseRecovery_20261017/SavedMapValidation.json`: saved/reopened map, all three LOS collisions, landing surface traces and exact before/after graph/flood snapshot equality.
- `Saved/Automation/GrappleCourseRecovery_20261017/RuntimeFinal.json`: complete final traversal, with frame sample times and velocities. `Saved/Logs/GrappleCourseRuntimeFinal.log`: event telemetry and engine log.
- `Saved/GrappleCourseRecovery_20261017/SpawnFinal.png` and `LandingViewFinal.png`: rendered captures inspected during verification. HighResShot omits UMG; these are not full HUD screenshots.
- `Saved/Logs/GrappleCourseBuild.log`: **UEIntroProject_1Editor Win64 Development succeeded** using the working UE5.7 Build.bat workflow after user closed Editor/Live Coding.
- `Saved/Automation/GrappleCourseRecoveryTests/index.json`: **26 passed, 0 failed** (19 clean + 7 expected headless viewport warnings). Includes post-release movement ticks with the actual 1500 falling brake, duplicate release, reattachment and landing restoration.

No unexpected grapple detachments, gameplay exceptions or crashes occurred in the final traversal. Two generic `LogAutomationTest: Error: Condition failed` messages occur during Editor startup before PIE, also in baseline runs; their source was not identified. An earlier unsaved generation experiment crashed the isolated NullRHI Editor with divide-by-zero at frame zero; commandlet generation and the final rendered runs completed normally. These are not concealed as gameplay passes.

## Final confirmation procedure

1. Open `Content/Jam/A/Maps/L_RooftopTest`. Use normal Play, with **Spawn Player At → Default Player Start**, rather than Play From Here/current-camera spawn. Select Lit viewport mode (Alt+4) for normal lighting; the existing Editor preference was Unlit. No asset/input/Level Blueprint hookups are required.
2. Click the game viewport. Target **1 → B** is already in front of the spawn camera. Hold RMB from standing; tap LMB to reel, then release RMB after about 0.7s. Look down toward B after release. No jump/run setup is needed.
3. Land on B, walk to LAUNCH 2 and aim at **2 → D**. Repeat, then LAUNCH 3 / **3 → C**. A/D steering remains available; the measured route needed no air steering. Existing E/F/R round behavior remains in place (F still depends on the director's existing testing option).

Tuning remains on GrappleComponent: range 2500, minimum attach 500, rope minimum 350, tangent/forward/up floors 1400/900/850, reel 650, swing cap 2600, release assist 0. Course geometry is tuned only in the generator. Existing graph/flood values: 90s preparation, budget 3, flood 200→1650, reveal 8s.

## Limits and rollback

No physical human mouse/keyboard playtest, subjective fun judgment, packaged/cooked build, or exhaustive release-duration/air-steering sweep was performed. The final three transitions were engine-tested using scripted aim and gameplay methods; OS input/focus and human timing remain the precise confirmation gap. Existing native RMB/LMB key handling has separate automated coverage. This pass did not evaluate E/F/R through physical keys or redo the 90s flood round; state preservation and existing regression tests passed.

Rollback copies and pass-only patches are in `Saved/GrappleCourseRecovery_20261017`. With Editor closed, run `git apply --check Saved/GrappleCourseRecovery_20261017/Course-only.rollback.patch`, then apply that patch and rebuild. Regenerate the earlier geometry with the restored generator through Unreal. Do not hand-edit/overwrite map bytes. The patch is relative to the pre-pass uncommitted implementation, not Git HEAD. Do not force a patch if newer edits prevent checking. LegacyPull remains available as the designer fallback.
