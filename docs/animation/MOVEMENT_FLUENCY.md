# Movement fluency

Why the players looked robotic after phase 2 of the movement realism plan, and what we are doing about it. The full analysis (two reference videos: a console tennis game used only as a reference for feel, and real pro padel) and the plan Caio approved on 2026-11-16 are in the project's "Fluid player movement plan" doc. This file is the in-repo summary that code comments point to.

## The diagnosis

The movement simulation (`FPlayerMotion`, `Data/Movement.json`) is fine. The pose on top of it was stiff:

- Four looping walk and jog clips blended by direction on one phase: no starts, stops, plants or reversals.
- No swing clips: the arm reaches for the ball with IK, the body doesn't turn or shift weight.
- The body turned at a constant 540°/s, all in one piece.
- 0.08 s linear blends, so state changes pop.
- No lean, no pelvis motion, an invisible split step, a frozen ready stance.

## The rule: the simulation leads, the pose follows

Movement stays code-driven in `PadelCore` (deterministic, used by the AI and the soak gate). The animation adapts to it: springs, warping, foot locking and motion matching on the simulation's trajectory. Root motion is not used.

## The steps

| Step | What | Status |
|---|---|---|
| **F1 body dynamics** | Facing on a spring with the head and chest leading the hips; trunk lean into the acceleration; the pelvis sinking on brakes and landings with the feet planted; a visible split-step hop; a light ready-stance bounce. Model: `FPlayerAnimModel::StepBody`; pose: `PadelBodyDynamics`. Data: `Data/Animation.json` (`Facing*HalfLifeS`, `FacingMax*TwistDeg`, `Lean*`, `PlantDrop*`, `PelvisHalfLifeS`, `SplitHopHeightM`, `SplitLand*`, `ReadyBounce*`). | this PR |
| F1 inertial blending | Inertialization inside `FAnimNode_PadelPlayerPose` for every discrete switch (state, swing in and out, clip crossfade). | next |
| F1 warping and feet | Orientation and stride warping, foot locking with leg IK. | next (needs the Mac) |
| F2 the shot as a movement | An early unit turn driven by time to contact, a set step, the body in the procedural swing, recovery steps. | after F1 |
| F3 motion matching | Epic's Game Animation Sample locomotion database (starts, stops, pivots, strafes) on the Motion Matching node, as a stand-in until mocap. Chosen by Caio: use it now. | parallel with F2 (Mac) |
| F4 padel mocap | Phase 3's session, plus the takes this analysis adds (approach with prep, swing on the move, recovery, reversals, lunges, two ready idles), into the same database. | phase 3 |

## Checks

- Native (`Tests/Native/PlayerAnimBodyTests.cpp`, `Anim.Body*`): the springs settle on their targets with no jump in turning speed; the head and chest lead a turn within their caps; lean follows the acceleration's direction and is capped; braking and landing sink the pelvis; the split step peaks at `SplitHopHeightM`; the ready bounce only plays standing ready in a rally.
- Unreal (`Padel.Animation.BodyDynamics`): on the skeleton, a pelvis drop keeps the feet within 1 cm, the hop lifts them, a chest twist turns the shoulders and a forward lean moves the head toward the net.
