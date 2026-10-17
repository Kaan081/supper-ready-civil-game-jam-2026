# SON HAT: rooftop round integration

`ASonHatEvacuationDirector` joins the existing grapple pawn and actual rooftop level to the existing evacuation network. It generates no city, floor, pawn, grapple anchor, or candidate route. It spawns only its transient round manager and lightweight presentation components. The fallback bootstrap remains a separate test harness for `L_EvacTest`; do not place both in one active level.

## Minimal final-level checklist

1. Keep the level's existing first-person/grapple GameMode, pawn, PlayerStart and grapple anchors. The director never replaces, possesses, teleports or moves that pawn. Keep the network actors loaded at round start (including in World Partition / Data Layers).
2. Place one `SonHatEvacuationNode` at each participating rooftop and one at high-ground safety. Keep node origins about 100 cm above roof geometry so the drawn connection lines are visible. Set **Civilian Count**, **Building Name**, and **Is Safe Hub**. Exactly one node is the hub. Names are optional but recommended for readable runtime labels.
3. Place a `SonHatEvacuationLink` terminal at each intended preparation station. Assign its **Node A**, **Node B**, and **Flood Safe**; optionally set **Connection Name**. Its actor origin is the station, independent of its endpoints. Put origins about 100 cm above the accessible rooftop floor, close to standing capsule-center height. The director draws its terminal marker and route labels. Safe/unsafe classification is authored, never derived from rooftop height or visual water height.
4. Space terminals over **550 cm** apart with the default **275 cm** interaction distance, or use walls/placement to keep choices distinct. Avoid terminals under opaque roof geometry: the default camera-to-terminal visibility trace must be clear. Test with the actual pawn/camera. Routes are undirected; preparing a connection from one rooftop can establish a route between two other endpoints.
5. Add **one `SonHatEvacuationDirector`**. Leave **Auto Discover Network** and **Auto Start** enabled. No arrays or Blueprint wiring are needed in a level with just this network. Do not also place a standalone manager for these links: the director owns creation/configuration of its manager before BeginPlay can auto-start it.
6. Set **Preparation Duration = 90**, **Connection Budget = 3**, and **Flood Reveal Duration = 8** (defaults). Position the director's **world Z at the desired final visual waterline**. Its XY water bounds automatically fit the network plus a 10 m margin; turn off **Fit Flood To Network** to set manual half-size around director XY. Water rises from 800 cm below that line. Place it where the blue sheet/grid can be seen between buildings. It has no collision or gameplay safety logic.
7. Press **Play**, click the viewport, use the existing grapple controls to reach stations, and press **E** when the HUD identifies the intended route. At the deadline, changes lock, the blue flood reveal rises, failed selected routes break red, and access results stay visible. After the reveal, **R** resets this network for a fresh round **at the player's current position**. Use Stop/Play for an identical PlayerStart-based traversal trial. No input mapping or Level Blueprint changes are required.

For multiple unrelated evacuation actors in a map, give this network's **nodes AND links** a shared Actor Tag (e.g. `SonHatRound`) and set the director's **Network Tag** to the same name. Alternatively disable discovery and fill its explicit node/link lists. Discovery is sorted by actor path for deterministic ties. A malformed layout or another manager claiming these links produces a visible setup error; R retries after correction. An active fallback bootstrap is rejected instead of taking over its links.

For rapid iteration, change the preparation duration to 5-10 seconds, or enable **Allow Early Finish For Testing** for **F** to start the same flood reveal early. Disable that option for the jam round. The 90-second clock uses game time, including pause and time dilation. No runtime actor discovery occurs during a round; replay takes a fresh network snapshot.

## Meaningful safe-route choices

Example layout to author on the existing rooftops, with a three-connection budget. There is no automatic level generation:

| Building | Population |
| --- | ---: |
| Hub | 0 |
| A | 35 |
| B | 45 |
| C | 60 |
| D | 20 |

| Terminal | Endpoints | Safety |
| --- | --- | --- |
| Hub-A | Hub / A | Safe |
| A-B | A / B | Safe |
| Hub-C | Hub / C | Safe |
| C-D | C / D | Safe |
| A-C | A / C | Unsafe |
| Hub-D | Hub / D | Safe |

Put A/B on one rooftop branch and C/D on another. Make the high-population branch require longer grapple travel or harder station access; keep an easier all-safe route available. Space stations across the real level so visiting them is an actual traversal decision within 90 seconds. Keep the risky A-C option clearly orange and explicitly labeled UNSAFE; do not make it a prerequisite for a good result.

- Select **Hub-A, A-B, Hub-C**: **140/160** residents have safe-route access (A, B, C).
- Select **Hub-A, Hub-C, C-D**: **115/160** have access (A, C, D), providing another safe population/travel trade-off.
- Select **Hub-A, A-C unsafe, C-D**: **35/160** have access. The selected A-C connection fails; the intact C-D subgraph has no safe hub path.

All three plans spend three connections. Unsafe connections are unnecessary for either good outcome. The real travel-time trade-off depends on authored terminal positions, grapple tuning and a timed playtest; the system cannot guarantee balance for an arbitrary city layout.

## Presentation and result meaning

- **Dashed gray**: unselected safe candidate. **Dashed orange**: unselected unsafe candidate.
- **Solid cyan**: selected safe connection during preparation. **Solid orange**: selected unsafe connection, with the risk shown before selection.
- **White terminal ring + E HUD prompt**: exact nearby visible target. Repeating E on a selected station spends no extra resource.
- **Green building**: complete safe hub access. **Cyan**: hub. **Gray building**: no safe route during preparation; **red at flood**: still no safe access.
- At flood, **green selected routes** remain intact with hub access; **yellow selected routes** remain intact but disconnected from the hub; **broken red route + X + FAILED AT FLOOD label** identifies selected unsafe routes. Unselected routes remain dashed and are never treated as failures of a prepared route.
- A blue rising sheet/grid illustrates flood arrival. It does not simulate water, set link safety, kill the player, or move civilians. Graph evaluation is frozen at the manager's deadline, before this visual reveal.

The Canvas HUD reports preparation time, connection budget, estimated access population, exact highlighted connection/safety/cost, activation feedback, phase, and final residents with/without access. Results mean **safe-route ACCESS**, not completed evacuation, rescues, travel time, or crowd capacity. Fictional pre-installed technology must not be presented as real-world improvised rooftop evacuation advice.

## Ownership, replay and verification

The existing manager remains the only writer of link activation and route results. `HasSafeRoute(Node)` exposes its BFS reachability cache; presentation never implements a second graph. `OwnsLink(Link)` detects conflicting integration without resetting someone else's manager. Replay destroys only the director's manager, allowing that manager's EndPlay to release/reset its links, then configures a new manager with the same placed actors and full budget. External nodes, links and player state survive director destruction. A phase delegate and Blueprint-callable `RestartRound` / `Interact` are available for later UI integration.

The existing bootstrap is preserved; its final HUD wording now says access rather than claiming people were evacuated. Development automation filter **SonHat.Evacuation** includes the existing manager/bootstrap tests and four director tests. The new tests exercise 90-second expiry, reveal phases, raw E/R input, safe alternative plans, risky failure, wall occlusion, snapshots, tag discovery, foreign ownership, malformed references, and cleanup of externally placed networks. Test geometry is transient and does not alter any map asset. These tests do not constitute a visual PIE/grapple traversal or city-balance playtest.

Presentation currently uses development debug drawing, TextRender components and a Canvas delegate. Shipping builds may omit debug lines, flood sheet and HUD; prepare production presentation before relying on a Shipping package. Single local player only. Auto discovery requires actors loaded at round start. Replay keeps the player's current location and existing grapple state. Runtime edits to population/safety/endpoints during a round are unsupported; frozen result labels assume those design settings stay fixed. No code here edits grapple, maps, materials, input configuration or existing Blueprints.

## Local build workflow

Verified on Windows / UE 5.7, 17 October 2026, base HEAD `176ea90`: **Win64 Development Editor build succeeded; 13/13 evacuation automation tests passed, 0 test warnings, 0 failures, 0 skipped.** This includes the two fallback bootstrap tests. Grapple source hashes matched their initial values. The branch, maps, existing Blueprint assets, Jam/B content, configuration and Build/Target files were not changed.

Close Unreal Editor and Live Coding first; this task never terminates them. The validated workflow uses UE 5.7's installed MSVC toolchain with temporary UBA/SARIF/debug-symbol overrides and a stable temporary directory to avoid cached sandbox paths. These are process/build arguments, not project configuration edits:

```powershell
$taskBuildTemp = Join-Path (Get-Location) 'Saved\SonHatEvacuationBuildTemp'
New-Item -ItemType Directory -Path $taskBuildTemp -Force | Out-Null
$env:TEMP = $taskBuildTemp
$env:TMP = $taskBuildTemp
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UEIntroProject_1Editor Win64 Development `
  '-Project=C:\Users\KAAN BARTU BOZKURT\Documents\Unreal Projects\UEIntroProject_1\UEIntroProject_1.uproject' `
  -WaitMutex -NoHotReloadFromIDE -NoUBA -NoDebugInfo -NoUBTMakefiles `
  '-ini:Engine:[/Script/WindowsTargetPlatform.WindowsTargetSettings]:bWriteSarif=False'
```

Run this from the project root in a temporary PowerShell session. Generated evidence: `Saved/Logs/SonHatEvacuationDirectorBuild.log`, `Saved/Logs/SonHatEvacuationDirectorTests.log`, and `Saved/Automation/SonHatEvacuationDirector/index.json`. The automation filter is `SonHat.Evacuation`. Actual rooftop traversal, station timing, camera readability and the visual flood reveal still require a PIE playtest with the restored grapple; no such PIE verification is claimed here.
