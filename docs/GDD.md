# Game Design Document: 2v2 padel sim (working title "Padel Street")

Status: **v0.2: engine switched to Unreal Engine 5, art direction realistic**. Owner: Agent 0 (game designer).
Every agent reads this before building anything. When the code and this doc disagree, this doc wins until it is changed by a PR that Caio approves.

Sections marked **[SLICE]** are in the first milestone. Everything else is later and is here so the slice is built in a way that won't block it.

---

## 1. Vision

An original 2v2 padel game with the depth and presentation of a modern football sim: real rules, real court, a full shot vocabulary, player ratings that matter, and broadcast-style presentation. It should feel like padel, where points are won by patience, position and the glass, not by whoever hits hardest.

**Pillars**

1. **The glass is the game.** Rebounds off the back and side walls are readable, consistent and playable. Defending from the back glass and turning defence into attack is the core skill.
2. **Two players, one pair.** Positioning as a pair (both up, both back, cover the middle) matters as much as shot-making. The AI partner must feel like a good teammate.
3. **Timing over button mashing.** Every shot has a timing window. Ratings set how forgiving a player is, but a human's timing and choices decide the point.
4. **Readable at a glance.** Score, serve, side and ball are always clear, on a TV from the couch.

**Originality rule (hard requirement).** No names, players, teams, tournaments, sponsors, logos, likenesses, UI layouts or assets from any existing game, sports league, tour or brand. All players, clubs and tournaments are invented. Before shipping a name, search it to make sure it isn't a real padel player, tour, club or racket brand.

**Engine:** Unreal Engine 5 (C++ first, Blueprints only for thin presentation wiring). Chosen for a realistic, football-sim-level look and direct console support later.

**Platform (assumed for the slice):** Windows PC with a gamepad, keyboard as a fallback. 60 fps at 1080p on a mid-range desktop GPU (an RTX 3060 class card). Consoles come after the slice.

---

## 2. The court [SLICE]

Real dimensions from the padel regulations. All units are metres.

| Element | Value |
|---|---|
| Playing area (inside) | 20 long × 10 wide |
| Net height | 0.88 at the centre, 0.92 at the posts |
| Service line | 6.95 from the net (3.05 from the back wall) |
| Centre service line | Splits each service area in two; extends 0.20 past the service line toward the back wall |
| Line width | 0.05 (lines count as in) |
| Back wall | Glass, 3.0 high, full 10 width |
| Back fence | Metal mesh on top of the back glass, up to 4.0 total |
| Side walls, first 2.0 from each back wall | Glass, 3.0 high |
| Side walls, next 2.0 | Glass, 2.0 high (the "step") |
| Rest of the side walls | Metal mesh, 3.0 high |
| Exits (doors) | One opening on each side of each half, at the net end of the side glass, about 2.0 wide (exact gap per court variant) |
| Clear height above court | 8.0 for the indoor slice court (ceiling is a fault if hit) |

**Coordinate system (all code uses this):** Unreal's own axes and units. Z is up and distances are in **centimetres** (this doc's tables are in metres for readability; multiply by 100 in code). The court is centred on the origin. Length runs along X (−1000 to +1000 cm), width along Y (−500 to +500 cm). The net is the plane X = 0. **Team A** (the human's team in the slice) plays the +X half at the start of the match. Ground is Z = 0.

**Surfaces** each have their own bounce response (starting values, to be tuned in data files):

| Surface | Restitution (normal) | Friction (tangential) | Notes |
|---|---|---|---|
| Artificial grass with sand | 0.72 | 0.55 | Spin changes the bounce: topspin kicks forward, slice skids low |
| Glass | 0.68 | 0.15 | Predictable, keeps most speed, spin mostly kept |
| Metal mesh | 0.30 | 0.70 | Dead and slightly random (±15° deflection, ±20% speed) |
| Net | Absorbs most energy | n/a | Ball can roll over the tape (let on serve, in play in a rally) |
| Joints between panels | n/a | n/a | Later: small chance of an unpredictable bounce |

**Ball:** diameter 0.065, mass 0.057 kg, lower pressure than a tennis ball so it bounces a little lower. **Racket:** solid, perforated face, max 0.455 long, 0.26 wide. The racket's sweet spot matters for the contact-quality model (section 5).

---

## 3. Rules [SLICE]

The referee is engine code and must implement every rule below with a unit test.

### 3.1 Scoring

- Points go 0, 15, 30, 40, game.
- **Golden point (default on):** at 40-40 one deciding point is played. The **receiving pair chooses** which of its players receives it (the AI chooses for an AI pair; the human chooses for the human pair with a prompt).
- **Advantage scoring** is a match option (off by default).
- **Games and sets:** a set is the first to 6 games with a 2-game lead. At 6-6 a **tiebreak** is played: first to 7 points with a 2-point lead.
- **Match:** best of 3 sets (option: 1 set for quick play and tests). An optional "super tiebreak" (first to 10, lead of 2) instead of a third set is a match option.
- **Change of ends** after the 1st, 3rd, 5th... game of each set, and every 6 points in a tiebreak.

### 3.2 Serve

- The serving team keeps the serve for a whole game. Serve alternates between teams each game, and the serving order (A1, B1, A2, B2) is kept for the whole set. A team may change its order at the start of each set.
- The first point of each game is served from the **right** half (as you face the net), then alternates every point.
- The server stands behind the service line, between the centre line and the side wall, with at least one foot touching the ground and not touching the line.
- The server **bounces the ball once** on the ground behind the service line, then hits it **underhand, at or below waist height**.
- The serve must go **diagonally** over the net and bounce inside the receiver's service box (lines are in).
- After that bounce the ball may hit the glass. If it hits the **mesh** first after the bounce, it is a **fault**.
- A serve that touches the net and lands in the right box is a **let** (replayed), unless it then hits the mesh, which is a fault.
- **Two serves per point.** Two faults is a double fault and the point goes to the receivers.
- Missing the ball entirely on the serve swing is a fault.
- The receiver must let the serve **bounce** before returning it. Volleying the serve loses the point. The receiver's partner can stand anywhere.
- The tiebreak: the first point is served from the right by the next server, then each player serves two points in turn, starting from the left.

### 3.3 Rally rules

A ball is **good** if it goes over the net and bounces first on the ground of the opponents' half (lines are in).

After that first bounce the ball **may hit any wall, glass or mesh on that half, any number of times**, and the receiving team may play it at any time before its **second bounce on the ground**.

A team may play the ball **off its own walls**: a player can hit the ball into their own glass or mesh so it rebounds over the net (the "contrapared"). It must then cross the net and bounce on the opponents' ground without touching anything else on the opponents' side first.

Partners hit alternately or not at all: only **one touch per team**, and either partner may take it.

**A team loses the point when:**

| Code | What happened |
|---|---|
| `DOUBLE_BOUNCE` | The ball bounces twice on its ground before it plays the ball |
| `INTO_NET` | Its shot doesn't clear the net |
| `WALL_BEFORE_BOUNCE` | Its shot hits the opponents' glass or mesh before bouncing on the opponents' ground (often called hitting it "glass on the fly"). Only contact with the opponents' walls counts here. |
| `OUT_DIRECT` | Its shot leaves the court without bouncing on the opponents' ground first (over the walls, through an exit door, or wide of the court), or hits the ceiling, lights or a post. If the ball touches the opponents' wall on its way out, the first contact decides: wall first is `WALL_BEFORE_BOUNCE`. |
| `OWN_SIDE` | Its shot bounces on its own ground, or hits its own mesh or wall and doesn't come back over |
| `BODY_HIT` | The ball hits one of its players' bodies or clothing before the second bounce (whether it's in play or a shot heading out) |
| `DOUBLE_HIT` | One of its players hits the ball twice, or both partners touch it |
| `NET_TOUCH` | A player, their racket or clothing touches the net or posts while the ball is in play |
| `CROSSED` | A player crosses the net plane into the opponents' half while the ball is in play (reaching over is allowed only for the case below) |
| `RACKET_THROWN` | A player hits the ball with a racket that left their hand |
| `DOUBLE_FAULT` | Serving team, two faults |
| `VOLLEYED_SERVE` | Receiving team, it volleyed the serve |

**Special cases:**

- **Out-of-court winner (`OUT_OF_COURT_WINNER`):** if the ball bounces legally in the opponents' half and then leaves the court, over the back wall (the "by three" smash) or through an exit door or over the side mesh (the "by four" smash), it is **still in play**. The receiving team may run out of the court and return it before it bounces a second time outside, if the court allows out-of-court play. If they don't return it, the hitting team wins the point.
  - **[SLICE]:** the referee detects this correctly and awards the point to the hitter. The player running outside the court to return it is **post-slice**.
- **Ball comes back on its own:** if the ball bounces in the opponents' half and spin or the glass carries it back over the net, the receivers may reach over the net to play it without touching the net. If they don't, the hitting team wins the point. [SLICE: no reach-over, hitter wins.]
- **Ball hits the top of the wall or a joint and stays in:** still in play.
- **Ball gets stuck in the mesh or a gap:** the point is won by the team that hit it if it bounced first in the opponents' half, otherwise lost.

### 3.4 Referee events

The match simulation broadcasts an event for each of these so the UI, audio, replays and stats can listen without touching simulation code. In Unreal they are multicast delegates on a `UPadelMatchEvents` world subsystem, named `On` + the event in PascalCase (`ball_bounced` becomes `OnBallBounced`). The event names below are the contract (Agent 2 owns them, Agent 7 consumes them):

`serve_started(server_id)`, `serve_fault(reason)`, `serve_let()`, `ball_hit(player_id, shot_type, timing_grade, speed, spin)`, `ball_bounced(surface, half, position)` with `surface` in {`ground`, `glass`, `mesh`, `net`, `ceiling`}, `ball_left_court(position)`, `point_ended(winning_team, reason_code, last_hitter_id)`, `score_changed(score_snapshot)`, `game_won(team)`, `set_won(team)`, `match_won(team)`, `change_of_ends()`, `golden_point_started(receiving_team)`.

---

## 4. Shots [SLICE except where marked]

Each shot is a row in the shots data table (`Data/Shots.json`, imported as a DataTable) with base speed range, base spin, base error, timing window widths, and a list of the attributes that drive it. No shot values are hard-coded.

| Shot | When it's available | Button (pad / keys) | Speed (km/h) | Spin | Intent | Key attributes |
|---|---|---|---|---|---|---|
| **Drive** | Ball at waist height after a bounce or off the glass | A / J | 50–100 | Flat or topspin | Main groundstroke from the back. Forehand or backhand is chosen by the ball's side. | Drive or Backhand, Power |
| **Volley** | Same button as the drive when the ball is taken before it bounces | A / J | 30–90 | Flat or slice | Block or punch at the net, keep the opponents back | Volley, Reach |
| **Lob** | Any ball below shoulder height | B / K | 25–55, apex 6–8 m | Slice or topspin | Push the net team back, the most important defensive shot | Lob, Touch |
| **Bandeja** | Ball above the head and the player isn't fully behind it | Y (tap or half charge) / L | 50–80 | Slice | Controlled overhead that keeps the net position. Lands deep, bounces low off the back glass. | Bandeja, Reach |
| **Víbora** | High ball on the player's forehand side | RB + Y / E + L | 70–110 | Heavy slice and side spin | Aggressive overhead aimed at the side glass; the ball skids and stays low | Víbora, Power |
| **Smash** | High ball and the player is set under it | Y at full charge / hold L | 90–140 | Flat or topspin | Finish the point. Aimed deep and flat it can bounce over the back wall ("by three"); aimed at the side with topspin it can exit through the door ("by four"). | Overhead, Power, Jump |
| **Chiquita** | Any low ball when the opponents are at the net | LB + A / Q + J | 20–40 | Slice or flat | Soft, low ball to the opponents' feet so they must volley up. The way to win the net back. | Touch, Composure |
| **Off-the-glass return** | Any shot played after the ball rebounds off the player's own glass | Same buttons, chosen by height | Same as the base shot | Same | The defensive core. A drive, lob or chiquita taken after the ball comes off the back or side glass. | Wall Play, plus the base shot's attributes |
| Drop shot (dejada) | Ball near the net | X / I | 15–35 | Backspin | Dies after the bounce | Touch | post-slice |
| Contrapared | Pull the stick toward your own back wall while hitting a drive | Drive with stick pulled back | 30–60 | Flat | Emergency: hit into your own back glass so it rebounds over | Wall Play | post-slice |
| Wall exit (bajada) | High rebound off the own back glass | RB + A | 70–110 | Topspin | Attacking shot from a high ball off the glass | Wall Play, Drive | post-slice |
| Rulo / gancho | Topspin overhead near the net | RB + Y with forward aim | 70–100 | Topspin | Attacking overheads | Overhead | post-slice |

**Serve [SLICE]:** A / J. Two types: flat-slice (default, aim at the receiver's body or the side glass) and kick (RB, higher bounce off the back glass). Aim with the left stick inside the box, power by hold time.

**Context rules for the overhead button (Y):** the shot system picks from the ball's contact height, how set the player is under the ball (feet still, behind the contact point) and the charge level:
- Not set, or charge under 60%: **bandeja**.
- RB held: **víbora**, if the ball is on the forehand side. On the backhand side, RB + Y plays a backhand overhead (treated as a bandeja with lower accuracy).
- Set under the ball and charge at 100%: **smash**. If not set, a full-charge overhead becomes a bandeja with extra error.

---

## 5. Contact model: how a shot is computed [SLICE]

When a shot fires, the shot system produces a **target point**, a **speed** and a **spin** and gives them to the ball engine. The ball engine then solves for the launch velocity that reaches the target and simulates it. The player's quality changes the **intended** values and adds **scatter**. Errors are emergent: a scattered target that lands out or in the net is just an error, not a dice roll.

### 5.1 Timing

Holding a shot button starts the charge and the swing. **Releasing** the button sets the moment of contact. The difference between release and the ideal contact moment (when the ball reaches the best contact point near the player) is graded:

| Grade | Window (base, before attributes) | Effect |
|---|---|---|
| Perfect | ±40 ms | Error × 0.5, speed +5% |
| Good | ±90 ms | Error × 1.0 |
| Early / Late | ±160 ms | Error × 1.8; early pulls the ball cross-court, late pushes it the other way (side depends on forehand or backhand) |
| Miss | outside | Mishit: random weak ball, often into the net or a short pop-up |

If the button is held through the whole window, the swing fires automatically at the late edge. The relevant shot attribute widens all windows by up to 35% (attribute 99) or narrows them by up to 20% (attribute 1). A small timing cue near the player shows the grade after every shot.

### 5.2 Position

The ideal contact point is about 0.8 m in front and to the side of the player, at the shot's preferred height. Being out of position multiplies error by `1 + 0.6 × distance_from_ideal_m`, and limits power to `1 − 0.3 × distance` (minimum 0.5). Running at full speed through the shot multiplies error by 1.3. A split-step before the opponent's contact (section 6) removes this penalty for the next shot.

### 5.3 The formulas (starting values, tuned in data)

Let `S` be the shot's main attribute (0–1, from rating/99), `P` = Power/99, `C` = Composure/99, `St` = current stamina (0–1).

- **Max speed** = `shot.speed_max × lerp(0.75, 1.10, P) × stamina_factor(St)`. Actual speed = max speed × charge (charge 0–1).
- **Spin** = `shot.base_spin × lerp(0.7, 1.2, S)`.
- **Scatter radius at the target (metres)** = `shot.base_error × (1 − 0.7 × S) × timing_mult × position_mult × power_mult × pressure_mult`
  - `power_mult` = 1.0 up to 80% charge, rising to 1.6 at 100%. Hitting hard is a risk.
  - `pressure_mult` on break points, set points and golden points = `1 + 0.4 × (1 − C)`, otherwise 1.
- **Wall shots** (any shot after a rebound off own glass) multiply scatter by `lerp(1.6, 0.9, WallPlay/99)` and use Wall Play instead of the base shot's attribute for the timing windows.
- **stamina_factor**: 1.0 above 50% stamina, falling linearly to 0.85 at 0%.

---

## 6. Players and ratings [SLICE: 4 fixed players]

Ratings run from 1 to 99, like a football sim. Each player is a row in `Data/Players.json`.

### 6.1 Attributes

| Group | Attribute | What it changes |
|---|---|---|
| Technical | **Drive** | Forehand groundstroke error, spin and timing windows |
| | **Backhand** | The same, on the backhand side |
| | **Volley** | Volley error and timing windows; how well hard balls are blocked |
| | **Lob** | Lob depth control (how close to the back glass it lands without going over) |
| | **Overhead** | Smash accuracy, and the chance a smash bounces out of the court |
| | **Bandeja** | Bandeja error and depth |
| | **Víbora** | Víbora error and how much side spin it gets |
| | **Wall Play** | Timing windows and error after rebounds off own glass (5.3); for the AI, how well it predicts rebounds |
| | **Touch** | Chiquita, drop shot and soft lob error; how low the chiquita stays |
| | **Serve** | Serve error, speed, and how often it reaches the side glass |
| | **Return** | Error on the first shot after a serve |
| Physical | **Speed** | Top running speed, 4.5 m/s (1) to 6.5 m/s (99) |
| | **Acceleration** | Time to top speed, 0.9 s (1) to 0.45 s (99) |
| | **Agility** | Turn rate, side-step speed, split-step recovery |
| | **Power** | Max shot speed (5.3) |
| | **Reach** | Max contact height and side reach (height plus arm and racket), 0.9 m to 1.2 m sideways |
| | **Jump** | Extra overhead contact height, and how often a smash can be a jump smash |
| | **Stamina** | How quickly sprinting and long rallies drain stamina, and how quickly it comes back between points |
| Mental | **Anticipation** | How soon the player reacts after the opponent's contact (AI read delay, human auto split-step quality) |
| | **Positioning** | AI movement to the right spot as a pair; for the human's player, the strength of the "home position" assist when no input is given |
| | **Composure** | The pressure multiplier on big points (5.3) |
| | **Consistency** | Shrinks the random part of scatter, so errors come from bad choices more than bad luck |
| | **Teamwork** | Partner coverage of the middle and calling; the baseline for pair chemistry (post-slice) |

**Profile (not ratings):** handedness (left or right), **preferred side** (right side, the "drive" side; or left side, the "revés" side, usually the more aggressive overhead player), play style tag (Wall, Net, Power, Touch, All-round), height, and up to 2 traits (post-slice).

### 6.2 Overall rating

Overall = weighted average of the attributes, with weights depending on the preferred side:
- Right side: heavier on Drive, Lob, Wall Play, Consistency, Volley.
- Left side: heavier on Overhead, Víbora, Bandeja, Power, Backhand.

A player playing on the wrong side plays at −4 overall (applied as −6% to their technical attributes).

### 6.3 Split-step and movement

- If a player has stopped (or slowed below 1 m/s) inside 150 ms before the opponent's contact, they split-step automatically: a small hop that gives full Agility for the first step and removes the moving-shot penalty on the next shot.
- Human-controlled movement is direct (the left stick is the direction), with acceleration from the Acceleration attribute. When the stick is released, the player drifts toward a "home position" (section 8) at a speed set by Positioning.
- Sprint (RT / Shift) adds 15% top speed and drains stamina.

### 6.4 The slice roster

Four invented players, two pairs, each around 75–82 overall, with contrasting styles so the AI's behaviour reads clearly:
- Team A: a right-side wall defender and a left-side power player. The human controls one of them (choice on the pairing screen, default the left side).
- Team B: an all-round right-side player and a left-side touch player.

Agent 7 and the asset agent design their names and looks. Names must pass the originality check in section 1.

---

## 7. Controls [SLICE]

Gamepad first; everything is reachable on a gamepad. Labels use the Xbox layout and must show the right glyphs for PlayStation and Switch pads.

| Action | Gamepad | Keyboard |
|---|---|---|
| Move / aim | Left stick | WASD |
| Sprint | RT | Shift |
| Drive / volley / serve | A | J |
| Lob | B | K |
| Overhead (bandeja / smash) | Y | L |
| Drop shot (post-slice) | X | I |
| Attack modifier (víbora, kick serve, wall exit) | RB | E |
| Touch modifier (chiquita) | LB | Q |
| Partner: go to the net | D-pad up | 1 |
| Partner: stay back | D-pad down | 2 |
| Partner: take the middle | D-pad left | 3 |
| Partner: "yours" (let your partner take the next ball) | D-pad right | 4 |
| Golden point side choice | Left stick + A | A/D + J |
| Pause | Start | Esc |

**Aim:** the stick direction at the moment of release sets the target relative to the camera: up = deep, down = short, left/right = across. No input = a safe target in the middle of the opponents' half. The target is shown only in the debug overlay, never in normal play.

**Power:** by hold time (charge 0% to 100% over 0.6 s). The charge also is the swing, so timing and power are linked: a longer charge needs an earlier press.

**Player switching:** the human always controls the same player. There is no switching to the partner.

**Rebinding** and dead-zone settings: post-slice. Agent 1 sets these up with Enhanced Input, one Input Action each: `IA_Move`, `IA_Sprint`, `IA_ShotDrive`, `IA_ShotLob`, `IA_ShotOverhead`, `IA_ShotDrop`, `IA_ModAttack`, `IA_ModTouch`, `IA_CmdNet`, `IA_CmdBack`, `IA_CmdMiddle`, `IA_CmdYours`, `IA_Pause`, in one mapping context `IMC_Match` (menus use CommonUI's input routing).

---

## 8. AI [SLICE]

**Pair tactics:** the AI pair has a formation state:
- **Both at the net** (about 2–3 m from the net) when they've played a good attacking shot or a deep lob gets them time to advance.
- **Both back** (about 1–2 m in front of the back glass) when defending, especially after a lob over them.
- **Split** (one up, one back) is a mistake state that the AI avoids and the opponents should punish (aim through the gap).
- Partners move together, keep about 4–5 m apart side to side, and shift toward the ball. The player whose side the ball is on takes it; balls down the middle are taken by the forehand of the player on the left by default, unless the partner is clearly better placed (Teamwork decides how cleanly this works).

**Home positions** come from the formation, the ball's position and which side each player plays.

**Shot choice:** a weighted choice that favours, in order:
- Opponents both at the net and the ball is low: lob (deep) or chiquita (if the ball is in front).
- Ball high and the player is at the net: bandeja (default), víbora (left-side players, aggressive personality), smash when the ball is short and high.
- Pinned at the back glass: a lob or a safe drive from off the glass.
- Opponents split: aim through the middle or at the player at the net's feet.

**Prediction:** the AI uses the ball engine to simulate the ball forward (with the same deterministic code) and picks the reachable interception point, including rebounds off the glass. Wall Play adds noise to the predicted rebound.

**Difficulty levels** (rows in `Data/Difficulty.json`):

| | Easy | Medium | Hard |
|---|---|---|---|
| Read delay after opponent contact | 280 ms | 180 ms | 110 ms |
| Timing grade distribution | Mostly good, often early/late | Mostly good, some perfect | Mostly perfect |
| Shot choice | Often the safe shot | Mixed | Tactical |
| Extra aim scatter | × 1.4 | × 1.0 | × 0.8 |
| Attribute scale | −10 | 0 | +5 |

**Partner commands** (D-pad): go to the net, stay back, take the middle, "yours". The partner acknowledges each with a short animation or voice line, and follows it until the end of the point or the next command.

**Determinism requirement:** AI decisions use a seeded random number generator owned by the match, so a replay or a soak test with the same seed plays the same way.

---

## 9. Camera [SLICE]

- **Default: broadcast end camera.** Behind and above the human team's back glass, looking down the court, about 7–9 m high and 6–8 m behind the glass, with a slight tilt so the near players don't hide the far half. Gentle horizontal tracking of the ball (no more than ±1.5 m), no rotation during a point.
- **The human's team is always at the bottom of the screen.** On a change of ends the camera moves to the other end during the change-over (with a short transition shot), so "up on the stick" always means "toward the opponents".
- **Between points:** short cut-aways (close-up of the server, the scoreboard). No camera moves while the ball is live.
- **Replays (post-slice):** side camera along the net, ball cam.
- Camera settings (height, tracking strength) are in a data file.

---

## 10. Art direction

**Decided: realistic, at the level of a modern football sim.** Real materials and lighting (Lumen), believable athletes, motion-captured movement, and TV-style presentation. The look to aim for is a televised indoor padel match.

- **Readability first, even when realistic:** each team has one strong kit colour, and the ball is always the brightest thing on screen (bright yellow, with a subtle motion trail at high speed). The glass shows reflections and edge highlights but never hides the ball.
- **Court:** one indoor club court for the slice, built to the dimensions in section 2. Blue artificial grass with visible sand, white lines, dark aluminium frame and mesh, tempered glass with real reflections, arena lighting. Signage around the court uses invented brands only.
- **Characters:** realistic athletes built with MetaHuman (or an equivalent the asset agent picks), with invented faces that resemble no real person. Real sports clothing proportions and cloth, sweat and hair detail later. Rackets of invented designs.
- **Animation:** motion capture for every swing and movement, with Motion Matching for locomotion so players move and plant their feet like real athletes.
- **Performance budget:** realism must still hold 60 fps on the target PC; the broadcast camera's fixed angle helps (see section 9).
- **UI:** a clean broadcast look: a compact score bug in one corner, big type for the score on change-overs.
- **Audio:** the pop of the ball on the racket face, the thud and ring of the glass, the rattle of the mesh, shoes on sand, a small indoor crowd. Sound is part of readability (the glass and the mesh must sound different).

The existing browser prototype in this project (`padel-street/index.html`, a neon arcade game with "street shots") is a separate arcade experiment. The sim does not inherit its arcade rules.

---

## 11. Modes, in priority order

1. **[SLICE] Quick Match vs AI:** human + AI partner vs 2 AI. Options: difficulty, match length (1 set or best of 3), golden point on/off, which player the human controls.
2. **Quick Match, full:** local multiplayer for 2 to 4 people on one machine (any mix of humans and AI on each team), more courts, a practice mode with a ball machine and drills (the ball machine also serves Agent 3's testing).
3. **Tutorial:** short drills that teach timing, the lob, the bandeja and playing off the glass.
4. **Career:** create a player, choose a preferred side, start in local club tournaments and climb an invented tour with ranking points. Pick and change partners (partner chemistry and side preferences matter), train to raise attributes, manage stamina and a tournament calendar, earn invented sponsor deals. Saves.
5. **Team building ("club" mode):** collect original players, build pairs, chemistry between partners from side preference, play style and time played together, and a fair economy with no pay-to-win: everything can be earned by playing, and no paid random packs.
6. **Online 2v2:** each player controls one player, 2 to 4 people per match. The engine is deterministic and fixed-step from the start so rollback netcode stays possible. The choice between rollback and server-authoritative is made when this mode starts.

---

## 12. Vertical slice: scope and definition of done

**In:** one indoor court, four invented players with fixed ratings, Quick Match vs AI from the main menu to the stats screen, all [SLICE] rules and shots, three AI difficulties, partner commands, broadcast camera, placeholder-to-first-pass art, basic audio, gamepad and keyboard.

**Out:** career, team building, online, local multiplayer, out-of-court play by players (the rule itself is in), drop shot, contrapared, wall exit, overhead variants beyond the four listed, replays, commentary, rebinding, more courts.

**The slice is done when every item is true:**

1. **A full match plays out:** from the main menu, a best-of-3 match vs AI is played to the stats screen with only a gamepad, no crash and no soft-lock.
2. **The score is always right:** at least 30 rule tests pass (one or more for every row in section 3), and the AI-vs-AI soak test (100 matches, headless, fixed seeds) finishes with zero stuck points and zero referee invariant failures (for example, a point always ends with exactly one winner and a reason code).
3. **Every slice shot can be played on purpose:** drive, volley, lob, bandeja, víbora, smash, chiquita, serve (both types), and each of them off the glass. Verified against the ball machine in practice and in a match.
4. **The rally feels like padel:** on Medium, AI vs AI rallies average 6 to 12 shots; at least 25% of points include a shot off the glass; lobs and the fight for the net decide points, not just smashes.
5. **It runs well:** 60 fps on the target PC, input-to-swing response under 100 ms.
6. **It reads well:** in a playtest, Caio can always tell the score, who is serving, and where the ball is.
7. **Caio signs off on feel** after a playtest using the checklist in `docs/PLAYTEST.md`.

---

## 13. Open decisions for Caio

I've picked a default for each so work isn't blocked. Change any of them and the doc gets updated.

1. **Art direction.** ~~Stylised or realistic?~~ Decided: realistic, in Unreal Engine 5 (section 10).
2. **Name and brand.** Does the sim use the "Padel Street" name from the arcade prototype, or should it get its own name? (Default: "Padel Street" as a working title only.)
3. **Platform.** Windows PC with a gamepad first (default). Which consoles after the slice? Unreal supports them directly, but each needs a platform developer account.
4. **Partner commands.** Are these four right: go to the net, stay back, take the middle, "yours"? (Default: yes.)
5. **Slice match length.** Best of 3 with golden point (default), or 1 set to keep test matches short? Both are options; this sets what the main menu offers first.
