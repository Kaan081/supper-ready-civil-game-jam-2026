# Rescue migration preflight — 18 October 2026

**Status update, 15:34 TRT:** the complete brief and exclusive Source ownership were subsequently supplied. The four-beacon Source migration is now implemented, Editor-built and tested (55/55). See `SonHatRescueSetup.md` for the current handoff. The assessment below records the earlier preflight; final-city placement and packaging still await the exact new city pin and walkable-roof evidence. Unreal ownership has been released to Composer.

## Decision

**CONDITIONAL GO for the minimal four-beacon migration. Geometry-dependent implementation and a RescueCandidate release are blocked.** This is a source/data audit, not final-city gameplay acceptance. No Editor, build, cook, shared C++ edit or map placement was performed during this preflight.

The referenced detailed FINAL RESCUE GAME brief was not included in the latest message. The proposal below implements only the four-beacon requirements actually supplied. Source ownership and Composer's geometry handoff remain outstanding.

## Current evidence

- Branch: `integration/son-hat`; substantial existing changes are preserved.
- Imported city pin: `1a8d9a3735d3c115d38660e8142cb88b03615a09`.
- Import summary reports 4,646 files, zero LFS skips and zero protected-path conflicts. Local SHA256 values for `L_City.umap`, `building_6.umap` and `building_7.umap` match that pin's import manifest. This does not verify all 4,646 files or their gameplay collision.
- The existing CityFinal builder still creates five nodes and six links, with three terminals on A, two on C and one on D. It uses measured positions from the previous `5b2db81` snapshot when live actors cannot be found.
- The builder removes generated actors before resolving rooftops. It must not be run for this migration with incomplete geometry evidence.
- `discover_l_city.py` filters labels by `B_Building`, `B_SafeCenter`, `GA_` and `PlayerStart`, plus selected tags. Untagged `building_6`/`building_7` instances can be omitted. Its output has actor origins, not walkable roof measurements. Missing A/C labels therefore do not establish missing geometry; present B/D labels do not establish safe collision.
- Current builder settings are 90 seconds, flood start 150 cm, final 1,800 cm and exponent 2.8. Existing closure estimates use the old placements and cannot certify the new city or a 150-second round.
- Prior FinalRevision and ComprehensionRecovery package directories exist. Neither is overwritten by this proposal. Prior automated/rendered results do not accept this new city snapshot.

## Smallest implementation after the gates pass

Keep `ASonHatEvacuationManager` as the only state and scoring authority. Reuse the existing Node and Link classes; do not introduce a rescue score counter or separate beacon actor framework.

Configure a star graph: a zero-population Hub and four population nodes A=35, B=45, C=60, D=20. Four SAFE links connect Hub directly to each group. Each link's existing interaction actor becomes that group's beacon, positioned on its own roof. The Manager's three-activation budget, `TryActivateLink`, `IsActive`, `HasSafeRoute`, deadline and frozen final score remain authoritative. Its existing AlreadyActive rejection prevents a second charge for the same group.

Add an opt-in rescue presentation mode to the Director; keep the legacy mode as the default so the six-link demo remains functional. Validate the rescue topology before starting: exactly one zero-population Hub, four positive-population groups and four unique direct SAFE links, one per group. Reject mixed legacy or duplicate configurations with a clear setup error.

Reuse the existing console, waypoint, objective, HUD layout, interaction, flood closure, audio, recovery and replay machinery. Change presentation text in rescue mode only:

- Goal: register rooftop groups for rescue before flood closes their beacons.
- HUD: time, signal charges, residents registered / total, and persistent next available unregistered group.
- Prompt: `E — REGISTER ROOF A: 35 RESIDENTS — 1 SIGNAL CHARGE`.
- Immediate feedback: `ROOF A REGISTERED +35 — 35/160 — 2 CHARGES LEFT`.
- Population label: `RESCUE REGISTERED` or `AWAITING REGISTRATION`, with existing threat presentation.
- Results: registered and unregistered resident counts; no claims of simulated escape, rescue arrival or death.

The graph still models safe access. In the strictly validated star configuration its connected set is also the registered set; presentation must explain registration rather than imply a physical evacuation simulation. A completed registration stays counted when water subsequently closes its terminal; closure prevents late activation rather than revoking completed registration.

With three charges, all 160 cannot be registered. The three-group totals are A+B+C=140, B+C+D=125, A+C+D=115 and A+B+D=100. Traversal distance and flood deadlines must make the choice meaningful. If the missing detailed brief requires a 160/160 outcome with only three one-group charges, that rule needs reconciliation before implementation.

Expected C++ ownership: Director header/implementation/tests and PopulationPresenter wording/tests. Reuse the existing HUD widget layout; no Manager, grapple physics, character architecture or population art changes are needed. Composer retains ownership of the map generator and map integration unless explicitly transferred.

Rollback: rescue mode off restores legacy presentation; preserve the six-link generator/placement data separately before creating four-link data. Snapshot the affected dirty files before editing, and revert only this pass's changes if necessary. Do not use a Git reset or overwrite concurrent edits.

## Geometry handoff required from Composer

Use the exact pin above and the current imported assets. Supply evidence for **all four roofs**, not just A/C, plus a valid Hub reference. Labels may differ from old integration labels; do not rename or fabricate buildings just to satisfy a lookup.

For each A/B/C/D group provide:

1. Actual actor/component path, owning loaded level/LevelInstance, asset and world transform. Identify which real roofs `building_6`/`building_7` represent, if applicable.
2. Measured world-space floor point and normal from collision, clearly distinguished from actor origin and character capsule center. Confirm the floor is walkable for the current CharacterMovement configuration.
3. A clear standing capsule at the proposed player/interaction location, using the actual pawn dimensions and collision profile. Confirm beacon approach and recovery positions are unobstructed.
4. Supported rooftop footprint for the representative population group and a separate accessible beacon. Check the actual figure offsets against the roof rather than validating only their central anchor.
5. Current flood closure elevation at each beacon, water start/final elevations, proposed 150-second closure times, and an initial rooftop that remains usable during onboarding. No reuse of the previous layout's calculated heights.
6. A rendered view and a short walk/landing check on the current CityFinal collision. Record the tested snapshot and timestamp. Structural actor counts are insufficient.

Before any destructive generation/save, verify the complete handoff and matching snapshot. Missing, stale or failed checks must stop placement rather than silently use `MEASURED_STAND`. Do not edit Jam/B or add surrogate platforms/collision.

## Verification and release gates

Before any Source edit or Unreal operation, confirm that Composer is not editing the same files or running an Editor/build/cook. An empty process list alone does not establish ownership.

After coordinated implementation, run focused rescue tests: single activation, repeat activation, three-charge cap, each three-group total above, invalid topology rejection, deadline/closed-beacon rejection, frozen results and replay reset. Keep existing legacy tests. Use synthetic test geometry only for automation, never as a substitute for city acceptance.

On the verified city, render and test actual E input, changing score and charges, registered beacon and group states, persistent objectives, flood closure, free-surface grapple, airborne boost, fall recovery and R replay. Test a full round. Explicitly distinguish automated, rendered and human acceptance.

Only then create a separate RescueCandidate Windows output. Recheck E, grapple, fall and replay in its real EXE. Preserve all previous archives. A structurally valid map or passing suite alone is not permission to describe the city as accepted.

Suggested deadline gates, subject to actual handoff/build time: resolve geometry and Source ownership by about 15:30; freeze the candidate implementation by about 16:30; reserve the remaining window for cook, EXE verification and human acceptance. If verification cannot finish safely, ship the known-good FinalRevision fallback rather than continue the migration into the submission window.

## Status at handoff

Only this preflight document was added. No runtime code, map, asset, configuration or package was changed. Build/tests/rendered PIE were not run for this preflight. No RescueCandidate exists yet. Resume only after current geometry evidence, Source ownership and the complete gameplay brief (if it adds requirements) are available.
