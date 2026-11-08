# SON HAT

**A 48-hour Unreal Engine 5.7 gameplay engineering prototype**  
**C++ · Unreal Engine · Blueprints · Gameplay systems · Automation tests**  
**Civil Game Jam, 16–18 October 2026 · Team: Supper's Ready (2 people)**

> **Project status — unfinished game jam prototype.** We did not complete the intended jam delivery. This repository preserves the engineering work, not a polished or self-contained game release. In particular, the gameplay code is available, but the published integration branch is missing some external-actor and third-party environment dependencies required to reproduce the final city.

## The idea

In a city threatened by rising floodwater, the player traverses rooftops using a grapple mechanic and registers isolated groups for rescue before their rooftop beacons become inaccessible.

The **four-beacon rescue prototype** models groups A–D with **35, 45, 60 and 20 residents** (160 total). Players have **three signal charges** and a **150-second default preparation period** in rescue mode. Choosing which groups to register creates a constrained route/time decision; the maximum possible registered population is **140/160**. The score represents **registered rescue signals**, not simulated physical evacuation, rescue arrivals or deaths.

## Engineering work

| System | What was implemented | Source |
|---|---|---|
| Grappling movement | Surface acquisition, attach/reel/swing/release, obstruction handling, movement recovery and presentation | [`GrappleComponent.cpp`](Source/UEIntroProject_1/GrappleComponent.cpp) · [`GrappleSwingPhysics.h`](Source/UEIntroProject_1/GrappleSwingPhysics.h) |
| Rescue round | Timed phases, beacon validation/activation, signal budget, flood closures, result snapshots and replay | [`SonHatEvacuationDirector.cpp`](Source/UEIntroProject_1/SonHatEvacuationDirector.cpp) · [`SonHatEvacuationManager.cpp`](Source/UEIntroProject_1/SonHatEvacuationManager.cpp) |
| Player feedback | Interaction guidance, grapple reticle, objective/status HUD and population representation | [`SonHatEvacuationHUDWidget.cpp`](Source/UEIntroProject_1/SonHatEvacuationHUDWidget.cpp) · [`SonHatPopulationPresenter.cpp`](Source/UEIntroProject_1/SonHatPopulationPresenter.cpp) |
| Automated checks | Native Unreal automation tests for grapple, evacuation, rescue-state invariants, invalid configurations and replay | [`GrappleComponentTests.cpp`](Source/UEIntroProject_1/GrappleComponentTests.cpp) · [`SonHatEvacuationDirectorTests.cpp`](Source/UEIntroProject_1/SonHatEvacuationDirectorTests.cpp) |

The central design constraint was **keeping gameplay state authoritative**: a single manager handles registration/budget/results, while the director coordinates player interaction and presentation. The flood visuals and population proxies do not claim to be a physical flood or civilian evacuation simulation.

## What is actually verified?

| Evidence | Status | Limit |
|---|---|---|
| C++ systems and native automation test definitions | Present in the `integration/son-hat` source tree | Source inspection alone does not prove runtime behavior |
| Unreal Editor C++ build | **Passed** on the portfolio snapshot machine (UE 5.7) | Requires local Epic toolchain |
| SON HAT automation (`RunTests SonHat`) | **57/57 passed** before November 2026 map edits (local); **not re-run** after Phase 2b/2d map promotion | Re-run in Editor after pulling this branch |
| Manual Phase 2d smoke test (promoted to this branch) | **User confirmed** — Roof A spawn/walk, grapple, visual cleanup, `building_3`, beacon, replay | No published capture or packaged build |
| `Win64_Development_RescueFix2` | Earlier handoff package per [`SUBMISSION_HANDOFF.md`](SUBMISSION_HANDOFF.md) | Not this portfolio snapshot |
| Clean clone → full playable jam city | **No** | World Partition `__ExternalActors__/Jam` and third-party environment packs are local-only |
| Final game jam completion | **Not achieved** | This is a technical case study, not a completed jam entry |

## Repository layout and reproducibility

- **Default play map (portfolio snapshot):** `/Game/Jam/A/Maps/L_SonHat_CityFinal` — four-beacon rescue integration over `/Game/Jam/B/Maps/L_City`.
- **November 2026 map snapshot (this branch):** Phase 2b rescue spawn on Roof A `(4570, -5570, 1400)`; Phase 2d `L_City` cleanup (hide non-essential BasicShape/GA debug meshes, collision preserved). Locally, `building_3` World Partition data was aligned to the art pin (624 external actor files, **434 stale MAIN-only cells removed** — not stored in git).
- [`integration/son-hat`](https://github.com/kaanbartubozkurt/supper-ready-civil-game-jam-2026/tree/integration/son-hat) is the merge target for gameplay integration; `main` is an earlier starter snapshot.
- **UE 5.7** · **Git LFS** for tracked Unreal binaries (`Content/Jam/…` only — **58** Jam assets tracked; **zero** `__ExternalActors__/Jam` paths in git).
- **Not redistributable in this repo:** `Content/Fab`, `Content/ModularBuildingSet`, `Content/GrassLandscape`, and the bulk of `Content/__ExternalActors__/Jam`. A fresh clone **cannot** reproduce the full jam city without your own licensed/mirrored content; it **can** still be used to review C++, run automation against test maps, and open the integration map if you supply missing environment data locally.
- **Do not** commit marketplace/Fab assets to a public remote. Obtain packs under their licenses and merge locally only.

For technical review, start with the C++ files linked above and the following engineering notes:

- [`SonHatRescueSetup.md`](Source/UEIntroProject_1/SonHatRescueSetup.md) — rescue-mode rules, ownership, geometry requirements, historical test results.
- [`SonHatReleaseGate.md`](Source/UEIntroProject_1/SonHatReleaseGate.md) — player-experience risks, regression verification and remaining acceptance gates.
- [`SonHatMapIndependenceAudit.md`](Source/UEIntroProject_1/SonHatMapIndependenceAudit.md) — collision, grapple and map-integration assumptions.
- [`SUBMISSION_HANDOFF.md`](SUBMISSION_HANDOFF.md) — frozen Windows package decision and its explicit limits.

Some earlier documents describe a **legacy six-link/90-second evacuation prototype**. The later four-beacon rescue mode uses different rules; do not conflate historical test stages with the final map's intended gameplay.

## Team and scope

**Kaan Bartu Bozkurt:** primarily C++ gameplay and grappling systems, rescue-state logic, HUD/interaction behavior, testing and gameplay integration.  
**Recep:** primarily city environment, level art and building layout.  
**Shared:** level integration, Git/LFS coordination and game jam delivery work.

This repository is a record of a short, collaborative engineering effort. It should not be used to imply sole authorship of team art or third-party assets. No blanket license for externally sourced game assets is granted by this README.

## What I would improve next

1. Separate redistributable, original gameplay code from licensed environment assets, and resolve the final city's World Partition actor dependencies.
2. Re-run UE 5.7 Win64 build and the complete automation suite on a reproducible snapshot; publish scoped evidence.
3. Provide a short, genuine gameplay capture and a verified runnable package if distribution rights permit.

**Takeaway:** the jam game did not reach its intended finish line, but the time-boxed C++ gameplay, state-management and validation work is available for technical review.
