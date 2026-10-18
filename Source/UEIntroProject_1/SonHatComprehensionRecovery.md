# SON HAT — Comprehension Recovery handoff

2026-10-18. Work began ~12:04 TRT; implementation, rendered checks and new Windows package completed before 13:05. Stop here for human acceptance.

**Candidate: built and agent-tested. HUMAN ACCEPTANCE: PENDING.** FinalRevision's reported comprehension failure remains the baseline. No uncoached human has accepted this candidate. Passing tests and staged viewpoints do not establish fun or player understanding.

## Run this candidate

`C:/Users/KAAN BARTU BOZKURT/Documents/Unreal Projects/SonHat_Packaged/Win64_Development_ComprehensionRecovery/UEIntroProject_1.exe`

This is the new archive requested, not a replacement for FinalRevision. Startup is `/Game/Jam/A/Maps/L_SonHat_CityFinal`. Editor and packaged test processes have exited normally. No commit, reset, branch switch, merge or push occurred.

## Actual inventory and cause

- Authoritative graph: native `ASonHatEvacuationManager`, `ASonHatEvacuationNode`, `ASonHatEvacuationLink`. The manager computes hub reachability using selected SAFE edges. RISK spends an opportunity without carrying access. No scoring semantics or private activation state changed.
- Existing `ASonHatEvacuationDirector` and native `USonHatEvacuationHUDWidget` remain the only round/HUD system. The HUD widget implementation itself was unchanged in this pass; its Director-supplied content was corrected.
- Six actual native Links had SceneComponent interaction roots. Cosmetic boxes did not communicate a console or its purpose. There are still exactly six links, not a second terminal system.
- Existing `ASonHatPopulationPresenter` already represented 160 residents with 24 instanced humans. Normal group labels were disabled in this map, placement obscured people, and the material washed out under automatic exposure.
- Old instructions faded before any connection, feedback did not name the affected rooftop clearly, and the next marker could suggest a connection without useful hub access. Those were comprehension failures despite functioning E/scoring.
- Existing Grapple recovery used last supported ground minus 1500 cm. From high ground, a valid descent toward lower city roofs could cross that threshold above water. Flood contact and this fall test are separate.

## P0 changes

Persistent compact objective and named primary marker now remain until an actionable successor exists. Before E, the prompt names the real route and its cost. SAFE confirmation lasts eight seconds and names newly connected rooftops, their newly gained access, total access and remaining opportunities. RISK explicitly says no new people gained access. Exhausted or flooded choices stop being suggested. Results distinguish access from lack of access, used opportunities and failed RISK choices; no physical rescue/death is asserted.

The next objective is the nearest unused, un-submerged SAFE link joining the hub-connected component to a disconnected rooftop. Used, RISK and redundant links cannot become the primary objective. This is graph/availability guidance, not a pathfinding guarantee across city geometry.

CityFinal has an explicit **up-to-12-second orientation interval**. Water and the full 90-second challenge clock hold together; the first successful E ends the interval early. Afterward the unchanged 90-second challenge runs normally. Other levels default to zero intro time.

Each existing Link owns five collision-free runtime cube-mesh parts: body, screen housing, screen, status light and control pad. SAFE is green, RISK amber, used screens dim and labels say SET, submerged screens are purple/CLOSED. No additional interactive actor or Blueprint was created. Unlit route text stays readable under storm lighting. Compact result debug node spheres no longer appear at the camera; group colors and route lines communicate results.

CityFinal opts into the existing absolute recovery interface at **Z=-900 cm**, below valid city roofs, and automatically acknowledges genuine OOB retry. It retains the same manager, score, opportunities and clock. Manual R remains the existing fallback; results R restarts the full round. A checkpoint must be walkable and its return remains collision-checked. Grapple swing/reel, targeting, boost and earned release momentum were not edited.

## Console assets and exact placements

New materials under `/Game/Jam/A/Materials/`:

- `M_EmergencyConsoleBody` — Brightness .04.
- `M_EmergencyConsoleTrim` — Brightness .05.
- `M_EmergencyConsoleScreen` — Brightness .025, runtime state color.
- `M_CitizenReadable` — Brightness .018, original citizen palette and pose meshes.

All six interaction centers/endpoints/topology are unchanged. Only cosmetic facing was authored. Coordinates are cm.

| Existing Link | Rule | Center X, Y, Z | Visual yaw |
|---|---|---|---|
| Hub-A | SAFE | 95.000, -1895.000, 983.000 | -132.95 |
| A-B | SAFE | 21.000, -1731.005, 970.000 | -95.48 |
| Hub-C | SAFE | 2156.666, -2115.547, 1270.000 | 175.61 |
| C-D | SAFE | 2045.194, -2054.692, 1270.000 | 177.07 |
| A-C | RISK | 217.237, -1984.758, 970.000 | 170.91 |
| Hub-D | SAFE | 3101.685, 1910.089, 1170.000 | -104.76 |

Budget stays 3. A=35, B=45, C=60, D=20. Hub-A, A-B, Hub-C can give 140/160 if prepared before closure; the manager test verifies this. Hub-A, RISK A-C, A-B gives 0→35→35→80. No score target was added to the HUD.

## Population/map presentation

Six representatives per rooftop, original 24 instanced pose meshes and batched reactions retained. Counts remain logical populations, not figure counts. Feet still project to WorldStatic support; no person collision, physics, AI or per-person Tick was added. Existing waiting/threat/safe/stranded reactions and replay state remain.

Population anchor centers (cm): A=(500,-1850,855); B=(550,1880,1005); C=(1650,-2600,1155); D=(2710,1770,1055). Feet projection resolves actual roof heights independently. Labels are enabled, size 20, height 245; they name the roof and resident count and distinguish needing a route from access to high ground. PlayerStart stays (-40,-2040,945), facing Hub-A. The HUD associates A's green reaction with its 35-person confirmation.

A single Jam/A-owned unbound `SonHatComprehensionPostProcess` volume overrides **only bloom (.08)**. Existing automatic exposure is preserved. A tested fixed-exposure variant darkened the city and was removed before the archive was built.

**Actual map flood tuning is start150/final4200/exponent2.45**, not the stale generator's former1800/2.8. This pass preserves the actual curve and aligns generator defaults. Preparation-terminal closures therefore occur roughly47–54 seconds into the challenge, depending on terminal height, rather than the previously quoted70–78. The explicit intro delays the challenge start; it does not silently slow the curve. Further balance requires a human playtest decision.

## Exact implementation files

Changed native files:

- `Source/UEIntroProject_1/SonHatEvacuationDirector.h`
- `Source/UEIntroProject_1/SonHatEvacuationDirector.cpp`
- `Source/UEIntroProject_1/SonHatEvacuationDirectorTests.cpp`
- `Source/UEIntroProject_1/SonHatPopulationPresenter.cpp` (player-facing label text)

Map: `Content/Jam/A/Maps/L_SonHat_CityFinal.umap`, saved through Unreal APIs. Four new material `.uasset` files correspond to the material paths above. No existing Blueprint or population mesh/animation asset changed.

Generator: `Saved/SonHatTools/build_l_sonhat_city_final.py`, updated with intro/bounds, group placement/material/labels, console facing, audited flood values and bloom-only volume. It was **not** run destructively over the integrated city. `Saved/CityIntegration/city_final_layout.json` is unchanged.

Task scripts in `Saved/ComprehensionRecovery/`: `AuditAndMaterials.py`, `PatchCityPresentation.py`, `BalancePresentation.py`, `ValidateCityPIE.py`, `RestartPIEProbe.py`, `FinalVisual.py`, `PackageCandidate.ps1`. JSON reports, logs, screenshots and source/map backups are alongside them. `ChangedFiles.txt` enumerates deliverable files and `PackageManifest.json` hashes the new executable/container payloads.

Task-only `ComprehensionRecovery.patch` passes `git apply --reverse --check`; it preserves prior dirty work. Original map/source copies are in `Before/`. These are rollback aids, not an applied rollback.

## Verification and limits of evidence

- Final Editor build succeeded (`Build-Release.log`,16.98s); UAT built game/Editor, cooked, staged and archived successfully (ExitCode0; BuildCookRun154.73s).
- Final suite **50/50 passed**:35 clean,15 with warnings. Warning events are headless missing-viewport notices and deliberately invalid network fixtures. Two generic startup `Condition failed` log lines also occur in the pre-existing passing baseline logs; no crash occurred. They are not represented as a clean error-free log.
- Packaging has one unique existing third-party warning: `store_front_door_b_left` recomputes physics and should be resaved for deterministic cooking. It is outside this pass's ownership and was left unchanged. Include-order migration notices also remain.
- Actual rendered CityFinal PIE: desktop E sequence0→35→35→80, A response, named successor, six one-per-link consoles, all24 figures, staged valid high-to-low descent, staged OOB return with80/160 and zero opportunities unchanged, natural80/160 result, actual R replay to0/160 and3. `PIEValidation.json` reports success; viewport1280×720. The first helper run failed on empty authored ConnectionName strings, not gameplay; the fixed helper uses actual actor labels.
- Final saved presentation was separately rendered in PIE (`FinalVisual.json`, pass). Its automated public Interact/staged viewpoints are explicitly separate from keyboard evidence; viewport1517×808. This was before the last small used-RISK dimming/label/replay-coverage correction, subsequently compiled/tested/packaged.
- **Actual new packaged EXE**: startup CityFinal; idle natural0/160 result; actual R replay; actual E→35/160 and2 opportunities; visible green A reaction and named A-B objective; real Space jump; real RMB acquire/release. Release log velocity=(613.28,658.70,835.01)cm/s, peak1237.9cm/s. This is a short input/motion check, not sustained swing/reel or chained traversal acceptance.
- Packaged OOB probe used built-in Development `BugItGo(-2000,-4000,-1000)` then `Walk` to restore normal collision/movement. Automatic checkpoint return occurred once, preserving35/160 and2 opportunities while time continued. This is a **console-staged** test, not a naturally played fall.
- Packaged natural35/160 result showed125 without access; actual R restored0/160,3 and intro. Test EXE closed normally. `PackagedVerification.json` and `Packaged.log` record evidence. Launch-only F10 console override was for QA, not a project configuration edit.
- Actual packaged screenshots `Packaged_01_RoofA35.png`, `Packaged_02_Results35.png`, `Packaged_03_Replay.png` are **1280×720** engine screenshots. Window capture appeared larger due Windows DPI; screenshot headers verify actual render dimensions.

## Safety / remaining gates

Jam/B hashes match the task-start snapshot. Grapple/boost physics, HUD implementation, Manager scoring, audio source/assets and population meshes/animations match the task-start source/assets used by this pass. Existing uncommitted changes remain. FinalRevision executable/content payload snapshot entries are unchanged. Two pre-existing archive Saved profile/log files have12:10 runtime timestamps; those were preserved, not reverted. New package QA history/screenshots/settings are ordinary generated Saved files.

The imported city's large bright cyan props remain and can compete with labels; they were not hidden or modified under the Jam/B restriction. Some close world labels overlap the HUD; the screen-space objective and E prompt remain readable. No human performance comparison, headphone acceptance, sustained LMB reel, three-transition traversal or uncoached comprehension pass is claimed. Twenty-four figures remain lightweight, but no frame-time regression benchmark was performed.

## Five-minute technical check (after the uncoached run)

1. Launch the new EXE normally. Confirm the persistent first objective; let the intro expire once, then replay and connect early once.
2. E at Hub-A:35/160,2 connections, Roof A group green. Read the named successor. Inspect amber RISK before E.
3. Choose RISK A-C, then SAFE A-B:35 remains after RISK, then80; no extra budget from repeat E. Alternately test three SAFE choices before closure for140.
4. Traverse with the approved RMB/LMB/Space controls. Verify no relative-height false-fall while descending toward lower roofs. Check a genuine OOB recovery retains score/time; ordinary streets are not automatically OOB.
5. Wait for results; compare access/without-access/used opportunities. R must reset routes, figures, cues and three opportunities without leftover result spheres or grapple cosmetics.

For the final human gate, give an unfamiliar player the EXE with **zero verbal gameplay instructions** and keep this report hidden until they play. Afterward ask them to explain: the goal; what35 means; what green means; why three opportunities; SAFE versus RISK; where to go next; what decides the result. If they cannot, mark HUMAN ACCEPTANCE FAILED. This candidate remains PENDING until that happens. Stop feature work and hand off the candidate for that test.
