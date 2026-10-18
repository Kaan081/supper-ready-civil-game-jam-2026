# P0 grapple launch recovery

> Historical pass: the course below failed human acceptance and is superseded by [GrappleCourseVerification.md](GrappleCourseVerification.md). Use that report for the current saved course, runtime evidence and rollback.

Open the regenerated `/Game/Jam/A/Maps/L_RooftopTest`, press Play and click the viewport. Start at A, aim up at target 1, hold RMB for about 0.7 seconds, then release while still rising. Land on B, walk to launch 2, then repeat B -> D and D -> C. No run-jump setup is needed. LMB while attached independently reels; WASD supplies tangent steering. E/F/R behavior and evacuation/flood code were preserved (F still depends on the director's existing testing option).

## Confirmed root causes

The pre-change automated headless PIE audit inspected the actual `BP_GrappleCharacter_C` instance after BeginPlay:

- MovementStyle was `SwingAndReel`, minimum rope 350cm, slack 10cm, reel 650cm/s, speed cap 2600cm/s, release assist 0. These matched the native defaults; no Blueprint override was hiding a different grapple mode.
- CharacterMovement was Walking with velocity zero. V3 only entered Falling from the grounded path while reeling; RMB had neither a takeoff impulse nor a motor floor. Walking could discard vertical movement and brake lateral movement.
- The original nearest anchor CENTER was 121.7cm from the character and 130.8cm from the camera at the audited spawn. The next was 267.6cm / 279.2cm. These are center distances, not the logged trace impact distances. Both were far below the 350cm reel floor. The original code used character-to-impact distance for rope length; the old log reported camera-to-impact distance. Its short successful traces did not establish useful movement.
- Existing PIE logs also confirmed distant traces striking roof StaticMeshActors. Original anchors only 140cm above the roofs were poor standing swing targets.

Evidence is preserved under `Saved/GrappleP0Rollback_4e47b89_20261017`: BeforeP0.PIE.log, BeforeP0.RuntimeAudit.json and Current_BP_GrappleCharacter.t3d. The audit used actual PIE but no rendered/human game-feel judgment.

## Targeted movement changes

Suitable attachment from Walking now immediately enters Falling and supplies a one-time assisted takeoff. The kick first supplies a tangent speed floor, then modest forward and upward motor floors so shallow angles do not cancel departure. The supplied course uses steep elevated anchors, so the resulting launch is primarily tangent. Movement is still performed by CharacterMovement collision sweeps; no teleport or collision disabling is used.

Fast airborne reattachments do not receive this kick and preserve earned velocity. Low-speed airborne attachments can restart a stalled swing. RMB then keeps the existing gravity/tension swing; there is no new always-on tractor pull. Reeling, steering, speed limits, obstruction releases and landing detachment retain their existing implementation.

Swing mode rejects a target when character-to-impact distance is below `max(MinimumSwingAttachDistance, MinimumSwingRopeLength + 50cm)`. The reticle becomes orange and reads **TOO CLOSE - AIM FARTHER**. It does not silently create a rope that cannot retract. LegacyPull retains the previous close-range behavior.

Release keeps actual velocity, including its upward component at the chosen release time. No new release impulse was added. SwingReleaseAssist remains 0. Looking somewhere else before releasing does not rotate the launch.

## Tuning defaults

| Setting | Default |
|---|---:|
| Enable Attach Launch Assist | true |
| Attach Tangent Speed | 1400cm/s |
| Attach Forward Speed | 900cm/s |
| Attach Up Speed | 850cm/s |
| Airborne Assist Speed Threshold | 350cm/s total speed |
| Minimum Swing Attach Distance | 500cm character-to-hit |
| Minimum Swing Rope Length | 350cm |
| Swing Speed Limit | 2600cm/s |
| Swing Steering Acceleration | 1400cm/s² |
| Reel Speed / Acceleration | 650cm/s / 2400cm/s² |
| Swing Release Assist | 0 |
| Log Grapple Telemetry | false |

These are on the existing Grapple component in Details. Disable Enable Attach Launch Assist to compare the passive V3 behavior. Select LegacyPull for the older pull/boost/landing-assist implementation. Setting unusually low speed caps can reduce the requested takeoff floors; the cap always wins.

Enable Log Grapple Telemetry for event-only records: acquisition style and camera hit distance; attach character hit distance, rope/minimum length, grounded state, before/after velocity and actual assist delta; reel input transitions; peak attached speed; release/detach velocity and normalized direction; detach reason. PeakSwingSpeed and LastDetachReason also remain inspectable without logging. Nothing logs every frame by default.

## Course and verification

Only the existing `Saved/SonHatTools/build_l_rooftop_test.py` generates the geometry changes. Roofs and evacuation configuration are unchanged. Three anchors sit 1500cm above standing launch origins and 300cm ahead toward the next roof: approximately 15.3m from the character, with clear upward aim rays and roof-sized landing areas. Numbered fixed labels identify A -> B -> D -> C. Existing grapple/network/generated actor tags are retained. Other traversal anchors are elevated as well. No new city/platform framework was introduced.

The generator was run through Unreal and saved only L_RooftopTest. Before/after comparison of nodes, populations, links/endpoints, terminal positions, tags, budget and flood settings passed: `Saved/Automation/GrappleP0CourseGeneration.json`. A byte copy of the previous map is backed up before regeneration; no binary map bytes were edited externally.

An automated headless PIE traversal used the real Blueprint pawn, normal grounded walking between launch markers, grapple start/reel/release calls, gravity and collision. It used no teleports or direct launch velocity writes. All three standing launches and B/D/C landings passed. At 0.3 seconds each launch traveled about 4.6m and rose 2.1m. Reel pulses shortened the radius about 24cm while retaining lateral movement. Release velocity discontinuity was exactly zero. Attached peak speeds were about 1606-1615cm/s. See `Saved/Automation/GrappleP0PIECourse.json` and `Saved/Logs/GrappleP0PIECourse.log`.

This is measured headless PIE traversal with scripted aim/controls, not confirmation of rendered reticle readability or subjective fun. A human manual playtest is still required. UE5.7 Development Editor build succeeded; all **25 SonHat automation tests passed**, including eight grapple tests and the existing evacuation/flood regressions. Seven evacuation tests have headless "No game viewport was found" warnings. Standing takeoff is also tested at 30/60/120Hz; close-target rejection, independent reeling, exact release and LegacyPull acquisition passed. Reports are at `Saved/Logs/GrappleP0Build.log` and `Saved/Automation/GrappleP0/index.json`. No packaged Shipping/cook verification was performed.

## Short manual check

1. Open L_RooftopTest and Play. Without WASD or jump, aim up at target 1 and hold RMB. Movement should be immediate; within 0.3s the pawn should clearly be airborne above the roof.
2. Tap/hold LMB while attached and steer with A/D. Check that the rope shortens while sideways control remains. Release LMB to keep the current radius.
3. Release RMB while rising (~0.6-0.8s is a useful first attempt). Land on B, walk to marker 2 and repeat toward D, then marker 3 toward C. Release direction should follow movement even if you look sideways. Holding longer lets tension bend the trajectory into a pendulum arc rather than continuing a straight launch.
4. Aim at an anchor from less than 5m: expect the explicit TOO CLOSE hint and no fake successful attachment. Check E at a terminal, the existing flood round, F if enabled, and R from results.

## Exact rollback

Pre-pass source and generator copies are in `Saved/GrappleP0Rollback_4e47b89_20261017`. P0-only forward and rollback patches compare against those copies, including your earlier V3 changes, rather than resetting to Git HEAD.

With Editor/Live Coding closed, from project root:

```powershell
git apply --check Saved/GrappleP0Rollback_4e47b89_20261017/P0-only.rollback.patch
git apply Saved/GrappleP0Rollback_4e47b89_20261017/P0-only.rollback.patch
```

Rebuild Editor. To restore the earlier test geometry, rerun the restored generator through Unreal; do not overwrite map bytes manually. If you later edit these files and patch checking fails, do not force it: inspect the backed-up copies and preserve the newer work. The fast designer fallback is MovementStyle=LegacyPull. No branch switch/reset, commit or push is needed.
