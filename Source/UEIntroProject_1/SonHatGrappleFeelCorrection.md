# RMB swing / LMB reel feel correction

17 October 2026, current local `integration/son-hat` working tree. Existing uncommitted work retained. Only grapple source/tests and this note changed; no maps, Blueprints, evacuation/flood code, targeting, obstruction handling or release code edited.

## Measured cause

RMB was **already fixed-length**. `GrappleSwingPhysics::Step` changes `RopeLength` only under `bReel`, which comes from manual reel request or the independently configured LMB key. The live BP pawn used SwingAndReel, RMB attach and LMB reel, with ReelSpeed 650 cm/s and ReelAcceleration 2400 cm/s².

The initial assist is **one-time**, not a continuous RMB motor. On the first shallow course target it supplies velocity `(608.15, 663.44, 850)` cm/s from standing, yielding **1186.6 cm/s inward** and **352.7 cm/s tangential** initially. It is therefore predominantly inward on this target despite starting with tangent assistance; the forward/up speed floors are responsible for that direction. Looking at the distance alone makes this launch look like reeling.

During the measured first 1.25 seconds, RMB rope length stayed **1278.18 cm** while character-to-attachment distance fell from **1268.18 to about 246 cm**. All sampled RMB reel/tension accelerations and landing-assist velocity deltas were **zero**. This approach is earned launch momentum plus gravity/changing radial direction, rather than a continuous radial assist or shortening constraint. Rope tension still operates normally when taut; it can supply inward constraint acceleration without changing rope length.

Old LMB did shorten the rope, but its radial winch assistance only acted near tautness. The character was already approaching faster than the 650 cm/s retraction, leaving a slack rope. Raising retraction alone to 1300 cm/s also produced **zero sampled radial winch acceleration** on this first approach, so that tuning-only candidate was not selected.

## Targeted change

Added one setting: **Reel Pull Speed = 1600 cm/s**. While LMB is held and the character is outside MinimumSwingRopeLength, an arcade motor accelerates inward toward that target, including during slack travel. It shares the existing **2400 cm/s² ReelAcceleration** budget with the winch. It does not brake faster earned inward speed, overwrite tangential velocity, exceed the existing total speed limit, or continue powering inside the 350 cm minimum radius.

Actual rope winding remains independently controlled by **ReelSpeed = 650 cm/s**, with the existing minimum length and acceleration. RMB alone receives no new motor and does not shorten the rope. Releasing LMB stops winding/motor immediately, holding the newly prepared rope length without braking earned velocity. RMB release remains the existing momentum-preserving code.

The initial tangent/forward/up launch settings remain **1400 / 900 / 850**. Free-surface targeting, same-actor obstruction, swept correction, air control, landing assistance and LegacyPull were not changed.

Setting **Reel Pull Speed = 0** restores the previous tension-only LMB behavior. It also provides the simplest human A/B comparison without rebuilding. No new RMB assistance was added: requested B (fixed rope, current initial assist) already matches the original RMB behavior, so optional C was unnecessary for this correction. Human feedback can still justify later tuning of the initial assist direction; this pass deliberately preserves it.

## Comparison and evidence

Automated rendered PIE, existing `L_RooftopTest`, actual BP_GrappleCharacter/CharacterMovement, standing start, public attach/reel methods; LMB-equivalent input starts at 0.25 seconds. No per-frame velocity writes or modified map geometry. Values below are nearest recorded samples, not a human game-feel certification.

| Trial | Rope start → 1.25 s (cm) | Inward velocity around 0.5 s (cm/s) | Distance around 0.5 s (cm) | Maximum sampled reel acceleration (cm/s²) |
|---|---:|---:|---:|---:|
| A: original RMB only | 1278.18 → 1278.18 | 967 | 728 | 0 |
| A: original RMB + LMB, pull speed 0 | 1278.18 → 710.82 | 969 | 733 | 0 |
| B: fixed RMB, pull setting 1600 but no LMB | 1278.18 → 1278.18 | 969 | 733 | 0 |
| B: RMB + LMB, pull speed 1600 | 1278.18 → 710.83 | 1569 | 653 | 2400 |

All four trials retained attachment through the measurement window. Peak observed speed with new LMB was about **1609 cm/s**, below the existing 2600 limit. All four releases had **zero measured velocity discontinuity**. Native deterministic tests prove RMB outputs identical with pull setting 0 or 1600 at the same inputs, including slack, taut and previously shortened rope cases; small differences between rendered samples reflect frame/sample timing.

Baseline: **35/35 existing SonHat tests passed**. Final: **36/36 passed**, 27 clean and 9 with existing/headless or intentional configuration warnings. This includes existing grapple collision, frame-rate, native mouse, standing launch, release, targeting and obstruction tests, plus `SonHat.Grapple.Feel.FixedRMBAndIntentionalLMB` at 30/60/120 Hz. Final UE5.7 Editor build succeeded in **26.20 seconds**. No human playtest or packaged-build certification is claimed.

Evidence under `Saved/RMBReelFeel/`:

- `Measurements.json` / `.csv`, `Summary.json`, `Measure.log`: final A/B traces and sampled acceleration sources.
- `Before-measure.log`, `Before-measurements.json` / `.csv`: original behavior and rejected 1300 cm/s winding-only candidate. Launch assist in the original log's `Attach` entries is authoritative; the first harness's JSON vector-delta field used a live property view and is flagged invalid. The final harness copies vectors before measuring.
- `Build.log`, `BaselineTests/index.json`, `FinalTests/index.json`.
- `Before/`, `Pass.forward.patch`, `Pass.rollback.patch`: pass-only copies/patches preserving earlier uncommitted work. Review before applying over any later edits. No patch was automatically applied to revert work.

Existing **Log Grapple Telemetry** now includes throttled **4 Hz** `Motion` samples while attached, plus existing launch/reel/release events. It remains off by default. Fields distinguish distance from authoritative rope length, radial velocity before/after the step, winch speed, reel acceleration, tension acceleration and landing delta velocity. Accelerations are velocity-control contributions in cm/s², **not physical forces in newtons**, and are reported before final combined-speed limiting. Gravity is integrated separately by CMC; post-move collision/correction remains unchanged.

## Short human A/B procedure

1. Open `L_RooftopTest`, Play from its normal PlayerStart, click the viewport. Keep **Movement Style = SwingAndReel**, ReelSpeed 650, ReelAcceleration 2400 and launch assistance unchanged. Use the same distant elevated anchor and standing start for each trial.
2. **A:** before attaching, use the PIE Eject control, select the spawned BP_GrappleCharacter's GrappleComponent, set **Reel Pull Speed = 0**, then repossess. Change only the runtime instance; no Blueprint asset save is necessary. Hold RMB alone, then repeat from Stop/Play while pressing LMB after about a quarter second. RMB's initial approach is expected and is not auto-reeling.
3. **B:** Stop/Play restores the new native default **1600** unless you authored a separate override. Confirm the value on the live component before attaching. Repeat RMB only, then RMB + LMB from the same standing start. RMB should feel the same; LMB should produce a clearly stronger approach while the rope visibly shortens.
4. Repeat A/B with the same sideways WASD input to assess retained lateral steering. Release **LMB only** while keeping RMB: winding/pull must stop and the shortened rope length must hold. Release RMB next: the player must carry the actual earned trajectory. Repeat on a higher anchor and an airborne chain before accepting the feel.
5. If the pull is too strong, lower only Reel Pull Speed and repeat. **0** is the old LMB fallback; it does not disable initial launch assistance. Leave optional telemetry off after comparison. Human judgment, landing/chaining and rooftop timing remain the final feel checks.

Exact product files changed: `GrappleComponent.h`, `GrappleComponent.cpp`, `GrappleSwingPhysics.h`, `GrappleComponentTests.cpp`, and this new report. Local ignored helper: `Saved/RMBReelFeel/MeasureFeel.py`. No Jam/B content, other systems, map assets, project configuration, commits or branch changes.
