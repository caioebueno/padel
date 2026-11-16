# 0003: Match engine in PadelCore

**Date:** 2026-11-15 · **Status:** proposed (Agent 2)

The ball physics, referee and score (GDD 2, 3) live in `PadelCore` as plain C++ and are tested by the native suite. What was decided while building them:

## Shape

- `FMatchSimulation` is the one object a match needs: the ball (`FBallSim`, `FBallState`), the referee (`FReferee`) and the score (`FMatchScore`), stepped by `Tick()` at the fixed 120 Hz with the match's seeded `FPadelRng`. The ball integrates at `SubStepsPerTick` (4, so 480 Hz) so fast balls can't tunnel through the net.
- The GDD 3.4 events go out through `IMatchEventListener`. `UPadelMatchEvents` (a world subsystem in `Padel/Presentation`) attaches a small forwarder to the simulation and rebroadcasts each call as its `On*` delegate, so the simulation never depends on the engine. The delegates are native, not dynamic, so they carry PadelCore's enums and `FScoreSnapshot` unchanged; every listener is C++. Positions arrive in centimetres (converted once, in the forwarder), spin in rad/s and speed in m/s. The owner of the `FMatchSimulation` calls `Attach` and must call `Detach` before destroying it.
- The referee is driven only by `FBallContact`s (surface, crossed the net, left the court) and by reports from gameplay (hits, body hits, net touches, crossings, thrown rackets). Every rule test feeds it hand-written contacts; separate tests play real flights through the whole match.
- `SolveLaunch` turns the shot system's target, speed and spin into a launch velocity (GDD 5). Shot spin is given in the shot's own frame: X = topspin (+) / slice (−), Y = side spin, rad/s. With `ApexHeight > 0` it solves for that apex instead of a speed (lobs).
- Physics tuning is `Data/Physics.json` (one DataTable-style row, `Default`), read by `ParseBallPhysicsJson` with a small JSON reader in `PadelCore`, because the Json module is not plain C++. Every field is required. The native tests load the real file.

## Rule readings

- **Sides on Unreal's axes.** Facing +X, +Y is on your right. A team on the +X half faces −X, so its right side is −Y. The right box is diagonal to the right box.
- **`OUT_DIRECT` vs `WALL_BEFORE_BOUNCE`:** wall first decides (GDD 3.3, coordinator ruling). A shot that touches its own wall and then leaves the court or bounces on its own side is `OWN_SIDE`.
- **Reaching over:** a hit before the ball has crossed to the hitter's half is `CROSSED`.
- **After a legal bounce, the ceiling** counts like leaving the court: `OUT_OF_COURT_WINNER` for the hitter.
- **Out-of-court play is post-slice,** so the point ends as soon as a legally bounced ball leaves the court.
- **Three extra reason codes** name the GDD 3.3 special cases: `OUT_OF_COURT_WINNER`, `CAME_BACK_OVER` (bounced in, came back over on its own; no reach-over in the slice), `STUCK_IN_FENCE` (also the watchdog: a live ball nearly still for `StuckSeconds`, or no hit for `MaxFlightSeconds`).
- **Serve faults** have their own codes (`EServeFault`): `NO_BOUNCE`, `ABOVE_WAIST`, `FOOT_FAULT`, `MISSED_BALL`, `INTO_NET`, `WRONG_BOX`, `WALL_BEFORE_BOUNCE`, `MESH_AFTER_BOUNCE`, `OUT`, `HIT_PARTNER`. A serve that hits the server's partner is a fault, and one that hits a receiver before the bounce loses them the point.
- **Lets:** a serve that touches the net and bounces in the box is replayed with the same serve number as soon as anything other than the mesh happens next, including a return. Mesh first is a fault, let or not.
- **Rolling.** A bounce too slow to rise turns into a roll and also reports a second ground contact, so a ball that dies on the ground always ends as a double bounce.
- **Golden point:** the receiving pair picks the receiver with `ChooseGoldenPointReceiver(PlayerId)`. The default is the normal side for the seventh point (right).
- **Change of ends:** after a game when the set's game total is odd (so a 6-3 set changes, and again after game 1 of the next set; a tiebreak counts as one game), and every 6 points in a tiebreak.
- **Tiebreak serving:** the next server serves point 1, then two each. The team that received first in the tiebreak serves first in the next set.
