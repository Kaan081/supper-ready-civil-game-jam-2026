# Final player experience — 18 October 2026

The existing HUD now explains the flood deadline, limited connections, safe-route access and first green console in the opening ten seconds. Route feedback and results describe access to high ground without claiming physical rescue or deaths. Round audio uses bounded owned components, continues through flood reveal, then cleans up at results/replay. Movement remains frozen at the human-approved implementation.

## Exact source ownership

Changed relative to the pass-start copies in `Saved/PlayerExperienceP0/Before/`:

- `SonHatEvacuationDirector.h`: two owned one-shot audio channels, their lifecycle helpers and a submerged-alert cooldown timestamp.
- `SonHatEvacuationDirector.cpp`: concise onboarding/interaction/result text; brief resident-impact feedback; contextual closure text; bounded audio event routing; siren escalation; storm/reveal/result/replay audio sequencing.
- `SonHatEvacuationHUDWidget.h`: one shared waypoint-card placement rule and the current readout line count.
- `SonHatEvacuationHUDWidget.cpp`: timer/access font hierarchy, spacing, card placement outside the aiming/readout regions, overlapping secondary-card suppression and case-sensitive text refresh.
- `SonHatEvacuationDirectorTests.cpp`: three focused presentation/audio tests.
- Added this handoff document.

No GrappleComponent, swing/reel/boost/targeting code, reticle, Manager/Node/Link scoring, population code/art, Blueprint, input mapping, map, root configuration or audio asset was changed. No package, commit, branch change or reset was performed. Existing dirty/untracked work remains.

`ProtectionAudit.json` compares all 37 initial module files: only the five existing source files above changed. All 53 inventoried Jam/B, Jam/A map/Blueprint/audio and root configuration files retain their hashes. All 47 inventoried staged/archive files retain size and write time. The movement and scoring source hashes are unchanged. `PlayerExperienceP0-only.patch` preserves just this pass's source changes; reverse-apply check passed. Check again against later concurrent work before using it for a rollback; do not restore whole working-tree files from Git.

## Player-facing behavior

- The nonblocking opening keeps the timer and connections first, then safe-route access, the objective, green-console/E action and controls including Space Jump/Boost. It explains that SAFE survives the flood while RISK spends a connection but fails. All numbers come from the manager/configuration, rather than hardcoded 160/90/3 values in the HUD.
- Opening guidance yields immediately after the first successful activation, or after ten seconds. If the player has not connected anything, the first-green-console instruction remains after the opening controls disappear.
- Timer uses `MM:SS`, connections use remaining/maximum and access remains residents/total. The existing bottom prompt names the connection and explains E's cost before activation; a RISK prompt explicitly says it fails and gives no safe access. Selected, submerged and exhausted states explain why E is unavailable.
- A useful SAFE connection shows `SAFE ROUTE ESTABLISHED` and `+N RESIDENTS GAIN SAFE-ROUTE ACCESS` for the existing five-second status duration. The persistent score shows the new total. A safe but disconnected edge correctly says no new access yet and asks for a connection to high ground.
- RISK activation shows `UNSAFE CONNECTION` / `1 CONNECTION USED — NO NEW SAFE ACCESS`. It retains the existing scoring semantics. Closure notifications do not replace an active route confirmation. Closure text also avoids telling a player with zero connections to choose another console.
- Waypoint cards avoid the central crosshair/grapple-state region and the persistent readout; secondary cards that would overlap an already drawn card are omitted. Terminal consoles remain visible, restrained, collision-free runtime meshes. No new large sphere, hook cosmetic or presentation actor was introduced.
- Results show residents with and without access, connections used, number of failed RISK routes, the importance of connecting to high ground, the distinction from individual evacuation and R to replay. The per-link HUD list was replaced with this compact summary; existing world route/failure visualization remains.

## Audio audit and wiring

All 13 owned SoundWaves resolved in actual CityFinal PIE, including the actual four GrappleComponent slots and nine Director slots. No asset was generated or changed.

| Sound | Existing/updated event and evidence |
| --- | --- |
| SFX_GrappleFire | Existing successful acquisition fires it. Actual BP slot resolved; acquisition exercised with the public gameplay API in rendered PIE and in the engine-mix smoke. No movement/cosmetic timing change. |
| SFX_GrappleAttach | Existing immediate attach cue with cosmetic travel disabled; delayed impact path remains unchanged if a designer enables travel. Actual slot resolved. |
| SFX_GrappleRelease | Existing active detach cue; duplicate release calls remain idempotent. Actual slot resolved and detach exercised. |
| SFX_GrappleReelLoop | Existing owned pawn component starts on reel, stops on release/clear/retry/end. Actual playing=true during reel and false after release were captured. |
| AMB_RainWindLoop | Existing preparation ambience, retained through visible flood reveal; stopped at results, recreated once on replay. Actual playing state verified. |
| AMB_EmergencySirenLoop | Same lifecycle; now scales conservatively from 0.65 to 1.4 times its configured base volume as preparation progresses. |
| AMB_FloodRumbleLoop | Existing progression from quiet to configured volume retained; now continues through flood reveal instead of cutting out just when the water arrives. |
| SFX_FloodWarning | Round start/replay alert. Actual playing state recorded. |
| SFX_FloodUrgent | Once per round at 15 seconds remaining. Actual playing state recorded. A subsequent terminal closure cannot interrupt this cue. |
| SFX_LinkSafe | Successful safe connection only. Actual playing state recorded after E. |
| SFX_LinkUnsafe | Successful RISK connection only. Distinct reference and actual playing state recorded after E. |
| SFX_TerminalSubmerged | New inactive-terminal closure. Closures detected together produce one cue; closely spaced closures have a 1.5-second cooldown. Actual playing state recorded. |
| SFX_RoundResults | Now plays when the actual results screen begins, after flood reveal, rather than eight seconds before that screen. Actual playing state recorded. |

Round feedback and alerts each reuse one owned non-auto-destroy component, so repeated E cannot stack unlimited one-shots. Replay/end explicitly stop and destroy old cue components. Three ambience loops remain during flood reveal, one result-cue component remains at results, and replay creates exactly three loops plus the opening alert, with no old result channel. Designer-assigned sounds are still respected; default fallback loading remains unchanged.

Native round cue volumes are conservative: safe/risk and urgent 0.65, opening/results 0.55, closure 0.5. Existing map ambience overrides were preserved (observed rain 0.16; siren base 0.12; rumble base 0.18). Grapple sound/reel settings were not touched. These gains and a nonclipping digital mix do not establish a good perceptual balance.

## Build and tests

- Final UE 5.7 Editor Development Win64 build succeeded in **9.03 seconds**, using the project's established Build.bat/UnrealBuildTool workflow with no Editor/cook running. `Saved/PlayerExperienceP0/Build-final.log`.
- Final full SonHat suite: **48/48 passed**: 35 clean and 13 with warnings, zero failed/not run. `Tests-final/index.json` and `Tests-final.log`. Headless viewport warnings remain; this is not a warning-free or visual-acceptance claim.
- New tests: `SonHat.PlayerExperience.ObjectiveImpactRiskAndReplay`, `WaypointCardsPreserveAimAndReadout`, `RoundAudioWiringAndBoundedReplay`.
- Tests cover starting resources/objective/controls, useful SAFE/RISK impact, 80/160 results, case-only heading refresh after reveal, replay clearing old feedback, aim/readout card exclusion at 1280x720 and 1920x1080, resolved round sound slots, bounded cue allocation, storm continuity during reveal and result/replay cleanup. Existing movement, scoring, fall/retry, flood and population tests also passed.

## Actual rendered verification

`CityValidation.json` reports passed=true, with no flow errors. `RenderedCity-final.log` and PNGs are under `Saved/PlayerExperienceP0/`. The runtime viewport was **1280x720**, using the real CityFinal map, BP pawn, controller, Director, manager and population presenter.

- Desktop E at Hub–A: 0 -> 35/160, connections 3 -> 2. The 35-resident group reported SafeAccess/true within the scheduled presentation refresh.
- Desktop E at RISK A–C: stays 35, connections 2 -> 1; unsafe cost feedback and distinct sound were observed.
- Desktop E at safe A–B: 35 -> 80, connections 1 -> 0.
- Public grapple/reel/release functions exercised the existing tether and sound lifecycle; the reel component was playing during reel and stopped after release.
- **Natural** 90-second deadline: recorded at world time 90.02; three storm components continue through reveal. Results at 98.03 show 80 with access, 80 lacking access, three connections used and one failed RISK route; only the result cue remains.
- Actual desktop R replay: 0/160, three connections, ~89 seconds, fresh opening and exactly four Director audio components. No old result cue or tether remains.

Useful reviewed images: `03_wait_safe_e.png` (opening/E prompt, clear aim area), `04_SafeRoute35.png`, `03_wait_risk_e.png` (warning before activation), `05_UnsafeConnectionNoAccess.png`, `08_ReelLoopPlaying.png`, `13_Results80With80Without.png`, `14_ReplayOpeningAndCleanAudio.png`.

Evidence limits: test viewpoints were staged, so this is not an uncoached discovery or traversal test. Space/mouse feel was not retuned. E and R used real desktop input; grapple/reel/release used public APIs. The earliest `01_OpeningObjective.png` captured the editor viewport during window handoff and is excluded as gameplay evidence. Some captures requested immediately before a staged move show the subsequent camera position; `02_FirstTerminalDiscovery.png` therefore does not prove its intended 5.5 m viewpoint. The held `03_wait_safe_e.png` is the reviewed opening/near-console view. The population's SafeAccess state was checked in live PIE; perceptual readability of its color/pose under the current bright lighting remains a human/art acceptance item.

### Actual audio output evidence

The full natural-round attempt recorded audio in memory but its WAV export used a relative engine path incorrectly; `CityValidation.json` records zero export bytes. It is not claimed as a successful recording.

A separate bounded audio smoke corrected the absolute export path and produced `Audio/CityGameplayMix.wav` (5,292,076 bytes): actual stereo engine master output, 48 kHz, 16-bit PCM, **27.56 seconds**, peak ~0.207 full scale, RMS ~0.0117, zero full-scale samples. `AudioSmoke.json` reports passed=true, with no errors; see `AudioSmoke-final.log` and `Audio/WaveReview.json`.

This audio-only fixture temporarily configured an isolated runtime Director for 18 seconds plus its normal eight-second reveal, then replayed using public APIs. It exercised real safe/risk, grapple/reel/release, urgency, terminal closure, reveal, results and replay events; it did not save the configuration or change the shipped 90-second map. The natural 90-second validation above is separate. The mix is non-silent and has digital headroom; neither waveform statistics nor component playback flags certify individual cue intelligibility, subjective balance, speakers or headphone listening.

## Composer handoff / release gates

1. Keep the existing single Director/native HUD and current movement defaults. No new actor, BP hookup or input mapping is required for these changes. Rebuild/repackage through the coordinated release workflow; this pass did not package independently.
2. `Config/DefaultGame.ini` already includes `/Game/Jam/A` and `/Game/Jam/A/Audio` in DirectoriesToAlwaysCook. Preserve that inclusion and verify all 13 SoundWaves load and play in the next packaged candidate. Existing successful packages were preserved and do not automatically include this updated DLL/source.
3. Review the existing large bright cyan city markers and exposure/emissive balance around the first small green console. They remain visible in the screenshots and were not created or altered by this pass. Adjust only the intended decorative markers/materials in the integration-owned map; do not hide real building roofs or make terminals invisible. HUD markers cannot by themselves certify that a new player identifies the physical console.
4. Use headphones for mandatory listening: fire/attach/release distinction, reel stopping, rain versus siren/rumble, SAFE versus RISK confirmation, urgency, result transition and replay without duplicates. Tune the existing Editor volume/asset settings if needed; keep movement physics frozen.
5. Remaining release gates: uncoached first-ten-second understanding and terminal discovery; human headphone balance/intelligibility; comfortable continuous traversal with readable HUD; final packaged city verification. No known native compile/test blocker remains, but these human/package gates are not passed by automation. Memory-pressure notifications appeared during Editor testing; no frame-time benchmark is claimed.

## Five-minute human acceptance

1. Start CityFinal at 1280x720 with headphones. Without explaining the rules, ask a new player for the objective, time, connections and first action. They should identify the green SAFE console and E.
2. E at Hub–A: expect +35 and 35/160 with a clear confirmation sound and existing SafeAccess reaction. Read the RISK warning before E: it consumes one connection, adds no access and sounds different. Safe A–B then gives 80/160.
3. Grapple/reel/release normally. Reticle/state text must stay readable, the reel sound must stop promptly, and the approved movement must feel unchanged.
4. Let the full timer expire. Expect escalating danger, storm audio through flood arrival, then 80 with access / 80 without, one RISK failure and R to replay. Judge the explanation, not simulated rescue/death claims.
5. R: a fresh 90-second round, 0/160, three connections, opening guidance and one set of ambience loops. Confirm no old result/feedback/reel audio persists. Also check the next coordinated packaged candidate before release.

Stop native development here and hand off for human acceptance and Composer's final map/package work.
