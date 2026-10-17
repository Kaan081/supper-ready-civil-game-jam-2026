# SON HAT evacuation network

This is a fictional, pre-installed rooftop connection system for a preparedness game. Do not present improvised rooftop ropes as real-world flood evacuation advice. Link safety is authored explicitly by the designer, never inferred from building height.

## Files and ownership

- `SonHatEvacuationNode.h/.cpp`: building population and hub designation; scene root for placement.
- `SonHatEvacuationLink.h/.cpp`: two endpoint references, designer-authored `bFloodSafe`, interaction-terminal scene root, read-only activation state and `OnActivationChanged`.
- `SonHatEvacuationManager.h/.cpp`: round authority, configured graph registration, exclusive link ownership, budget, timer, graph evaluation and frozen results.
- `SonHatEvacuationTests.cpp`: seven development-only Unreal automation tests using transient worlds.
- `SonHatEvacuationSetup.md`: this guide.

Only the manager writes link activation state. A link cannot belong to two running managers. Configuration is editable in Details and read-only to Blueprints; configure before the round starts. Start snapshots the node/link lists and budget, removes duplicates, and ignores null/foreign-world entries with warnings. Nodes/endpoints/risk/population are intended to remain fixed during a round. Destroyed actors are safely ignored on the next evaluation; spent activations are never refunded.

Edges are undirected. BFS starts at exactly one hub and traverses only active, valid, flood-safe links owned by this manager. A visited set prevents counting civilians twice, including in cycles. Hub residents, if configured, count as already safe. Counts clamp at the signed 32-bit maximum. Missing or multiple hubs yield zero safe population and `InvalidNetwork` activation failures. Invalid individual edges do not disable otherwise valid routes.

## Build and load

1. Close Unreal Editor and Live Coding before compiling these new reflected classes. Do not use Live Coding to introduce them.
2. Build `UEIntroProject_1Editor`, `Win64`, `Development` against UE 5.7. No module, plugin, target, configuration, grapple, map or existing asset changes are required.
3. Open the project. In the Content Browser enable **Settings > Show C++ Classes**, then expand **C++ Classes > UEIntroProject_1**. Drag `SonHatEvacuationNode`, `SonHatEvacuationLink`, and `SonHatEvacuationManager` into a test level as described below. The Place Actors search is another option after the classes load.

The local validation command uses temporary flags to avoid this machine's UBA cache permission error and MSVC D8050 debug-record error. It changes no project configuration. Debug symbols are disabled for this validation build:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' `
  UEIntroProject_1Editor Win64 Development `
  '-Project=C:\Users\KAAN BARTU BOZKURT\Documents\Unreal Projects\UEIntroProject_1\UEIntroProject_1.uproject' `
  -WaitMutex -NoHotReloadFromIDE -NoUBA -NoDebugInfo `
  '-ini:Engine:[/Script/WindowsTargetPlatform.WindowsTargetSettings]:bWriteSarif=False'
```

These actors contain scene roots but no runtime meshes, collision, VFX, HUD, or automatic visual markers. Use the World Outliner to select them and the transform gizmo to position them. Designers can later create Blueprint subclasses with terminal meshes and risk labels; this implementation creates no assets.

## Exact sample level

Place five node actors, naming them in the World Outliner as follows. Place their origins on the corresponding rooftops. Populate **Son Hat > Evacuation** in Details:

| Actor label | Civilian Count | Is Safe Hub |
| --- | ---: | --- |
| Hub | 0 | true |
| Building_A | 10 | false |
| Building_B | 20 | false |
| Building_C | 30 | false |
| Building_D | 40 | false |

Place six link actors. In each link's **Son Hat > Evacuation** Details section assign `Node A`, `Node B` using the actor picker and set `Flood Safe`:

| Actor label | Node A | Node B | Flood Safe |
| --- | --- | --- | --- |
| Link_HB | Hub | Building_B | true |
| Link_BC | Building_B | Building_C | true |
| Link_CD | Building_C | Building_D | true |
| Link_HA | Hub | Building_A | true |
| Link_AC_RISK | Building_A | Building_C | false |
| Link_AB | Building_A | Building_B | true |

Place each link actor's origin at an accessible interaction terminal. Activation uses **3D distance from the player actor origin to this terminal**, not distance to the node, a segment between nodes, or the camera crosshair. Keep terminals at least 650 cm apart with the default 300 cm interaction radius so choices are unambiguous; move terminals toward the player's capsule-center height if rooftop elevation makes interaction difficult. Connections need not be drawn to function.

Place **one** manager, name it `EvacuationManager`, and set:

- **Son Hat > Network > Nodes**: five entries, Hub, Building_A, Building_B, Building_C, Building_D.
- **Candidate Links**: six entries, Link_HB, Link_BC, Link_CD, Link_HA, Link_AC_RISK, Link_AB.
- **Son Hat > Preparation > Max Activations**: 3.
- **Preparation Duration**: 120 seconds (use 15 for a quick test).
- **Interaction Distance**: 300 cm.
- **Auto Start Preparation**: true.

The timer begins on the manager's BeginPlay. For a menu or delayed start, disable Auto Start and call `Start Preparation` once when ready. Repeat starts return false and never refill the budget. Zero/negative/nonfinite duration ends immediately. The clock uses game time, so pause and time dilation apply. Stop and restart PIE for a fresh round.

## Exact E-key Blueprint hookup (Level Blueprint, no project input changes)

1. Select `EvacuationManager` in the World Outliner, then open **Blueprints > Open Level Blueprint**.
2. Right-click in the graph and select **Create a Reference to EvacuationManager**. Dragging the selected actor from the Outliner into the graph also creates the reference.
3. Right-click empty graph space, search **E**, and choose **Keyboard Events > E**. Use the **Pressed** execution pin so holding E does not activate multiple links.
4. Drag from the manager reference and add **Try Activate Nearest Link**. Connect `E > Pressed` to its execution input.
5. Add **Get Player Character**, index 0, and connect its return value to **Interactor**. The manager reference supplies **Target**.
6. Drag from **Return Value** and create **Switch on ESonHatEvacuationActivationResult**. Connect the function's execution output to the switch. For playtesting, connect its branches to **Print String** with messages such as `Success: connection prepared`, `BudgetExhausted: no connections remaining`, `NoCandidateInRange: approach a terminal`, `AlreadyActive: already prepared`, `PreparationEnded: preparation finished`, and `InvalidNetwork: check hub configuration`. The other branches identify missing actors, invalid endpoints, wrong ownership, or distance failures.
7. **Out Link** is the chosen terminal. On success, reading its **Flood Safe** value lets a Blueprint show a danger message for the risky connection. Bind a future visual to **On Activation Changed** or read **Is Active**. Avoid overlapping terminals: the nearest inactive candidate wins, and exact ties follow the configured array order. If only active terminals are in range, the result is `AlreadyActive`. Malformed/foreign-owned nearby terminals return their specific failure instead of silently selecting a farther one.
8. Compile the Level Blueprint. Click the PIE viewport to give it keyboard focus. If the character's E event already consumes input, uncheck **Consume Input** on that event or put the same manager call in the possessed Character Blueprint using a manager reference. Do not change grapple logic or input mappings.

For result feedback without a widget: drag from the manager reference and add **Bind Event to On Round Ended**. Connect **Event BeginPlay** to the bind node and use its Event pin to create a matching custom event with integer **Safe Population** and **Total Population** pins. Connect those pins to **Format Text** (`Evacuated {Safe} / {Total}`), convert its output to string and call **Print String** with a long duration. For zero-duration rounds, bind before calling `Start Preparation` with Auto Start disabled; ordinary BeginPlay ordering between actors is unspecified.

## Blueprint API

| Blueprint API | Meaning |
| --- | --- |
| Get Remaining Time | Live game-time countdown, clamped at zero |
| Get Remaining Connections | Unspent activations, never negative; zero before start |
| Estimated Safe Population | Population with complete safe routes now |
| Total Population | Unique valid-node population, frozen at round end |
| Final Safe Population | Final outcome; meaningful once Game Ended is true |
| Has Started / Game Ended | Round state |
| Configuration Status | NotStarted, Ready, NoValidNodes, MissingHub, MultipleHubs |
| Try Activate Nearest Link | Result enum plus selected Out Link; no resource spent on failures |
| Try Activate Link | Targeted activation with the same checks, including distance |
| On State Changed | Start, successful activation, countdown refresh every 0.1 seconds, and end |
| On Round Ended(Safe Population, Total Population) | Exactly once after results are frozen |
| Finalize Preparation | Optional early evaluation, useful for a designer test button |
| Link: Is Active / Flood Safe / Has Valid Endpoints | Activation and designer risk information |
| Link: On Activation Changed(bool) | Activation notification, after manager state is consistent |

Timer notifications can arrive up to one update interval late, but interaction checks the actual deadline before accepting any link. Final results stay frozen after the round ends, even if actors are subsequently removed. The final result describes graph reachability; it does not simulate civilian travel time or NPC evacuation.

## Two distinct playtest outcomes

Use the sample graph above. Set a comfortable duration for traversal; shorten to 15 seconds only when terminals are reachable in that time. Optionally call `Finalize Preparation` from a temporary test event to evaluate early.

1. Start PIE. Prepare **Link_HB**, **Link_BC**, **Link_CD** in that order. Estimated safe population should be **20**, **50**, **90**. Remaining connections should be **2**, **1**, **0**. Wait for expiry. Result: **90 / 100**, A disconnected.
2. Stop and restart PIE. Prepare **Link_HA**, **Link_AC_RISK**, **Link_CD**. Estimated safe population should be **10**, **10**, **10**. The risky edge still consumes a resource, and the safe C-D subgraph has no safe route to the hub. Wait for expiry. Result: **10 / 100**, with the same three activations spent.

Additional acceptance checks: with HA active, attempt `Try Activate Link(HA, Player)` again and verify `AlreadyActive` with unchanged budget; try a fourth inactive link for `BudgetExhausted`; press E at a terminal after expiry for `PreparationEnded`; remove the hub from the manager's Nodes array before PIE for `MissingHub`, zero safe population, and `InvalidNetwork`. Clear a link endpoint for `InvalidLink`; no crash and no resource spent. Duplicate nodes/cyclic paths do not inflate population.

## Automated validation

Verified locally on Windows with UE 5.7 on 17 October 2026, branch `integration/son-hat`, base HEAD `5a5041f`: Win64 Development Editor build succeeded using the command above; all **7 tests passed, 0 warnings, 0 failures, 0 skipped**. Generated evidence is in `Saved/Logs/SonHatEvacuationBuild.log`, `Saved/Logs/SonHatEvacuationTests.log`, and `Saved/Automation/SonHatEvacuation/index.json`. Existing grapple file hashes were unchanged. No existing source, Blueprint, map, configuration, Build/Target file, branch or history was edited.

The test filter is **SonHat.Evacuation**. In Unreal Editor, open **Tools > Debug > Session Frontend**, select **Automation**, search that filter, and run all seven tests. Headless execution is also supported with the Editor closed:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'C:\Users\KAAN BARTU BOZKURT\Documents\Unreal Projects\UEIntroProject_1\UEIntroProject_1.uproject' /Engine/Maps/Entry `
  -unattended -nop4 -nosplash -NullRHI -NoSound `
  '-ExecCmds=Automation RunTests SonHat.Evacuation' `
  '-TestExit=Automation Test Queue Empty' -log
```

The tests cover direct/chain/cyclic connectivity, disconnected nodes, both sample outcomes, risk budget consumption, null and unregistered actors, invalid endpoints, missing/multiple hubs, interaction range, duplicate activation, depleted budget, deadline rejection before a timer callback, automatic/zero-duration expiry, frozen results, destroyed nodes, and exclusive manager ownership/cleanup. They do not validate visual presentation, an E-key Blueprint asset, player traversal times, or a packaged game. Those need the level hookup above.
