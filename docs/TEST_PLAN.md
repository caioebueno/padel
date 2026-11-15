# Test plan: vertical slice

Owner: Agent 8 (QA). This is the checklist QA reviews PRs against. It turns GDD section 3 (rules), section 2 (physics), section 5 (contact model) and section 12 (definition of done) into concrete tests. Each row has an ID; put the ID in the test name or a comment (`# TP R-07`) so coverage can be traced.

Status key: **gate** = CI test already enforces it · **todo** = the owning agent writes the test with their feature · **QA** = QA runs or reviews it by hand.

## 0. Gates already in CI (this PR)

| ID | Check | Test |
|---|---|---|
| G-01 | Every GDD 3.3 reason code (and `OUT_OF_COURT_WINNER`) is an enum key in `scripts/engine/types.gd` | `tests/unit/test_gdd_contracts.gd` (pending until `types.gd` exists) |
| G-02 | Every reason code is referenced by at least one test | same file (pending until `types.gd` exists) |
| G-03 | The `MatchEvents` autoload declares every GDD 3.4 signal with the right argument count | same file (pending until the autoload is registered) |
| G-04 | `scripts/engine/` uses no global RNG, wall clock, frame delta, scene tree or `RigidBody3D` | `tests/unit/test_engine_hard_rules.gd` |
| G-05 | No script in `scripts/engine/` is a Node | same file |
| G-06 | Every script parses with untyped declarations as errors | `tools/check_scripts.sh` (Agent 1) |

The reason codes and event names are read from `docs/GDD.md`, so changing the GDD changes the gate. That is on purpose.

## 1. Scoring (GDD 3.1) — Agent 2, `todo`

| ID | Case | Expect |
|---|---|---|
| S-01 | Points from 0-0 | 15, 30, 40, game |
| S-02 | 40-40, golden point on | Next point wins the game; `golden_point_started(receiving_team)` fires once |
| S-03 | Golden point, receiving pair chooses receiver | Chosen player receives, server unchanged |
| S-04 | 40-40, advantage on | AD, back to deuce, then game after two in a row |
| S-05 | Set at 6-4 | Set won; 6-5 continues; 7-5 wins |
| S-06 | 6-6 | Tiebreak starts; 7-5 wins it; 7-6 doesn't; 8-6 does |
| S-07 | Best of 3 | 2-0 and 2-1 both end the match; no fourth set ever starts |
| S-08 | 1-set option and super tiebreak option | 1 set ends at the first set; super tiebreak is first to 10, lead of 2, replaces set 3 |
| S-09 | Change of ends | After games 1, 3, 5... of each set (total games odd); every 6 points in a tiebreak |
| S-10 | `score_changed` snapshot | Emitted after every point, and always equals the score object |

## 2. Serve (GDD 3.2) — Agent 2, `todo`

| ID | Case | Expect |
|---|---|---|
| V-01 | Serving order | A1, B1, A2, B2 kept for the whole set; serving team keeps serve for the game |
| V-02 | First point of a game | Served from the right half; alternates each point |
| V-03 | Server position | In front of the service line, or on it, or across the centre line = fault |
| V-04 | Contact height above the waist / overhead | Fault |
| V-05 | Serve to the wrong box, or not diagonal | Fault |
| V-06 | Serve on the box line | In |
| V-07 | Good serve, then glass | In play |
| V-08 | Good serve, then mesh first | Fault |
| V-09 | Net then into the right box | Let, replayed, same serve number |
| V-10 | Net then right box then mesh | Fault, not let |
| V-11 | Net then wrong box | Fault |
| V-12 | Two faults | `DOUBLE_FAULT`, point to receivers |
| V-13 | Fault, then a good second serve | Point plays on; fault count resets for the next point |
| V-14 | Swing and miss on the serve | Fault |
| V-15 | Receiver volleys the serve | `VOLLEYED_SERVE`, point to servers |
| V-16 | Receiver's partner volleys or touches the serve before it bounces | Point to servers (body or volley) |
| V-17 | Tiebreak serving | First point from the right by the next server, then two each, starting from the left |

## 3. Rally (GDD 3.3) — Agent 2, `todo`

One test per reason code is the minimum (G-02 enforces it). These are the cases that catch the bugs referees usually have:

| ID | Code / case | Expect |
|---|---|---|
| R-01 | `DOUBLE_BOUNCE` | Second ground bounce on a side loses for that side, even after glass in between |
| R-02 | Bounce, glass, glass, mesh, then played | Legal: walls any number of times after the bounce |
| R-03 | `INTO_NET` | Shot doesn't cross Z = 0 above the net height at that X |
| R-04 | Net cord rolls over in a rally | In play (not a let) |
| R-05 | `WALL_BEFORE_BOUNCE` | Hits opponents' glass or mesh before their ground |
| R-06 | Straight out over the back wall without bouncing | `WALL_BEFORE_BOUNCE` or `OUT_DIRECT` as the GDD table says; pick one and test it, don't leave both |
| R-07 | `OUT_DIRECT` | Ceiling, light, post, outside run-off before bouncing in |
| R-08 | `OWN_SIDE` | Bounces on own ground after own hit |
| R-09 | Contrapared: own glass, then over, then opponents' ground | Legal |
| R-10 | Contrapared: own glass, over, hits opponents' glass first | `WALL_BEFORE_BOUNCE` |
| R-11 | Own wall, never comes back over | `OWN_SIDE` |
| R-12 | `BODY_HIT` | Ball touches a player before the second bounce, including a ball heading out |
| R-13 | `DOUBLE_HIT` | Same player twice, and partner after partner |
| R-14 | `NET_TOUCH` | Player, racket or clothing touches the net or post while live |
| R-15 | `CROSSED` | Player crosses Z = 0 while live |
| R-16 | `RACKET_THROWN` | Contact from a racket not in hand |
| R-17 | `OUT_OF_COURT_WINNER` | Bounces in, then leaves over the back wall ("by three") or through a door / over side mesh ("by four") = point to hitter (slice) |
| R-18 | Ball comes back over on its own | Point to hitter (slice, no reach-over) |
| R-19 | Top of the wall / joint, stays in | In play |
| R-20 | Stuck in mesh after bouncing in / before | Hitter wins / hitter loses |
| R-21 | Lines | Ball touching the line (within 0.025 m of its centre line + ball radius rule chosen by Agent 2) is in. Test exactly on the edge |
| R-22 | `point_ended` | Fires once per point with one winner, one reason code, and the last hitter |

## 4. Ball physics (GDD 2) — Agent 2, `todo`

Physics tests are tolerance tests at a fixed step. Values come from `data/physics/*.tres`, so tests load the same resource instead of repeating numbers.

| ID | Case | Expect |
|---|---|---|
| P-01 | Free fall from 2 m, no spin | Lands at t = sqrt(2h/g) ± one step |
| P-02 | Ground bounce, no spin | Outgoing normal speed = restitution × incoming, ± 1% |
| P-03 | Topspin vs flat vs slice, same launch | Topspin bounces forward faster and higher; slice skids lower and slower |
| P-04 | Glass vs ground vs mesh | Glass keeps more speed than ground; mesh is the deadest |
| P-05 | Mesh randomness | Deflection within ±15° and speed within ±20%; same seed gives the same result; different seeds differ |
| P-06 | Net | Ball hitting the tape loses most energy; can roll over |
| P-07 | Determinism | Same seed and inputs give a bit-identical trajectory over 10 s, run twice in one process |
| P-08 | Step independence | 120 Hz vs 240 Hz land within a few cm of each other (catches integrator bugs) |
| P-09 | No tunnelling | A 140 km/h smash at the glass never passes through it at the fixed step |
| P-10 | Energy | No bounce gains energy |
| P-11 | Launch solver | Given a target, speed and spin, the ball lands within 5 cm of the target with zero scatter |

## 5. Shots and contact (GDD 4, 5, 7) — Agent 3, `todo`

| ID | Case | Expect |
|---|---|---|
| C-01 | Timing grades | Release at 0, ±40, ±90, ±160, ±161 ms gives perfect, perfect, good, early/late, miss |
| C-02 | Attribute windows | Attribute 99 widens by 35%; 1 narrows by 20% |
| C-03 | Auto-fire | Holding through the window fires at the late edge |
| C-04 | Position multiplier | `1 + 0.6 × d` error, power `1 − 0.3 × d`, floor 0.5 |
| C-05 | Moving shot | Full speed ×1.3 error; removed after a split-step |
| C-06 | Formulas 5.3 | Max speed, spin, scatter, `power_mult` (1.0 to 80%, 1.6 at 100%), `pressure_mult` only on big points, stamina factor |
| C-07 | Overhead context | Not set or charge < 60% → bandeja; RB forehand → víbora; RB backhand → bandeja with lower accuracy; set + 100% → smash; not set + 100% → bandeja with extra error |
| C-08 | Wall shots | Scatter × `lerp(1.6, 0.9, WallPlay/99)`, Wall Play drives the windows |
| C-09 | No hard-coded tuning | Every number above comes from a `.tres` (QA reviews) |
| C-10 | Every slice shot reachable | A ball-machine integration test produces each shot type at least once, and each off the glass |

## 6. AI (GDD 8) — Agent 4, `todo` and `QA`

| ID | Case | Expect |
|---|---|---|
| A-01 | Prediction | AI's predicted interception matches the engine sim with Wall Play 99 and no noise |
| A-02 | Determinism | Same seed, same match, twice: identical point log |
| A-03 | Difficulty data | Easy/Medium/Hard load from `data/difficulty/` with the GDD table values |
| A-04 | Partner commands | Each command changes the partner's target position and lasts until the point ends or the next command |
| A-05 | Soak (GDD 12.2) | 100 AI-vs-AI matches, fixed seeds: zero stuck points, zero invariant failures. QA runs it on every engine or AI PR |
| A-06 | Soak invariants | Each point ends with exactly one winner and a reason code; a point never lasts more than N simulated seconds; score always valid; serve order valid; ball never below Y = 0 or outside the walls without `ball_left_court` |
| A-07 | Feel numbers (GDD 12.4) | On Medium: average rally 6–12 shots; ≥ 25% of points have a shot off the glass. The soak prints these |

## 7. UI and presentation (GDD 3.4, 9, 12) — Agent 7, `QA`

| ID | Case | Expect |
|---|---|---|
| U-01 | Full flow on gamepad only | Main menu → quick match → stats screen, no keyboard or mouse |
| U-02 | HUD matches the engine | Score bug equals `score_changed` after every point, including golden point and tiebreak |
| U-03 | UI never writes engine state | UI and audio only listen to `MatchEvents` (QA reviews) |
| U-04 | Pause | Pausing mid-point and resuming changes nothing in the engine result (determinism) |

## 8. How QA reviews a PR

For every PR:

1. Read the GDD sections the PR says it implements, and check the behaviour against them, not against the PR description.
2. Check out the branch and run `tools/check_scripts.sh` and `tools/run_tests.sh` locally with Godot 4.7.2. Don't trust "tests pass" without running them.
3. For engine or AI PRs, run the soak once it exists.
4. Check CLAUDE.md: folder ownership, static typing, no tuning numbers in code, signals not renamed, generated files not hand-edited, no assets or names from real padel or other games, `assets/manifest.csv` updated for new assets.
5. Look for missing tests: every rule needs one, every bug fix gets one that would have caught it.
6. Leave a review with exact file and line references and, for bugs, steps to reproduce. File an issue for anything outside the PR's scope.
7. QA doesn't change gameplay code. When a test exposes a bug, the test goes in the QA PR (or a comment on the owner's PR) and the fix belongs to the owner.
