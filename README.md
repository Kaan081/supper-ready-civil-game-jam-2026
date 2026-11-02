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
| Unreal Editor build and **55/55** SON HAT tests | **Historically reported** on 18 October 2026 in [`SonHatRescueSetup.md`](Source/UEIntroProject_1/SonHatRescueSetup.md) | These results were **not re-run** in the 2 November 2026 cloud audit; original test logs are local/ignored |
| `Win64_Development_RescueFix2` | Documented in [`SUBMISSION_HANDOFF.md`](SUBMISSION_HANDOFF.md) as the human-tested playable fallback | The package is **not published here** and was not independently tested in the latest audit |
| Clean GitHub clone builds and plays the complete final city | **Not established** | Some World Partition external actors and licensed environment assets are absent from the integration branch |
| Final game jam completion | **Not achieved** | This is a technical case study, not a completed jam entry |

## Repository layout and reproducibility

- [`integration/son-hat`](https://github.com/Kaan081/supper-ready-civil-game-jam-2026/tree/integration/son-hat) contains the frozen gameplay implementation and the `L_SonHat_CityFinal` map definition.
- `main` is an **earlier starter-project snapshot**, not the final gameplay implementation. Browse the integration branch when assessing the C++ work.
- The Unreal project targets **UE 5.7** and uses **Git LFS** for Unreal binary assets.
- The integration snapshot intentionally does **not** include all `Content/__ExternalActors__/Jam` files or third-party `Content/Fab`, `Content/ModularBuildingSet` and `Content/GrassLandscape` content. A clean clone must **not** be advertised as a turnkey playable build.
- Environment packs must only be obtained and used under their applicable licenses. **Do not copy marketplace/Fab assets from another installation into a public repository.** An asset-safe, reproducible portfolio branch remains future work.

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
