# SON HAT — Living Rooftop Population P1

## Level designer: minimum setup

Existing Population Presenters upgrade automatically after the rebuilt Editor loads: leave **Representative Mesh** empty to use the new human pose set. No existing maps or Blueprint assets were changed. Do not add another presenter to a network that already has one.

For a new map:

1. Add one **Son Hat Population Presenter**. Assign the existing evacuation **Director**. Auto Find Source also works with one unambiguous Director; use matching Network Tag or an explicit reference in multi-network levels.
2. Positive-population nodes receive six figures each by default. A=35, B=45, C=60, D=20 therefore show **24 representatives for 160 residents**. Visual density never changes graph population.
3. Check editor previews. Default Group Offset is `(250,200,0)` relative to each node. One-time WorldStatic traces project each foot onto the roof. Failed traces omit the affected figures and report the issue in Status Message.
4. Override awkward placements using **Building Placements**: Node, optional rooftop Anchor, Offset, Facing Yaw, optional Representatives. Offset is relative to Anchor when assigned, otherwise Node. Feet origin is Z=0; height is 170 cm before raised hands. Use **Rebuild Presentation** after changing geometry or anchors. For noncolliding roofs disable Project Feet To Floor and place feet explicitly.
5. Press Play. Prepare a useful safe connection: every newly connected building changes pose and color immediately. Wait for flood/results, then use the existing **R** replay: the group poses and idle timing reset without duplicates.

No Level Blueprint, input mapping, NPC controller, navmesh or additional HUD is needed. The presenter never starts rounds, activates links, changes scoring, or replaces the manager. Its source is the Director's public round manager, including manager replacement on replay. Standalone Manager is also supported, without Director flood proximity.

## Readable states

| State | Appearance | Meaning |
|---|---|---|
| Waiting | Amber shirts, arms relaxed, gentle staggered breathing and turning | No verified safe route yet |
| Safe access | Green shirts, raised hand, short 1.25-second positive reaction | Manager verifies a safe route to the hub |
| Flood threatened | Pulsing orange shirts, both hands raised, faster turning | Flood near feet; still no safe route |
| Stranded | Pink/red shirts, hand to forehead and open palm, subdued movement | Final graph has no safe-route access |

Skin, hair, trousers and shoes remain recognizable through the shirt-color changes. Pose plus text communicates state without relying solely on color. Safe buildings retain green access even when flood is nearby. Unsafe selected links and disconnected safe branches cannot turn a building green.

Figures remain on their own rooftop. Safe access is preparation of a route, **not a claim that anyone physically reached the hub**. Stranded is absence of route access, **not simulated deaths**. There is no rescue/death animation or casualty accounting. Water can overtop stationary figures; status labels remain above water.

## Art and tuning

Original project-owned assets in `/Game/Jam/A/Population`:

- `SM_Citizen_Waiting`, `SM_Citizen_Safe`, `SM_Citizen_Threat`, `SM_Citizen_Stranded`: four static poses, each 508 triangles, one material slot, feet-origin, no collision. Recognizable faceted head/face/hair, torso, arms, separate legs and shoes. Raised hands extend above the 170 cm head height.
- `M_CitizenState`: unlit instancing-compatible material. Palette RGB preserves body details; palette alpha blends the `Color` state parameter mainly into clothing.
- `T_CitizenPalette`: tiny 64×4 palette.
- `Source/BuildPopulationArt.py`: reproducible original mesh/palette generator for Unreal's Python Editor environment. Run through a full Editor after Slate initializes, not a NullRHI Python commandlet. It skips existing assets by default. `REIMPORT_POSES=True` explicitly rebuilds only these four owned meshes; intermediate OBJ/TGA files go to Saved/LivingPopulation/ArtSource.

Designer controls:

- **Representatives Per Building**: 6 default, maximum 16; per-building override -1 inherits, 0 hides figures but retains label. Max Total Representatives caps visuals only.
- **Columns / Spacing / Proxy Height**: defaults 3 / 90 cm / 170 cm. Keep enough rooftop clearance for raised arms.
- **Animate Representatives**: disable for static state poses. No motion in editor preview.
- **Safe Reaction Duration**: 1.25 seconds. **Animation Distance**: 4500 cm; distant groups retain their state pose without idle transform updates.
- **Human Waiting/Safe/Threat/Stranded Mesh**, **Human Material**: replace the pose set with project art using the same origin, height and single material convention. Material needs the configured Color vector parameter. The native Human Material is independent of old Population Material instance overrides, so existing presenters gain the new skin/clothing treatment automatically.
- **Representative Mesh**: optional single custom static mesh overriding the pose set. Receives subtle idle/state reaction transforms and Population Material colors, but retains its authored silhouette. Clear it to restore the four default poses. Missing native human assets retain the earlier primitive fallback.
- Existing label size/height, state colors, Flood Warning Clearance, placement/floor settings and optional world-space network board remain available. No duplicate screen HUD is introduced.

## Gameplay checks

The existing 3-connection test graph remains authoritative:

- Initial or unsafe A–C alone: **0/160** with safe-route access.
- Unsafe A–C, Hub–A, A–B: **80/160** with access; C/D's **80** lack access at results.
- Hub–A, A–B, Hub–C: **140/160** with access; D's **20** lack access.

Replay clears safe/threat/final states and restarts idle animation. The 24 representative figures do not become 160 NPCs. No Character actors, pathfinding, physics, collision or aim-ray blockers are added.

## Cost and verification

Default presentation uses **4 instanced mesh components / 24 whole-human instances** (12,192 triangles), replacing 8 components / 144 primitive parts. One shared 20 Hz timer updates state, labels and nearby transform batches; actor Tick is disabled. Each nearby building submits one batch, with no per-person Tick or heap allocation for the capped transform array. Feet translations remain fixed: yaw and tiny breathing scales operate around the authored feet origin. Instances cast no shadows, generate no overlaps, and contribute no navigation.

Focused tests cover graph choices, visual counts versus population, unsafe/disconnected routes, final counts, Director flood and replay, floor placement, collision/navigation, caps, source cleanup/ambiguity, native mesh bounds/material slots, idle movement with planted feet, pose transitions and replay animation reset.

Local build/test/PIE evidence is under `Saved/LivingPopulation` (Git-ignored). The rendered PIE helper uses an unsaved copy of L_RooftopTest with a temporary inspection camera/presenter and shortened round. It exercises public manager activation and Director replay, not physical keyboard/grapple play. No map is saved. See the delivery for final verified results.

Remaining integration checks: appearance/placement in the final artist level, packaged cook, human E/R/grapple playtest and a formal frame-time benchmark during traversal. The short automated offscreen animation comparison is only a local regression check, not a shipping performance guarantee.

## Verified P1 result — 2026-10-17

- UE5.7 Win64 Development Editor build **Succeeded** (187.54 seconds), including both changed Population translation units. Evidence: `Saved/LivingPopulation/Build.log`.
- Full SonHat automation: **37 passed, 0 failed, 0 not run** (28 clean successes, 9 successes with existing headless-viewport/configuration warnings). All **5 Population tests passed**; its Director test has the existing headless `No game viewport was found` warning. Report: `Saved/LivingPopulation/Tests/index.json`.
- Rendered automated **1280×720 PIE passed**, with the native new assets and animation, not the earlier art-only compatibility probe. Captures inspected: `Waiting.png`, `SafeReaction.png`, `GrappleDistance.png`, `Threatened.png`, `Stranded.png` in `Saved/LivingPopulation`. These scene HighResShot images omit UMG.
- Initial/unsafe result **0/160** unchanged. Hub–A and A–B produced **80/160** immediately. Flood threatened the unconnected D group; final result was **80 with access / 80 lacking access** with C/D stranded poses. Replay through the public Director API restored **0/160**, waiting meshes and the exact initial transforms. The test uses public activation/replay calls, not physical E/R input.
- Every snapshot retained **4 components / 24 instances**, planted feet, and only **one Character actor** (the player). No duplicate preview figures, population Character actors or figure collision were introduced. New focused automation also checks navigation/collision behavior and native mesh bounds.
- Local animation-off/on steady windows: 121 frames each; median **16.667 ms** in both, p95 **16.667 / 16.681 ms**. This capped, stationary offscreen check found no obvious regression; it does not establish traversal performance. Raw samples: `PIE_After.json`.
- Protected source/config/map hashes matched at completion, including L_RooftopTest and L_SonHat_Playable. No shared source, maps, Blueprint assets, Jam/B or project settings were written. See `Scope.json`. No commits or branch operations were performed.
- The final PIE log contains two generic `Condition failed` startup messages before PIE, also seen in prior project runs. Their source remains unidentified; no population assertion, Python exception or crash occurred in the completed run.

Rollback evidence: `Saved/LivingPopulation/Before` contains pre-pass Population files; `PopulationP1.patch` contains this pass's source/doc delta and was reverse-apply checked without applying it. The new assets are isolated in Content/Jam/A/Population. Do not roll back over later concurrent Population edits. Saved evidence is Git-ignored and must be copied explicitly if sharing with the other developer.
