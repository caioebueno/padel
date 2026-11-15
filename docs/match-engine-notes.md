# Match engine design notes (engine-agnostic, WIP)

Written by the match engine thread (Agent 2) before the switch away from Godot. These notes carry the rules, scoring and physics design into the Unreal build. GDD section numbers refer to docs/GDD.md.

## Conventions agreed with the other threads

- Teams: A = 0, B = 1. Players: 0, 1 are Team A; 2, 3 are Team B. Partner = id XOR 1.
- Coordinates as GDD 2: metres, Y up, net at Z = 0, Team A starts on +Z. (Unreal is Z up and centimetres; convert at the boundary.)
- A player facing -Z has +X on their right. So a team on half s (+1 or -1) has its right side at x sign = s. Serves go right box to right box and left to left (diagonal).
- Shot types (agreed with controls): DRIVE, VOLLEY, LOB, BANDEJA, VIBORA, SMASH, CHIQUITA, SERVE_FLAT, SERVE_KICK, MISHIT. Timing: PERFECT, GOOD, EARLY, LATE, MISS.
- The shot system passes spin in the shot's own frame: x = topspin (+) / slice (-), y = side spin, rad/s. World spin = topspin × (up × forward) + side × up.
- Event names are GDD 3.4. The UI reads enum keys by name. The golden-point receiver choice needs an engine method (for example choose_golden_point_side) that the HUD calls.
- Score snapshot keys promised to the UI: points (display strings "0", "15", "30", "40", "AD" or tiebreak numbers), games, sets (finished sets), set_index, in_tiebreak, golden_point, serving_team, server_id, receiver_id, serve_side, team_a_end, break_point, set_point, match_point, match_over, winner.
- AI and controls asked for: a copyable ball state, a pure step(state, dt, rng) returning contacts, predict(state, horizon, sample_dt) returning samples {t, position, velocity, ground_bounces, off_glass}, a shared launch solver, and referee queries team_to_play, bounces on that half, off_own_glass, is_serve, is_live, last_hitter_team.

## Reason codes

Point reasons: DOUBLE_BOUNCE, INTO_NET, WALL_BEFORE_BOUNCE, OUT_DIRECT, OWN_SIDE, BODY_HIT, DOUBLE_HIT, NET_TOUCH, CROSSED, RACKET_THROWN, DOUBLE_FAULT, VOLLEYED_SERVE, plus three for the GDD special cases: OUT_OF_COURT_WINNER, CAME_BACK_OVER, STUCK_IN_FENCE.

Serve faults: NO_BOUNCE, ABOVE_WAIST, FOOT_FAULT, MISSED_BALL, INTO_NET, WRONG_BOX, WALL_BEFORE_BOUNCE, MESH_AFTER_BOUNCE, OUT, HIT_PARTNER.

Coordinator ruling: OUT_DIRECT is a ball that leaves the court (or hits the ceiling) without bouncing. WALL_BEFORE_BOUNCE is only a ball that hits the opponents' glass or mesh before bouncing.

## Referee (event driven)

Inputs: ball contacts from the sim (ground, glass, mesh, net, ceiling, each with half and position), a "crossed the net" event when the ball centre changes Z sign inside the court, a "left court" event, hits (player id), and reports for a body hit, a net touch, a player crossing, a thrown racket, a missed serve and a stuck ball.

State after a hit by team T (own half H, opponents' half O): crossed, legal_bounce, bounces, touched_net, walls since the bounce.

- Crossed the net: the first time sets crossed. After a legal bounce it means the ball came back on its own: CAME_BACK_OVER, T wins (no reach-over in the slice).
- Ground on H before crossing: INTO_NET if the net was touched, otherwise OWN_SIDE.
- Ground on O: the first is the legal bounce. The second is DOUBLE_BOUNCE, and T wins.
- Glass or mesh on H before crossing: allowed (contrapared). Ground on H after that is OWN_SIDE, and so is leaving the court.
- Glass or mesh on O before the legal bounce: WALL_BEFORE_BOUNCE. After it: in play, any number of times.
- Ceiling or leaving the court: before the legal bounce OUT_DIRECT, after it OUT_OF_COURT_WINNER.
- A second hit by T: DOUBLE_HIT. A hit by O before the ball crossed: CROSSED (reaching over).
- Body hit: the body's team loses. Net touch, crossing or a thrown racket: the offender's team loses, only while the ball is live.
- Watchdog: a live ball that stays nearly still for about 1.5 s, or 20 s with no hit, ends as STUCK_IN_FENCE. The hitter wins if the ball had bounced legally, and loses otherwise. This keeps soak runs free of stuck points.

Serve:
- Checked at contact: the ball bounced first, contact is at or below the waist, and the server is behind the service line (not on it), between the centre line and the side wall on the correct side.
- In flight: the first bounce must be in the diagonal box (lines in: half a line width of tolerance). Ground on the server's own half is INTO_NET if the net was touched, otherwise WRONG_BOX. Glass or mesh before the bounce: WALL_BEFORE_BOUNCE. Out or ceiling: OUT. The serve hits the server's partner: HIT_PARTNER. The receiving team hits before the bounce: VOLLEYED_SERVE. The serve hits a receiver's body before the bounce: BODY_HIT.
- After a good bounce: if the first wall contact is mesh, it is a MESH_AFTER_BOUNCE fault, even for a serve that touched the net. If the net was touched and the next event is anything but mesh, it is a let, and the same serve number is replayed.
- First fault: serve number 2. Second fault: DOUBLE_FAULT, and the receivers win.

## Score

- Game: 0, 15, 30, 40. Golden point: at 3-3 the next point wins, and the receiving team picks the side (default right, which is also the normal side for the seventh point). Advantage mode: win at 4 or more with a 2-point lead, shown as 40/AD.
- Set: first to 6 with a 2-game lead, tiebreak at 6-6, first to 7 by 2. Match: sets_to_win 2 (best of 3) or 1. Optional super tiebreak to 10 instead of the deciding set.
- Serve side: points played even = right, odd = left (tiebreaks too: point 0 right, point 1 left, point 2 right, and so on).
- Rotation: A1, B1, A2, B2, kept for the set. Each team may reorder at the start of a set. Tiebreak: point 0 by the next server, then two each in rotation. The next set starts with the team that received first in the tiebreak.
- Change of ends: after a game when the set's game total is odd (so also after a set with an odd total, then again after game 1 of the next set), and every 6 points in a tiebreak.
- The snapshot carries break_point, set_point and match_point, computed by playing the next point on a copy, for the GDD 5.3 pressure multiplier.
- Event order after a point: point_ended, game_won, set_won, match_won, change_of_ends, score_changed, then golden_point_started when the next point is golden (so the HUD can prompt during the pause).

## Ball physics

- Forces: gravity, quadratic drag (a = -k_d |v| v, k_d about 0.0194 1/m), Magnus (a = k_m (spin × v), k_m about 0.0007), and slow spin decay. Fixed sub-step of 1/240 s, semi-implicit Euler, with a seeded RNG passed in (only the mesh uses it).
- Bounce: restitution on the normal speed, and Coulomb friction on the contact-point slip (velocity plus spin × contact offset), capped at the impulse that stops the slip, with a moment of inertia of 2/3 m r² (hollow ball). This gives the topspin kick and the slice skid on the grass, and keeps spin on the low-friction glass.
- Surfaces (restitution / friction): ground 0.72 / 0.55, glass 0.68 / 0.15, mesh 0.30 / 0.70 with ±15° and ±20% randomness, net 0.10 / 0.80, ceiling 0.5 / 0.3. A ground bounce slower than about 0.25 m/s vertically turns into rolling, and the switch also reports a second ground contact so the referee always sees the double bounce.
- Walls by height: the back wall is glass to 3 m, then mesh to 4 m. Side walls: 8 to 10 m from the net, glass to 3 m then mesh to 4 m; 6 to 8 m, glass to 2 m then mesh to 3 m; 4 to 6 m, the exit (open); 0 to 4 m, mesh to 3 m. Above those tops the ball leaves the court.
- Net: a swept test against the plane, because fast balls move more than a ball diameter per step. Below the tape the ball bounces back. Within one radius above the tape it clips it and carries on with about half its speed.
- Launch solver: the flat arc starts from the drag-free angle and is refined with a secant method on elevation, plus a yaw correction for side spin (about 8 simulated flights). Apex mode (lobs, pop-ups) solves the vertical speed for the apex and the horizontal speed for the landing point. It returns ok, velocity, world spin, flight time, landing point, and whether the ball clears the net.
- Prediction runs the same step on a copy with no mesh randomness, and returns samples with ground bounces and an off-glass flag for the AI.

## Tests planned

One or more per GDD 3 row: each point reason, each serve fault, let and let-then-mesh, contrapared, walls after the bounce, out-of-court winners over the back wall and through the exit, the ball coming back over, and the score cases (golden point, advantage, 6-4 versus 7-5, tiebreak 7-5 versus 8-6, super tiebreak, rotation, serve sides, change of ends, pressure flags). Physics: free fall, restitution, topspin kick versus slice skid, glass versus mesh energy, mesh determinism with a seed, exits, net, ceiling, solver accuracy within 0.15 m.
