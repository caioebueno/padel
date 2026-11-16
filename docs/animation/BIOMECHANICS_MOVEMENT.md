# Padel movement biomechanics

How padel players move between and around their shots, with numbers from sports science where they exist, and what that means for this game's movement model, AI positioning and animation. The strokes themselves are covered separately; this doc stops at the moment the swing starts and picks up again at the recovery.

**Status:** reference and proposals (2026-11-16). Nothing here changes code or data. Section 8 lists the follow-ups, each for the area that owns it. Implements no GDD section on its own; it feeds GDD 6.1, 6.3 and 8, [0005](../decisions/0005-player-animation.md), [0006](../decisions/0006-metahuman-players.md) and [MOCAP_SHOT_LIST.md](MOCAP_SHOT_LIST.md).

**How to read the numbers.** Each value is tagged:

- **[M]** measured in a padel study (source in section 9).
- **[T]** measured in tennis or another racket sport, transferred because nothing padel-specific exists.
- **[E]** an estimate from general biomechanics or coaching practice, with no study behind the exact number. Treat these as starting values for playtests, not facts.

All units are metres, seconds and m/s, like the GDD. "Depth" is distance from the net toward the player's own back glass, as in `AiTeamFrame`.

---

## 1. What a padel point asks of the body

Padel is a game of short, frequent, multi-directional bursts, mostly sideways and backwards, on a 10 × 10 m half with walls behind and beside the player. It is lower in volume and speed than tennis but has more changes of direction per metre covered.

| Measure | Value | Level | Tag |
|---|---|---|---|
| Point (rally) duration | 10 to 15 s typical; studies range 7 to 17 s | professional | [M] Torres-Luque 2015; Mellado-Arbelo & Baiget 2022 |
| Shots per point | 8 to 10 (men about 8, women about 11 in 2024 Premier events) | professional | [M] Sánchez-Alcaraz 2021; Premier Padel 2026 study |
| Stroke rate | 0.83 shots/s men, 0.75 women, so about 1.2 to 1.3 s between shots | professional | [M] Premier Padel 2026 study |
| Rest between points | 15 to 22 s | national to international | [M] García-Benítez 2016; review |
| Work : rest | 1:1.2 to 1:2.5 | various | [M] review |
| Ball in play as share of match time | 30 to 35% | professional | [M] Torres-Luque 2015 |
| Distance per match | 2,000 to 3,500 m (median 3,430 m in 85 min) | professional | [M] Miralles 2025 |
| Distance per hour | about 2,400 m | professional | [M] Miralles 2025 |
| Distance per point, per player | median 7.2 m (95th percentile 30 m) national; about 11 m mean professional; 4 m recreational | national, pro | [M] Ramón-Llin 2017; 2020/21 via Escudero-Tena 2023 |
| Average speed while the ball is in play | 1.0 to 1.2 m/s | national | [M] Ramón-Llin 2017 |
| Time spent walking / jogging / running / sprinting during rallies | 70% / 26% / 4% / 0% | national | [M] Ramón-Llin 2017 |
| Peak speed in a match | median 4.2 m/s (15.2 km/h); up to about 7 m/s (25 km/h) | professional | [M] Miralles 2025 (GNSS); Ramón-Llin (cited there) |
| Peak acceleration / deceleration in a match | median +4.6 / −4.5 m/s² | professional | [M] Miralles 2025 |
| Accelerations above 1.12 m/s² | about 395 per hour, 80% of them over 1 to 2 m | professional | [M] Miralles 2025 |
| Heart rate | 69 to 86% of max (pros at the top of the range) | national, international | [M] review |
| Blood lactate | 2.4 to 3.4 mmol/L | national | [M] review |

What this means for the game:

- **Points are short-burst work.** The typical player moves 1 to 2 m, stops, hits, and moves 1 to 2 m again, about once a second. A full sprint across the court is rare. A movement model and an animation set that look right for 1 to 3 m moves matter far more than ones that look right at top speed.
- **Most of the time the player is walking-speed or slower,** in an athletic ready stance, not upright. The upright walk cycle should almost never show during a rally (section 6).
- **Winners move more often, not further.** In Miralles 2025 the winners made more accelerations per hour (415 vs 382) over the same distance. So quality of movement shows as quick small adjustments, not long runs.

Caveat on peak speed: GNSS-based tracking smooths short bursts, so the 4.2 m/s median peak is probably a slight underestimate. It is still the best padel-specific number, and the 7 m/s upper value is the ceiling.

## 2. The building blocks

### 2.1 Ready position

The default posture whenever the ball is live and not on the way to the player's racket.

- Feet about 1.2 to 1.5 × shoulder width apart, weight on the balls of the feet, heels light. [E]
- Knees bent about 20 to 40° and hips flexed, chest forward over the knees, so the centre of mass is 5 to 15 cm lower than standing. Lower at the net when expecting a hard volley, higher at the back. [E]
- Racket in front at about chest height, head up, held with both hands or the free hand on the throat. Higher at the net (volley and overhead come fast), lower at the back. [E]
- Shoulders square to the net, or turned slightly toward the side the ball is on. [E]
- Not static: small rhythmic weight shifts or toe bounces, so the muscles stay pre-loaded. [E]

### 2.2 Split step

A small hop made as the opponent hits, so the player lands loaded and can push off in any direction. It is the single most important movement habit in racket sports.

| Measure | Value | Tag |
|---|---|---|
| Take-off relative to the opponent's contact | about 0 ms (−40 to +34 ms across studies) | [T] Mecheri 2019; Avilès 2002, 2014 |
| Landing after the opponent's contact | about 150 ms (150 to 160 ms; lab studies 180 to 210 ms) | [T] Mecheri 2019 |
| Air time | 120 to 200 ms (world class about 140 ms) | [T] Mecheri 2019 |
| First racket movement after contact | about 190 ms | [T] Mecheri 2019 |
| First lateral foot movement after contact | 265 ms (pros) to 310 ms (juniors) | [T] Mecheri 2019 |
| Hop height | 3 to 8 cm, just enough to unweight | [E] |

How it looks: the player lands slightly wider than the ready stance, often landing on the outside foot first on the side they're about to move away from (the "gravity" or pivot step), so the first push is already loaded. In padel the split is done at the back and at the net alike; at the net it is smaller and quicker because the ball arrives in 0.4 to 0.8 s. [E]

A split step needs about 0.3 s of near-stillness before contact to do well; a player still running hard when the opponent hits can't split and pays a 50 to 100 ms penalty on the first step. [E, consistent with the response-time data above]

### 2.3 Lateral shuffle (side step)

The main way to move 0.5 to 3 m across while staying square to the net.

- Push from the trailing leg, step with the leading leg, feet never cross and never come fully together. [E]
- Step length 0.5 to 0.9 m, cadence 3 to 4 steps/s, so a sustainable shuffle runs at about 2 to 3.5 m/s. The top shuffle speed is roughly 55 to 70% of the player's forward sprint speed. [E]
- The centre of mass stays low and level: a shuffle that bobs up and down looks wrong at once. [E]
- Used for nearly all moves under about 2.5 m, at the net and at the back. [E]

### 2.4 Crossover step and turn-and-run

When a move is longer than about 2.5 to 3 m sideways or the ball is going behind the player, a shuffle is too slow.

- **Crossover:** the outside leg crosses in front of the inside one, the hips turn 45 to 90° toward the direction of travel, and the player runs. A first crossover step is 0.8 to 1.1 m long. [E]
- **Turn-and-run (back):** for a lob over the head, the player pivots on the inside foot, turns the hips and shoulders sideways (90°) or fully toward the back glass, and runs with crossovers or a full run, watching the ball over the shoulder. [E]
- Backpedalling while facing the net is only good for 1 to 2 m; it tops out around 2.5 to 3 m/s and is unstable. Good players turn and run rather than backpedal any real distance. [E]
- Hip turn and the first step take about 0.15 to 0.25 s together; the turn is the price for the faster run. [E]

### 2.5 Forward run and the approach to the net

- Running forward from the back to the net covers 4 to 6 m, typically in the 2 to 3 s of a deep lob's flight or while the opponents are dealing with a ball off their glass. [M, Ramón-Llin 2019: an 8.6 m-deep lob gave the lobbed pair about 3 s]
- The approach ends with a deceleration and split step at about 3 to 4 m from the net, not a run into the net. [M, Ramón-Llin 2019: servers took the third shot 3.8 to 4.5 m from the net]
- Run step length at 4 to 6 m/s is 1.2 to 1.7 m, cadence 3.5 to 4.5 steps/s. [E, general sprint mechanics]

### 2.6 Deceleration and plant

Every burst ends in a stop. Padel's measured decelerations are as big as its accelerations (−4.5 m/s² median peak). [M]

- The player lowers the centre of mass in the last 1 to 2 steps, lengthens the final step, and plants the outside foot with the knee over the toes, so they can push back the other way. [E]
- Before a groundstroke the last step is the "set" step: the front foot (open or closed stance) lands as the racket starts forward. [E]
- On artificial grass with sand the shoe slides a few centimetres on a hard plant. A short slide is realistic; a long one isn't. [E]

### 2.7 Recovery steps

After the contact the player gets back toward the pair's home position.

- The first 1 or 2 recovery steps start during the follow-through (0.2 to 0.4 s after contact), with a shuffle or crossover back toward the middle. [E]
- At the net, recovery is mostly a re-set of depth: a step or two back to about 3 m after volleying close, or forward after a bandeja. [E]
- Recovery must be done, or nearly done, by the opponent's next contact (about 1.2 s later, section 1) so the split step can happen. [M for the 1.2 s, E for the rest]

### 2.8 Playing off the back glass

The defensive core of padel, and the clearest difference from tennis. Back-wall shots were 15% of all shots at national level, side-wall shots 7%. [M, Ramón-Llin 2017]

1. **Read and retreat.** As the ball crosses the net high or deep, the player turns side-on (hips 60 to 90° toward the side of the coming shot) and retreats toward the glass with shuffles or crossovers, staying ahead of the ball. [E]
2. **Wait.** They stop 1.5 to 2.5 m from the back glass, side-on, racket already back, and let the ball bounce and hit the glass. Getting too close to the glass is the classic error. [E]
3. **Follow it out.** As the ball comes off the glass, they move forward and out with it, 0.5 to 2 m, keeping it beside them, and hit at waist to chest height 1.5 to 3 m from the glass. [E]
4. **Recover forward** if the shot was good (a deep lob), or stay back if it wasn't. [E]

Timing: the ball arrives off the glass 0.3 to 0.8 s after its bounce, so the player commits to the "wait" spot about a second before contact. That is why `GlassRereadDelayScale` (the AI re-reads on a glass contact at half its delay) matters and why Wall Play is modelled as prediction noise. [E]

### 2.9 Playing off the side glass and the corner

- **Side glass:** the player moves parallel to the side glass and opens the body toward it (back to the side glass for a forehand side, chest to it for a backhand), lets the ball come off, and turns the hips back toward the court to hit. [E]
- **Corner (back then side, "double wall"):** the ball's path turns twice. The player stays further out from the corner (2 to 3 m from both walls), turns to face the corner, and pivots 90 to 180° as the ball comes off the second wall. This is the hardest movement read in the game. [E]

### 2.10 Overheads and jumping

- **Getting under the ball:** from the net the player turns side-on (non-hitting shoulder toward the net), and moves back with crossovers or a side-on skip (the "chassé"), keeping the ball in front of the hitting shoulder. Running straight back facing the net is a coaching error. [E]
- **Bandeja and víbora** are usually played without a real jump, often while still moving slightly back, to keep balance and the net position. [E]
- **Smash:** set under the ball, then a jump from both feet or a scissor kick (take off from the back leg, land on the front). [E]
- **Jump height:** countermovement jump 29 to 37 cm in professional men (right-side players higher in one study), about 24 cm in professional women, 28 cm in competitive 12 to 16-year-olds. [M, Pradas 2025; Sánchez-Alcaraz 2025] A scissor jump with a short approach can add a little, so 0.2 to 0.45 m of extra contact height is the realistic band.
- **Standing contact height:** standing fingertip reach is about 1.33 × body height, and the racket's sweet spot sits about 0.35 to 0.45 m beyond the hand, so a 1.80 m player hits a standing overhead at about 2.7 to 2.85 m on the toes. [E, anthropometric estimate]

### 2.11 Running out of the court (post-slice)

For a ball that has bounced and is leaving the court through a door or over the back glass (GDD 3.3, `OUT_OF_COURT_WINNER`). Post-slice for players (GDD 12), the rule is already in.

- The player turns and sprints diagonally toward the nearest door (2 m wide, at the net end of the side glass), covering 4 to 8 m inside the court and 2 to 5 m outside. This is the one place in padel where real top speed (6 to 7 m/s) happens. [E, with M for the 7 m/s ceiling]
- They play the ball back over the side wall or back through the door, then sprint back in; the partner covers the whole court alone meanwhile. [E]

### 2.12 Moving as a pair

Padel is played by a pair that moves "as if tied by a rope". [E, coaching consensus]

| Behaviour | What it looks like | Tag |
|---|---|---|
| Same depth | Both at the net (about 2 to 4 m) or both back (about 7.5 to 9 m). One up, one back is the losing "split" formation. | [E]; GDD 8 |
| Lateral spacing | 3.5 to 5 m apart; narrower when the ball is in the middle of the opponents' court, wider when it's wide. | [E] |
| Shift with the ball | The pair slides toward the side the ball is on. The ball-side player covers the line; the partner closes the middle. | [E]; GDD 8 |
| Diagonal stagger | At the net, the partner away from the ball stands 0.5 to 1 m deeper than the ball-side player, to cover the lob and the middle. | [E] |
| Advance together | After a deep lob or a good attacking shot both go forward in the ball's flight time (2 to 3 s), arriving together. | [M] for the time, [E] for the rest |
| Fall back together | When a lob passes one player, both retreat, so the pair isn't split. | [E] |
| Switching sides ("cross") | When a lob goes over one player toward the partner's side or the middle, the partner crosses diagonally behind to take it, and the lobbed player crosses to the partner's side. They switch back when play allows. | [E] |
| The middle | Down the middle, the forehand facing the middle takes it by default (GDD 8). | [E]; GDD 8 |
| Serving pair | The server serves from behind the service line and follows the serve in to about 3.8 to 4.5 m from the net for the third shot; the server's partner starts at the net. | [M] Ramón-Llin 2019 |
| Distance per player | The server covers noticeably more than the partner during a point. | [M] Ramón-Llin 2019 (cited) |

### 2.13 Fatigue

- Padel is mostly aerobic with short alactic bursts: lactate stays at 2.4 to 3.4 mmol/L, heart rate at 69 to 86% of max. [M]
- In the one study that measured neuromuscular fatigue across a three-set amateur match, jump height and grip strength did not drop; heart-rate load rose in the third set. [M, Marcos-Rivero 2025]
- So fatigue in padel shows mainly as **slower recovery between points and late in long rallies,** not as a big drop in speed over a match. Rallies over about 20 s and long runs (out of the court, repeated lobs) are where it bites. [E]

## 3. The current game, measured against this

### 3.1 Movement (`FPlayerMotion`, `Data/Movement.json`)

| Today | Real | Gap |
|---|---|---|
| Top speed 4.5 (rating 1) to 6.5 m/s (99), +15% sprint. A rating-77 player runs at 6.05 m/s, 6.96 sprinting. | Peak in a match: median 4.2 m/s, ceiling about 7. | Top speed is about right as a ceiling, but see the next row. |
| Constant acceleration to top speed in 0.9 to 0.45 s, the same in every direction. A rating-77 player accelerates at 11 m/s² and is at full speed after 1.7 m. A rating-99 player accelerates at 14.4 m/s². | Measured peak 4.6 m/s² (GNSS, likely low); human sprint starts reach about 6 to 9 m/s² and fall as speed rises. Most moves are 1 to 2 m. | Short moves reach unrealistically high peak speeds, and 99-rated acceleration is beyond human. The 5 m and 10 m straight-line times are close to an athlete's (1.1 s and 1.9 s at rating 77), so the fix is the *shape* of the curve, not the times. |
| Speed is the same forward, sideways and backward, and the body always faces the net. | Shuffle tops out at 55 to 70% of a sprint, backpedal at 2.5 to 3 m/s; going faster needs a hip turn costing about 0.15 to 0.25 s. | The biggest realism gap. Players backpedal at sprint speed to chase lobs and shuffle at 6 m/s. |
| Braking at 1.6 × acceleration. | Decelerations as large as accelerations, with a lowered plant step. | Fine as is. |
| Split step: if slower than 1 m/s inside 150 ms before the opponent's contact, a 0.18 s hop starting at contact, with a first-step acceleration boost for 0.25 s. | Take-off at contact, landing 150 ms later, first foot movement at 265 to 310 ms. | A close match. The hop's 0.18 s air time is within the 120 to 200 ms range. |
| Stamina drains only while sprinting and regenerates otherwise; it scales shot speed. | Fatigue appears as slower recovery and late-rally slowdown; no big match-long drop. | Reasonable; it could also come from accelerations and long rallies, not sprinting alone. |
| Players are clamped 0.3 m inside the walls and cannot leave the court. | Players play off the glass 1.5 to 2.5 m from it; running out is post-slice. | Fine for the slice. |

### 3.2 AI positioning (`FAiPairTactics`, `FAiInterceptPlanner`, `Data/AiTactics.json`, `Data/Difficulty.json`)

| Today | Real | Gap |
|---|---|---|
| Net depth 2.5 m, back depth 8.5 m. | Net 2 to 4 m (resting around 3 m, closer to put a volley away); back 7.5 to 9 m. | Net depth is at the close end; 3.0 m would leave more room against the lob. |
| Partners 4.5 m apart, the pair centre follows the ball by 0.35 (max 1.5 m). | 3.5 to 5 m apart, narrower in the middle. Shift with the ball. | Close. Spacing could depend on the ball's width. |
| Both partners at the same depth. | At the net the far partner is 0.5 to 1 m deeper. | Missing diagonal stagger. |
| Each slot keeps its side; the middle goes to the forehand facing it. | Pairs switch sides on lobs over one player. | Missing side switch (a slot model change, not a tuning one). |
| Read delay 110 / 180 / 280 ms (Hard / Medium / Easy), then a constant-acceleration run at 0.9 × top speed. | First foot movement at 265 ms (world class) to 310 ms (junior) after the opponent's contact. | Hard AI starts moving earlier than world-class humans. That's fine if read delay means "decision made" and the split step adds the rest; today it doesn't, so the AI's feet start about 150 ms earlier than a person's could. |
| Interception uses the same isotropic speed for every direction. | Backward and sideways moves are slower, and turning costs time. | The planner would accept balls a real player can't reach behind them. If movement becomes direction-dependent, the planner's `TimeToCover` must use the same model or the AI will plan runs it can't make. |
| Formation switches to the net after a deep lob, chiquita, volley, overhead or serve. | Same rules in coaching. | Good. The time to get there comes from movement, which is right. |

### 3.3 Animation (`FPlayerAnimModel`, `FAnimNode_PadelPlayerPose` on `agent-7/metahuman-motion`, `Data/Animation.json`)

| Today | Real | Gap |
|---|---|---|
| Locomotion states Idle, Ready, Run, Sprint, SplitStep. On the MetaHuman motion branch: Epic's mannequin idle, plus walk and jog in four directions (forward, back, left, right), blended by speed and direction on one phase. | Rallies are 70% walking-speed footwork in an athletic crouch, 26% jogging-speed, 4% running. | The walk and jog clips are upright, relaxed gait cycles. A ready-stance player shuffling sideways at 2 m/s will show an upright side-walk. That is the most visible gap. |
| Facing is always the net. | Facing stays square for shuffles and short moves, but turns 45 to 180° for crossovers, lob chases, playing off the glass and overheads. | Needs a facing model (section 5.3). |
| Run and sprint cycles play at speed ÷ authored speed, clamped 0.75 to 1.3. | Real stride length and cadence both rise with speed. | Fine as a fallback; Motion Matching will replace it. At 6 m/s on a 4 m/s jog the clamp gives 5.2 m/s of foot motion, so the feet slide; the sprint clip must exist to avoid that. |
| Walk threshold 0.3 m/s, sprint threshold 4.8 m/s. | Shuffle and crossover are different gaits at the same speed; the choice depends on distance to go and direction, not only speed. | The state should carry the movement mode (section 5.2). |
| The split step is a state while `IsSplitHopping`. | It's a hop plus a directional landing. | Needs a split-step clip with directional landings. |

## 4. Targets to calibrate against

Numbers a native test or the AI soak run (`Padel.Soak`, CLAUDE.md) can check, so realism is measured rather than eyeballed. All per player, AI vs AI on Medium unless noted.

| Metric | Target band | From |
|---|---|---|
| Distance per point | median 6 to 12 m; 95th percentile under about 30 m | [M] Ramón-Llin 2017, 2020/21 |
| Average speed while the ball is in play | 0.9 to 1.3 m/s | [M] Ramón-Llin 2017 |
| Share of rally time below 2 m/s | about 65 to 75% | [M] Ramón-Llin 2017 (walking 70%) |
| Share of rally time above 4 m/s | under 5% | [M] Ramón-Llin 2017 (running 4%, sprint 0%) |
| Peak speed per match | median 4 to 5 m/s, never above 7 m/s | [M] Miralles 2025 |
| Peak acceleration | 4 to 8 m/s² | [M] Miralles 2025 (lower bound), [E] sprint mechanics (upper) |
| Accelerations above 1.1 m/s² per minute of ball in play | about 15 to 20 | [M] Miralles 2025: 395/h of match at about 33% ball in play |
| Split steps | at nearly every opponent contact when not already running | [T] |
| Time from the opponent's contact to first foot movement (AI, Hard) | 250 to 300 ms | [T] Mecheri 2019 |
| Server's distance vs partner's | server higher | [M] Ramón-Llin 2019 |

Peak speed over a single move, as a test for the acceleration curve, from standing [E]:

| Move | Peak speed | Time |
|---|---|---|
| 1 m shuffle | 1.5 to 2.5 m/s | 0.5 to 0.7 s |
| 2 m sideways | 2.5 to 3.5 m/s | 0.7 to 0.9 s |
| 4 m (crossover) | 4 to 4.5 m/s | 1.1 to 1.3 s |
| 8 m forward | 5 to 5.5 m/s | 1.7 to 1.9 s |

## 5. Proposals for movement and AI

These change shared contracts (`Data/Movement.json`, `Data/AiTactics.json`, GDD 6.3) or another area's code, so each is its own PR by the area that owns it (CLAUDE.md: one agent, one area).

### 5.1 Acceleration that falls with speed (Gameplay, `FPlayerMotion`)

Replace the constant acceleration with one that is strongest from standing and drops toward zero at top speed, for example `a = a0 × (1 − v / vTop)`, with `a0` set so the 5 m and 10 m times stay where they are. Rating-99 initial acceleration should stay under about 9 m/s². Calibrate to the table at the end of section 4 with a native test. The AI's `TimeToCover` uses the same curve (closed form: `x(t) = vTop × (t − τ(1 − e^(−t/τ)))`, `τ = vTop / a0`).

### 5.2 Direction-dependent speed and a movement mode (Gameplay and AI)

Movement direction relative to facing the net decides the gait and its top speed. Proposed new `Movement.json` fields (starting values [E]):

```jsonc
"ShuffleSpeedFrac": 0.6,        // sideways, square to the net
"BackpedalSpeedFrac": 0.45,     // backwards, still facing the net
"TurnToRunS": 0.18,             // hip turn before a crossover or turn-and-run
"CrossoverMinDistM": 2.5,       // shorter moves shuffle; longer ones turn and run
"TurnBackMinDistM": 1.5         // backwards further than this: turn, don't backpedal
```

`FPlayerMotion` would expose the mode (`Adjust`, `Shuffle`, `Crossover`, `Backpedal`, `TurnAndRun`, `Run`, `Sprint`, `Decel`) for animation. For the human player the mode comes from stick direction and how long it has been held; for the AI from the planned distance and direction. The planner's time-to-reach includes `TurnToRunS` whenever the move needs a turn. Sprint (RT) keeps adding 15% to whichever mode is active.

### 5.3 A facing model (Animation, with a field from Gameplay)

Facing stays the net by default, and turns toward the direction of travel in `Crossover`, `TurnAndRun` and `Run`, up to 180° for a run to the back glass. During a glass read the player faces side-on to the coming ball (section 2.8). The swing system already reads the side from where the ball is; facing only changes the body, not the shot.

### 5.4 AI positioning (AI, `Data/AiTactics.json` and `FAiPairTactics`)

- `NetDepthM` 2.5 → 3.0, with the ball-side player stepping in to about 2.5 to volley.
- A diagonal stagger: at the net, the partner away from the ball 0.5 to 1 m deeper (`NetStaggerM`).
- Spacing narrows to about 3.5 m when the ball is central and widens to 5 m when it's wide (`PartnerSpacingMinM`, `PartnerSpacingMaxM`).
- Side switching on a lob over one player: the partner crosses behind and the slots swap until the point ends or the pair resets.
- Read delay as "decision time" and the split step as the rest: Hard's first foot movement should come at about 250 ms, not 110 ms. Either raise the delays (about 0.20 / 0.26 / 0.34 s) or add the split-step landing time to `StartDelayS`. Keep Hard's advantage in anticipation and prediction quality rather than raw reaction.

### 5.5 Fatigue (Gameplay)

Add a small drain for each hard acceleration and for each second of rally over about 15 s, recovering in the 15 to 20 s between points. Keep the match-long effect small (a few percent of top speed and acceleration at most) since the data shows little neuromuscular fatigue across a match.

### 5.6 Realism checks in the soak run (AI)

Report the section 4 metrics from `Padel.Soak` next to shots per point and the off-the-glass share, so changes to movement are measured.

## 6. Proposals for animation

### 6.1 Locomotion set

The Epic walk and jog clips are a reasonable stopgap for "a body that moves". For padel they need replacing, in this order of how often each shows during a rally:

1. **Ready-stance idle** (bouncing, weight shifts, racket up), at the net and at the back.
2. **Ready-stance shuffle** left and right, short and long, with starts, stops and reversals.
3. **Small adjustment steps** in every direction (0.2 to 1 m).
4. **Split step** with directional landings (left, right, forward, back) and a standing-still version.
5. **Decelerations and plants** from shuffle and run, into the ready stance and into a set step.
6. **Crossover start** left and right, from the ready stance, into a run.
7. **Turn-and-run back** for a lob, both sides, with the head watching the ball.
8. **Retreat side-on** to the back glass and **follow the ball out** off the glass, forehand and backhand side.
9. **Side-glass read** (moving parallel to the side glass, opening toward it) and the **corner pivot**.
10. **Forward run to the net** ending in a decel and split at 3 to 4 m.
11. **Overhead positioning:** side-on chassé and crossover backwards from the net, both directions; set step under the ball; jump and scissor-kick landing.
12. **Recovery steps** out of the follow-through, both sides, into the ready stance.
13. **Between-points walk** (relaxed, upright: the only place the upright walk belongs) and the walk back to position.
14. **Pair moves:** advancing together and the side switch need nothing special per player, but a recorded pair session gives the AI tuning a real reference.
15. **Out-of-court run** (post-slice): sprint to the door, play the ball, run back in.

### 6.2 Motion Matching database

0005 already plans Motion Matching on a trajectory at −0.4 to +0.6 s. For padel:

- Record long takes of the moves above in the real stance, not a clip per move: Motion Matching needs coverage of transitions (shuffle into crossover, decel into split).
- Tag frames with the movement mode from 5.2 and use it as a channel, so a shuffle and a crossover at the same speed are told apart.
- Add facing (5.3) to the trajectory, since padel's facing and travel direction differ much more than in most games.
- The future trajectory window (+0.6 s) covers about half the time between shots, which is enough to pick the right start step.

### 6.3 Mocap shot list additions

[MOCAP_SHOT_LIST.md](MOCAP_SHOT_LIST.md)'s locomotion takes cover ready idle, shuffle, backpedal, run, sprint, split step and the net approach. This doc suggests adding (owner: animation):

| Take | What |
|---|---|
| `loco_adjust` | Small adjustment steps in all directions in the ready stance, at the net and at the back |
| `loco_crossover` | Crossover starts from the ready stance into a run, both ways, plus decels back to ready |
| `loco_turn_run_back` | Turn and run back for a lob, both sides, watching the ball, ending side-on |
| `loco_glass_read` | Retreat side-on to the back glass, wait, follow the ball out, both sides; side-glass and corner versions |
| `loco_overhead_setup` | Side-on chassé and crossover back from the net, set under the ball, jump and scissor-kick landing |
| `loco_recovery` | Recovery steps out of a follow-through into the ready stance, both sides |
| `loco_between_points` | Relaxed walk, picking up and bouncing the ball, walking back to position |
| `pair_session` (two players) | Advancing to the net together, falling back on a lob, the side switch, closing the middle |

Same rules as the shot list: no imitation of a real player's signature movement, a right-handed player only (left-handers are mirrored), 60 fps or more.

## 7. Glossary for movement

- **Ready position:** the athletic stance between shots (2.1).
- **Split step:** the hop at the opponent's contact (2.2).
- **Shuffle:** side step without crossing the feet (2.3).
- **Crossover:** first step that crosses the feet to run sideways (2.4).
- **Turn-and-run:** turning the hips to run back rather than backpedal (2.4).
- **Set step:** the last step before a groundstroke (2.6).
- **Recovery:** getting back to the home position after a shot (2.7).
- **Chassé:** a side-on skip step backwards for an overhead (2.10).
- **Side switch ("cross"):** partners swapping sides after a lob (2.12).

## 8. Follow-ups, by owner

| # | What | Owner (area) | Contract change? |
|---|---|---|---|
| 1 | Speed-dependent acceleration (5.1), with a native test against section 4's single-move table | Gameplay (`PadelCore/Shots`) | `Data/Movement.json` fields |
| 2 | Direction-dependent speed and the movement mode (5.2) | Gameplay, then AI for the planner | `Movement.json`, GDD 6.3 |
| 3 | Facing model (5.3) | Animation | none beyond 2 |
| 4 | AI net depth, stagger, spacing, side switch, read delay (5.4) | AI | `AiTactics.json`, `Difficulty.json`, GDD 8 |
| 5 | Fatigue from accelerations and long rallies (5.5) | Gameplay | `Movement.json`, GDD 6.1 |
| 6 | Realism metrics in the soak report (5.6) | AI | none |
| 7 | Mocap additions (6.3) and the Motion Matching channels (6.2) | Animation | none |

## 9. Sources

Padel:

- Miralles R, Martínez-Gallego R, Guzmán J, Ramón-Llin J (2025). Movement patterns and player load: insights from professional padel. *Biology of Sport* 42(1):163–169. [termedia.pl](https://www.termedia.pl/Movement-patterns-and-player-load-insights-from-professional-padel,78,54169,1,1.html)
- Ramón-Llin J, Guzmán JF, Llana S, James N, Vučković G (2017). Analysis of padel rally characteristics for three competitive levels. *Kinesiologia Slovenica* 23(3):39–49. [dlib.si](https://dlib.si/stream/URN:NBN:SI:DOC-5AF0I23E/2589bc9e-7f55-4f50-9fdc-e7dde35dff3a/PDF)
- Ramón-Llin J, Guzmán JF, Llana S, Martínez-Gallego R, James N, Vučković G (2019). The effect of the return of serve on the server pair's movement parameters and rally outcome in padel using cluster analysis. *Frontiers in Psychology* 10:1194. [frontiersin.org](https://www.frontiersin.org/articles/10.3389/fpsyg.2019.01194/pdf)
- Mellado-Arbelo O, Baiget E (2022). Activity profile and physiological demand of padel match play: a systematic review. *Kinesiology* 54(1):51–61. [hrcak.srce.hr](https://hrcak.srce.hr/file/397460) (also the source for Torres-Luque 2015, García-Benítez 2016, Castillo-Rodríguez 2014 and the heart-rate studies cited here)
- Escudero-Tena A, Antona F, Muñoz D (2023). Performance in padel. *Aspetar Sports Medicine Journal* 12. [journal.aspetar.com](https://journal.aspetar.com/en/journals/volume-12-targeted-topic-sports-medicine-in-padel/performance-in-padel) (source for Ramón-Llin 2020/21 distance per point and Sánchez-Alcaraz 2021 shots per point)
- Game dynamics in men's and women's professional padel: volume, intensity and time parameters at Premier Padel (2026). *E-balonmano.com*. [revista-ebalonmano.unex.es](https://revista-ebalonmano.unex.es/index.php/ebalonmano/article/view/2535)
- Paulauskas R, Šakinis D, Figueira B (2025). Padel vs. tennis doubles: a comparison of performance demands and game attributes. *Frontiers in Sports and Active Living* 7. [frontiersin.org](https://www.frontiersin.org/journals/sports-and-active-living/articles/10.3389/fspor.2025.1540424/full)
- Pradas F, Muñoz D, García-Giménez A, Moreno-Azze A, Sánchez-Alcaraz BJ, Sánchez-Pay A (2025). Physical fitness in professional padel: performance differences according to players' gender and playing side position. *Biology of Sport* 42(4):231–239. [termedia.pl](https://www.termedia.pl/Physical-fitness-in-professional-padel-performance-differences-r-naccording-to-players-gender-and-playing-side-position,78,56155,1,1.html)
- Sánchez-Alcaraz BJ et al. (2025). Physical parameters in young competitive padel players: strength, power, agility, and smash velocity assessments. *Sports* 13(4):104. [mdpi.com](https://mdpi.com/2075-4663/13/4/104)
- Marcos-Rivero B, Yanci J, Granados C, Picabea JM, Ascondo J (2025). Evolution of physiological responses and fatigue analysis in padel matches according to match outcome and playing position. *Sensors* 25(17):5240. [mdpi.com](https://mdpi.com/1424-8220/25/17/5240)

Tennis (split step and response time):

- Mecheri S, Laffaye G, Triolet C, Leroy D, Dicks M, Choukou MA, Benguigui N (2019). Relationship between split-step timing and leg stiffness in world-class tennis players when returning fast serves. *Journal of Sports Sciences*. [accepted manuscript](https://pure.port.ac.uk/ws/files/13666713/Mecheri_et_al._Accepted.pdf) (also the source for Avilès 2002 and 2014, Uzu 2009 and Nieminen 2014)
- Filipčič A, Leškošek B, Filipčič T (2017). Split-step timing of professional and junior tennis players. *Journal of Human Kinetics*. [johk.pl](https://johk.pl/?p=4117)

Gaps in the literature worth knowing: no padel study measures shuffle versus sprint speed, step length, the time to turn, positions relative to the glass, or partner spacing directly. Those values above are tagged [E]. A tracking session with the pair from 6.3 would replace most of them.
