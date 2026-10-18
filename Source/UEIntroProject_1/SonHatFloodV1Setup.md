# Flood gameplay V1

Open the existing `/Game/Jam/A/Maps/L_RooftopTest`, press Play (Selected Viewport), and click the viewport. The existing director gets the new defaults automatically; no new actor, Blueprint wiring, input context, generator run or map save is required.

## During the round

The director's persistent, collision-free runtime water plane is now visible during preparation. It rises from world Z=200cm to Z=1650cm over the manager's existing 90-second round. The height curve is `lerp(start, final, progress^exponent)` with exponent 1.2, giving slower initial rise and faster late rise. Water uses the existing Engine BasicShape material tinted blue; it is an opaque illustrative surface, not fluid simulation. XY bounds still fit the loaded network with 1000cm padding, or use Flood Half Size when auto-fit is disabled.

The same code-driven UMG HUD shows water elevation, dry/unprepared and submerged terminal counts, the next closure deadline, a focused terminal's E/locked state, timer/budget and safe-route access. The final 15 seconds turn the timer/warning red. Existing terminal labels show a 12-second closure countdown. Submerged terminals get purple markers and raised labels above the surface; aiming at the visible marker gives the locked prompt, while interaction range and closure still use the original link actor origin. Selected connection lines, unsafe failures and building access markers retain their existing meaning.

A terminal closes when `water Z >= link actor origin Z`. All discovered terminals are vulnerable by default. Add entries under **Terminal Flood Rules** on the director to set **Vulnerable=false** for specific waterproof/protected terminals. This changes terminal availability only; it never changes the link's authored Safe/Unsafe rating. Duplicate overrides use the first matching entry.

Closure is latched until replay. E and direct `Director.Interact()` calls check current water height, including calls between ticks. A closed terminal returns the existing `InvalidLink` result with an explicit SUBMERGED/no-budget-spent status. Activation does not reach the manager. Integrations should use the director's Interact entry point; calls directly to manager activation APIs do not apply the director's flood access gate.

The manager remains authoritative for activation budget, selected state, reachability, population estimates and final scoring. Prepared links remain selected after their terminals submerge. Safe links still provide safe-route access; unsafe links still fail according to their existing authored rules. No NPC evacuation is simulated.

At expiry, the existing flood-reveal phase continues from the actual water height to final elevation + 100cm over the configured reveal duration (8 seconds by default), then shows results. F, when the existing testing option is enabled, starts the same reveal continuously from the current height. R from results creates a fresh manager, restores budget/selections and resets water, closure latches and audio-warning state. Existing player replay positioning settings still apply.

## Director tuning

| Setting | Default / meaning |
|---|---|
| Preparation Duration | 90 seconds, existing manager-owned deadline |
| Connection Budget | 3, unchanged |
| Enable Rising Flood | true; false keeps water hidden during preparation |
| Flood Start Elevation | 200cm absolute world Z |
| Flood Final Elevation | 1650cm absolute world Z at preparation end |
| Flood Rise Exponent | 1.2; 1 linear, >1 faster late, clamped 0.25-4 |
| Flood Reveal Surge | 100cm additional rise during the locked reveal |
| Flood Reveal Duration | 8 seconds, existing phase |
| Enable Terminal Flooding | true; independently disable closure for visual-only water |
| Terminal Flood Rules | Per-link vulnerability; no overrides needed for the test level |
| Terminal Flood Warning Seconds | 12 seconds |
| Color Submerged Terminal | Purple |
| Fit Flood To Network / Flood Half Size | Existing XY sizing controls |
| Flood Warning Sound / Flood Urgent Sound | Optional USoundBase assets, unassigned by default; played once at round start/final 15 seconds |

Start/final elevations, exponent, surge and round duration are captured on round start. Replay applies changed Details settings. Reversed elevations clamp to a stationary flood. The legacy **Flood Rise Distance** field remains serialized for old levels/generator compatibility but is unused: use the new absolute elevations and Flood Reveal Surge. Director world Z no longer positions the waterline.

For another level, set elevations relative to that level's real terminal origins. Keep some options dry through the full preparation period, or explicitly protect them. Begin below all intended starting options, and leave at least the warning lead time plus traversal time before lower closures.

## Existing rooftop timing

A read-only Unreal commandlet loaded the actual L_RooftopTest and checked its serialized terminal heights against the new defaults:

| Terminal rooftop | Terminal Z | Warning starts | Closes |
|---|---:|---:|---:|
| D: C-D and Hub-D | 1382.5cm | 63.93s elapsed | 75.93s elapsed (~14.07s left) |
| A: Hub-A and unsafe A-C | 1585cm | 74.63s elapsed | 86.63s elapsed (~3.37s left) |
| C: Hub-C | 1685cm | No preparation closure | Dry through 90s |
| B: A-B | 1787.5cm | No preparation closure | Dry through 90s |

The final reveal ends at 1750cm, below the high-ground hub roof. Hub-C may become submerged during the reveal, when all selections are already locked. Previously prepared access is preserved. Safe plans retain their original scores: Hub-A + A-B + Hub-C gives 140/160 safe-route access; Hub-A + Hub-C + C-D gives 115/160 if C-D was prepared before D closed. Unsafe A-C is not required by either plan.

No generator changes or map regeneration were necessary. The audit verified the water material's Color parameter and all 11 existing GrappleAnchor actor tags without saving assets. Output: `Saved/Automation/FloodV1RooftopAudit.json`. The existing generator's old FloodRiseDistance assignment has no effect on the new waterline; new properties use their C++ defaults unless overridden in the Editor.

## Build and automated checks

- UE5.7 **Development Editor** build succeeded: `Saved/Logs/FloodV1Build.log`.
- `Automation RunTests SonHat.Evacuation`: **17 passed, 0 failed**. Seven tests report the existing headless "No game viewport was found" warning. Report: `Saved/Automation/FloodV1/index.json`.
- Three focused flood tests cover interpolation/monotonicity, exact threshold and pre-closure warnings, protected/high terminals, direct interaction before Tick, unchanged budget and prepared access, persistent component reuse, final graph scores, early F/reveal continuity, native HUD construction/input visibility and R reset/re-activation.
- The five pre-existing evacuation Director tests, bootstrap fallback tests and manager graph tests passed unchanged.
- The grapple component and manager/link source hashes were checked against pre-task snapshots. The rooftop map file hash was checked after the read-only audit.
- **PIE was not played or visually verified. No packaged Development/Shipping build or cook was performed.** These are native automation and read-only level checks, not a claim of rendered gameplay acceptance.

## Short manual playtest

1. Open L_RooftopTest, press Play and focus the viewport. Check visible blue water below the roofs, water elevation in the existing HUD and unchanged RMB/LMB/WASD grapple controls.
2. Prepare a low safe terminal with E before its deadline, then reach D. Leave one D terminal unprepared. At about 64s elapsed, check its countdown; at about 76s, check its purple submerged state. Stay within interaction range, aim at its visible purple marker and press E: expect SUBMERGED and no budget spent. Confirm the already prepared link remains solid/selected with its safe access retained.
3. At 75s elapsed, verify the red final-15-seconds warning. Check A's closure near 87s and B/C availability through the deadline. At 90s, check the existing unsafe route failures and final safe-route totals during the reveal. No NPC rescues should be claimed.
4. After the 8-second reveal, press R. Check water returns to Z=200cm, terminals reopen, budget is 3 and selections/access reset. Press E on a formerly closed terminal to confirm it works again. Repeat with a different safe plan and compare 140 versus 115 residents with safe-route access.
5. Optional: enable the director's existing Allow Early Finish For Testing, replay, then press F partway through. Check water continues from its current height without snapping down and proceeds to results/R replay.

## Limits and scope

Water has no physical collision, swimming, buoyancy, destruction or fluid simulation. Player flood-contact recovery/time penalty is deferred; it needs a validated safe checkpoint and penalty policy alongside the new grapple movement. Falling below the surface alone does not end the round or remove score. Audio is silent until suitable optional sound assets are assigned. The surface is a simple opaque mesh, with no waves/refraction. Terminal rules use actor-origin Z rather than roof collision, and submerged beacons float visually above their original interaction positions for readability.

Only the evacuation Director, evacuation HUD, Director tests and this guide changed. Grapple files, graph algorithms, input/Blueprint assets, Jam/B, project configuration, generator and maps were preserved. Pre-task owned-file snapshots and protected hashes are in `Saved/FloodV1Before_4e47b89_20261017`.
