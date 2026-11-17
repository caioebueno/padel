# Movement fluency

Why the players looked robotic after phase 2 of the movement realism plan, and what we are doing about it. The full analysis (two reference videos: a console tennis game used only as a reference for feel, and real pro padel) and the plan Caio approved on 2026-11-16 are in the project's "Fluid player movement plan" doc. After F1, a playtest found the motion smoother but still generic, not padel; a third video (a professional match from the broadcast camera) gave the "Padel motion catalogue and plan" doc: what real players do in every situation and every shot, and the revised order below (approved 2026-11-17). This file is the in-repo summary that code comments point to.

## The diagnosis

The movement simulation (`FPlayerMotion`, `Data/Movement.json`) is fine. The pose on top of it was stiff:

- Four looping walk and jog clips blended by direction on one phase: no starts, stops, plants or reversals.
- No swing clips: the arm reaches for the ball with IK, the body doesn't turn or shift weight.
- The body turned at a constant 540°/s, all in one piece.
- 0.08 s linear blends, so state changes pop.
- No lean, no pelvis motion, an invisible split step, a frozen ready stance.

## The rule: the simulation leads, the pose follows

Movement stays code-driven in `PadelCore` (deterministic, used by the AI and the soak gate). The animation adapts to it: springs, warping, foot locking and motion matching on the simulation's trajectory. Root motion is not used.

## Why it still looked generic after F1

Real players are recognisable from the stands for three reasons, none of which F1 gave us: a **stance for each spot and moment** (net, back, receiving, serving, between points), a **body shape for each shot** (every shot was the same arm reach: none of `AnimClips.json`'s 42 swing clips exists), and the **rhythm of the point** (split step on every opponent contact, eyes on the ball, side-on for lobs, the wait off the glass, the pair moving as one).

## The steps

| Step | What | Status |
|---|---|---|
| **F1 body dynamics** | Facing on a spring with the head and chest leading the hips; trunk lean into the acceleration; the pelvis sinking on brakes and landings with the feet planted; a visible split-step hop; a light ready-stance bounce. Model: `FPlayerAnimModel::StepBody`; pose: `PadelBodyDynamics`. Data: `Data/Animation.json` (`Facing*HalfLifeS`, `FacingMax*TwistDeg`, `Lean*`, `PlantDrop*`, `PelvisHalfLifeS`, `SplitHopHeightM`, `SplitLand*`, `ReadyBounce*`). | done |
| **P1a stances and the head on the ball** | A stance per spot and moment (`EStance`: Relaxed, Serve, Receive, Net, Back), each with its crouch, racket carry, feet width, knee bend and trunk tip; before the serve the server, receiver and partners each take theirs (`EServeRole`); in a rally Net up to `StanceNetUntilM` from the net, Back from `StanceBackFromM`, blended between; all on a spring. A swing rises out of the stance (its crouch, knee bend, trunk tip and width fade with the swing weight), so the reach starts from full height and the contact warp bends only as far as the ball needs. The head follows the ball (yaw within `HeadLookMaxYawDeg` of the chest, pitch within `HeadLookMin/MaxPitchDeg`). Model: `FPlayerAnimModel::StepStance`, `StepBody`; pose: `PadelBodyDynamics` (`FeetWidenUnits`, `HeadPitchDeg`). Data: `Data/Animation.json` `Stances`, `Stance*`, `HeadLook*` (replacing `ReadyCrouchAlpha*`, `ReadyRacket*`, `ArmReadyAlpha`). | this PR |
| P1b point rhythm | Side-on retreat with crossovers for lobs, the wait for a ball off the glass for every player, the split-step landing toward the first step, the between-points walk and the ball bounce before the serve. | next |
| P2 shot bodies | Key poses per shot and phase (unit turn timed by time to contact, backswing, contact, finish, recovery steps) in `Data/ShotPoses.json`, so each shot has its own silhouette. Chosen by Caio: build them now, replace with captured padel later. | after P1 |
| P3 motion matching | Epic's Game Animation Sample locomotion database for starts, stops and pivots (legs and hips; the stance owns the upper body). | parallel (Mac) |
| P4 real padel capture | Phone-based markerless capture of a club player using the catalogue as the take list, replacing P2's poses shot by shot; the studio session stays for later. | when footage exists |
| F1 inertial blending, foot locking | Kept from the first plan: inertialization for discrete switches after P1, foot locking with P3. | later |

## Checks

- Native (`Tests/Native/PlayerAnimBodyTests.cpp`, `Anim.Body*`): the springs settle on their targets with no jump in turning speed; the head and chest lead a turn within their caps; lean follows the acceleration's direction and is capped; braking and landing sink the pelvis; the split step peaks at `SplitHopHeightM`; the ready bounce only plays standing ready in a rally.
- Native (`Tests/Native/PlayerAnimStanceTests.cpp`, `Anim.Stance*`): every stance is listed once and they differ as real players stand (the receiver lowest and widest, the racket higher at the net, lower and wider at the back, relaxed racket down); the rally blends net to back with no jumps; the four serve roles take their stances; the head follows the ball up, down and behind within its caps, and the server's partner doesn't look back at the serve.
- Unreal (`Padel.Animation.BodyDynamics`): on the skeleton, a pelvis drop keeps the feet within 1 cm, the hop lifts them, a chest twist turns the shoulders, a forward lean moves the head toward the net, the stance sets the feet wider with both on the ground, and a head pitch lifts the face. `Padel.Animation.SwingClearance` now starts every swing from each stance's racket carry.
