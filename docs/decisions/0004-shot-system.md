# 0004: Shot system, swing phases and the practice ball machine

**Date:** 2026-11-15 · **Status:** proposed (Agent 3) · Implements GDD 4, 5, 6.3 and 7

## Where things live

- **Engine-free logic in `PadelCore/Shots/`** (namespace `Padel::Shots`): the selector (GDD 4), timing grader (5.1), aim mapper (7), resolver (5.2, 5.3), player movement (6.1, 6.3) and the swing controller that ties them together. The AI (Agent 4) drives the same `FPlayerShotController` through `RequestAiShot`, so AI errors come from the same model as a human's. A request must arrive at least `AiSwingLeadS(charge, offset)` before the ideal moment, or it releases with less charge than it asked for. Full-charge requests get `AiChargeLeadS` of extra lead, so a requested smash still has its full charge when the predicted moment drifts during the windup.
- **Tuning in `Data/`**: `Shots.json` (one row per shot, named by `ShotTypeName()`), `ShotModel.json` (the shared contact model), `Movement.json`, `Players.json` (one practice player for now; the slice roster is Agent 7's), `BallMachine.json` and `PracticeBall.json`. The native tests read these same files, so CI checks the real numbers.
- **Unreal side in `Padel/`**: row structs in `Data/` (same field names), `APadelPlayerPawn` (Enhanced Input to an `FPlayerInputFrame`), `APadelPracticeDirector` (fixed-step practice loop), `APadelPracticeGameMode` and the `APadelShotDebugHUD` overlay.

## The handoff to the ball engine

A contact produces an `FShotIntent`: origin, target (already scattered), speed, spin (x = topspin + / slice −, y = side spin, rad/s), an apex height for lobs, the timing grade and the shot type. The engine solves the launch and emits `ball_hit`. The shot system reads the ball through `IBallPort` (current state, last hitter, a deterministic `Predict`, `ApplyShot`, `ServeToss`). Agent 2's `FMatchSimulation` gets an `IBallPort` adapter in a follow-up; until then practice uses `FPracticeBall`, a small stand-in (gravity and simple bounces, no spin in flight) that gets deleted when the adapter exists.

## Interpretations of the GDD

- **Speed by charge:** "max speed × charge" is read as a lerp from the shot's minimum to its (Power-scaled) maximum, so a tap still has the shot's minimum pace.
- **Ideal contact moment:** the predicted moment when the ball comes closest to the ideal contact point (0.8 m to the side, 0.3 m in front, at the shot's preferred height), before its second bounce on the player's half. Ties go to the earlier chance.
- **Early releases** meet the ball at the ideal moment (graded early, pulled across). **Late releases** hit at the release, judged for reach and position where the player set up at the ideal moment, so a late release within the window is a late hit rather than a whiff.
- **Shadow swing:** a release more than two early/late windows before the ideal moment, or with no ball coming, makes no contact.
- **Height fallbacks:** Y on a ball below the bandeja's band plays the groundstroke; B above the lob's band plays a bandeja. RB + Y on the backhand is a bandeja ×1.4 error; a full-charge Y without being set is a bandeja ×1.5 error.
- **Serve:** A/J tosses, then A/J swings (RB = kick). The toss drops from `ServeTossHeightM` so it bounces once into the serve's contact band.
- **Consistency** scales the whole random scatter (×1.15 at 1 to ×0.75 at 99); the early/late pull is the non-random part.
- **Return of serve:** the shot's rating is blended 50/50 with Return.

## Swing phases (for animation, Agent 6)

`FPlayerShotController::GetPhase()`, `GetPhaseTimeS()`, `GetCharge()` and `GetButton()` drive the animation state machine. All times come from data.

| Phase | Starts | Ends | Length | Animation |
|---|---|---|---|---|
| `Idle` | recovery ends | a shot button is pressed | — | ready stance, run/sprint by speed |
| `Windup` | press | release, or the late edge of the window | up to `ChargeTimeS` (0.6 s) | backswing, scrubbed by `Charge` 0→1; it holds at full |
| `Forward` | an early release | the ideal moment | 0 to ~0.2 s | forward swing; **the contact frame is the end of this phase** |
| contact | the `Hit`, `Whiff` or `ShadowSwing` outcome event | — | instant | contact frame. On a late release there is no `Forward` phase: contact is the release tick |
| `FollowThrough` | contact | | `FollowThroughS` of the shot played | follow-through, early/late/perfect variant from the grade |
| `Recovery` | follow-through ends | | `RecoveryS` | back to ready; no new swing until `Idle` |

So the contact frame lines up with the tick where `Tick()` returns an outcome. For a perfect hit that is the ideal moment; for an early grade it is still the ideal moment (the forward swing started early); for a late grade it is the release. The variant to play comes from `FShotIntent::TimingGrade`, and the swing type from `FShotIntent::ShotType` (`Mishit` uses the button's family). The split-step hop is `FPlayerMotion::IsSplitHopping()`, `SplitHopS` long. Movement during a swing is slowed to the shot's `SwingMoveMult` (planted feet), which keeps the feet from sliding while the arms swing.

## Practice mode

`APadelPracticeGameMode`: the placeholder court, the broadcast camera, the human's player on Team A's half and a ball machine on the far half. Drills come from `BallMachine.json` (groundstrokes, volleys, overheads, off the glass), plus `mixed` and `serve`. Partner-command buttons drive the machine in practice (there is no partner): D-pad left/right (3/4) change the drill, up (1) pauses, down (2) feeds now. The overlay shows the drill, swing phase, charge, time to the ideal moment, the last shot's grade, offset, speed, scatter and target, and draws the aimed target, scatter circle and landing target in the world.
