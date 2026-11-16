# Biomechanics of padel strokes

How a padel player's body produces every stroke in the game, and what that means for the shot system, the animation model and the motion-capture sessions. This file covers the **strokes**; movement and footwork between shots are covered separately.

It builds on GDD 4 (shots), 5 (contact model) and 10 (animation), [0004](../decisions/0004-shot-system.md) (swing phases), [0005](../decisions/0005-player-animation.md) (contact-frame sync) and [0006](../decisions/0006-metahuman-players.md) (MetaHuman retarget). It changes no code or data: the proposals in section 5 each need their own PR.

All descriptions are for a **right-handed** player; a left-hander is the mirror (0005 already mirrors clips). Positions are in the player frame the animation data uses: **right**, **forward** (toward the net) and **up**, in metres from the point on the ground between the feet.

## How much of this is measured

Padel has far less published biomechanics than tennis. Most padel papers are match analysis, ball speed by radar, injury epidemiology or muscle activity. No study we found reports full-body 3D kinematics (joint angles and angular velocities) for padel strokes. So each number below is tagged:

- **[P]** measured in padel (source in section 7).
- **[T]** measured in tennis and transferred. Padel strokes use the same proximal-to-distal kinetic chain, so tennis values are a good envelope for *order and timing*, but padel swings are shorter and slower (section 1.2).
- **[E]** an estimate from coaching material, video and physics, to be replaced by our own mocap numbers. The mocap sessions in section 6 are set up to measure exactly these.

---

## 1. What is common to every stroke

### 1.1 The kinetic chain

Every stroke, from a chiquita to a smash, is a **proximal-to-distal sequence**: each segment speeds up, peaks and slows down, handing its momentum to the next one out.

1. **Ground and legs:** knee and hip extension push against the ground, and the step or weight shift moves the centre of mass toward the ball. In the tennis serve, knee extension velocity is the strongest single correlate of ball speed (r = 0.75) [T, Wong 2014].
2. **Pelvis rotation**, then **trunk rotation** (and trunk lateral flexion on overheads). The gap between them is the hip-shoulder separation ("X-factor") that stretches the trunk muscles.
3. **Shoulder:** horizontal adduction on groundstrokes, internal rotation on overheads.
4. **Elbow** extension, **forearm** pronation (forehand side) or supination (backhand side).
5. **Wrist:** held laid back (extended) through contact on groundstrokes and volleys; flexion and ulnar deviation snap on overheads.

In elite tennis forehands, pelvis angular velocity peaks about **75 ms** before impact (about 540°/s) and trunk about **57 ms** before impact, while shoulder internal rotation peaks **after** impact [T, Landlinger 2010]. Better players peak later and closer together; less skilled players fire the pelvis and trunk too early, which loses racket speed. In the tennis serve the distal peaks are much higher: shoulder internal rotation about 2,400°/s, wrist flexion about 1,950°/s, elbow extension about 1,500°/s, upper torso about 870°/s, pelvis about 440°/s, with peak shoulder external rotation (layback) about 170° [T, Fleisig 2003].

**For animation,** this order is what makes a swing read as real at broadcast distance: hips open first, shoulders follow, the arm lags, the racket head lags the hand and arrives last. A swing where everything rotates together reads as a robot or a placeholder.

### 1.2 How padel differs from tennis

| | Padel | Effect on the stroke |
|---|---|---|
| Racket | Solid perforated face, max 45.5 cm long, about 340–375 g, short handle, wrist cord | Shorter lever: for the same angular velocity the head moves about 25–35% slower than a 68.5 cm tennis racket [E]. Less dwell and less grip on the ball than strings, so less spin per unit of brush [E]. One hand on the grip almost always. |
| Ball | Slightly lower pressure than a tennis ball | Slower off the racket and the floor; the game is played at lower ball speeds. |
| Court | 20 × 10 m, glass and mesh walls | Shots are taken after the glass; players **wait** for the ball, so swings are more compact and better timed, and the backswing is often limited by the glass behind. |
| Serve | Bounced, underhand, contact at or below the waist (GDD 3.2) | No overhead serve: the serve is a short pendulum swing (section 2.1). |
| Overheads | Ball usually not killed outright; the walls return it | Most overheads are **control** overheads (bandeja, víbora) that keep the net, not smashes. |
| Grip | **Continental** (the "hammer" grip) for almost everything | One grip for volleys, overheads, serve and slice; a slightly more eastern forehand grip on some topspin drives. No grip change between shots is needed in animation. |
| Stance | Players face the net much more than in tennis | Shoulder turn does most of the work; feet often stay more open. |

Measured frequencies show what matters most [P, Martín-Miguel 2024, professional matches, share of points that contain the shot]: lobs 76–86%, backhand volley 59–78%, forehand volley 57–64%, direct (no wall) backhand 67–75% and forehand 60–69%, bandeja 44–66%, back-wall forehand 42–48% and backhand 29–36%, flat smash 20–33%, topspin bandeja 16–23%, bajada 14–25%, double-wall returns 11–23%, contrapared 7–13%, smash por 3 4–9%. Points last 13.5–16 s at about 0.75–0.8 shots per second, so about **1.25 s between shots**. That pace is why swings must be compact, and why the recovery after a shot matters as much as the swing itself.

### 1.3 Speed and spin from the racket

- **Ball speed** comes from racket-head speed at the contact point, and racket-head speed is mostly angular velocity times lever length (shoulder to sweet spot, about 0.9–1.0 m with the arm extended, against about 1.2 m in tennis). For an overhead into a slow ball, ball speed is roughly **1.2–1.4 × racket-head speed** [E], so a 130 km/h smash (36 m/s) needs about 26–30 m/s at the sweet spot.
- **Spin** comes from the angle between the racket path and the racket face normal. Topspin: path low-to-high with a near-vertical or slightly closed face. Slice (backspin): path high-to-low with an open face. Side spin: path across the ball (outside-in for a forehand slice). The tangential share of racket speed sets how much spin; the normal share sets the pace. A flatter path through the ball means more pace and less spin, for the same racket speed.
- **The wrist** is the fine control: a laid-back wrist held firm through contact gives a stable face (groundstrokes, volleys); an active wrist and forearm snap at contact (víbora, smash) adds head speed and spin, at the cost of control. Experts keep the wrist extended through a backhand, while novices flex it [P, padel musculoskeletal review 2024].

### 1.4 Phases used in this document

These are the phases of [0004](../decisions/0004-shot-system.md) and the clip times of [0005](../decisions/0005-player-animation.md):

| Phase here | 0004 phase | Clip time in `AnimClips.json` |
|---|---|---|
| **Prep** (read the ball, unit turn, first steps, set the feet) | `Idle` / end of locomotion | before 0 |
| **Backswing** (racket back, load the legs and trunk) | `Windup`, scrubbed by charge | 0 → `WindupS` |
| **Forward swing** (the chain fires, racket accelerates) | `Forward` | `WindupS` → `ContactS` |
| **Contact** (a few ms; the face angle and path at this instant decide the ball) | contact tick | `ContactS` |
| **Follow-through** (the chain decelerates; the racket finishes) | `FollowThrough` | `ContactS` → `EndS` |
| **Recovery** (back to the ready stance, split-step for the next ball) | `Recovery` | holds `EndS`, blends out |

---

## 2. Stroke by stroke

Each stroke lists: when it's used and its `Shots.json` row; grip; stance and footwork; the phases with timings; the kinetic chain; key angles; racket path and face; the contact point; how speed and spin are made; and what an **early** or **late** version looks like (the three clip versions of 0005).

Angles: knee flexion is 0° straight; elbow flexion is 0° straight; shoulder abduction is the arm away from the side (90° = horizontal); wrist extension is the back of the hand toward the forearm ("laid back"). Face angle: 0° is vertical to the ground, "open" tilts the strings up, "closed" tilts them down.

### 2.1 Serve (flat-slice and kick) — `serve_flat`, `serve_kick` [SLICE]

**Use:** start every point. Underhand, the ball bounced once behind the service line, contact at or below the waist (GDD 3.2). The default is a **slice serve** to the receiver's body or the side glass; the kick has topspin for a higher bounce off the back glass.

- **Grip:** continental.
- **Stance:** side-on or slightly open, the left foot forward and angled toward the target box, about shoulder-width apart, behind the service line. At least one foot stays on the ground through contact (the rule); most servers keep the back foot's toe down and step through *after* contact.
- **Prep:** bounce the ball a couple of times, look at the target. Ball held in the left hand at about hip height, in front and to the right of the front foot.
- **Ball drop:** the ball is dropped (not thrown) from hip height, about 0.4–0.6 m in front of the front foot and 0.5–0.7 m to the right. It bounces and rises back to about 0.6–0.9 m [E]. The game drops from `ServeTossHeightM` (1.7 m), which is higher than most real drops (≈ 0.8–1.1 m from the hand); the higher toss gives the player more time, worth keeping, but the animation drop should start at hand height and the ball should then fall faster than it looks thrown.
- **Backswing (0.3–0.5 s):** as the ball drops, the racket swings back like a pendulum to about hip height behind the body, the shoulders turn 40–60° away from the target, weight on the back foot, knees bend to 30–50° [E]. Racket face already open for the slice.
- **Forward swing (0.2–0.35 s) [E]:** weight shifts onto the front foot, the hips then the shoulders rotate back toward the target, the arm swings forward from the shoulder with a nearly fixed elbow (≈ 20–40° flexion) and a firm wrist. Little forearm rotation on the slice; more pronation and a faster low-to-high brush on the kick.
- **Contact:** at the top of the bounce or slightly after it, at **0.6–0.9 m high** (never above the waist), about **0.3–0.5 m in front** of the front foot and **0.6–0.8 m to the right** of the body centre. `Shots.json` has 0.75 m preferred, 0.3–1.1 m band: right.
- **Racket path and face:** slice: path slightly high-to-low or level, face open 10–25°, often with an outside-in component for side spin toward the side glass. Kick: path low-to-high 20–35°, face square to slightly closed, brushing up the back of the ball.
- **Follow-through (0.3–0.5 s):** toward the target and up to shoulder height, then the back foot steps through and the server moves toward the net (servers almost always follow their serve to the net).
- **Speed and spin:** 50–85 km/h. Pace comes from weight transfer and shoulder flexion; the wrist adds little. Slice is the default; the kick trades pace for topspin.
- **Early / late:** early: contact while the ball is still rising, in front, the face closing → a lower, faster, flatter ball. Late: contact as the ball falls beside the body, the arm lifting more → a floaty, short serve.

### 2.2 Forehand drive (derecha) — `drive`, fore [SLICE]

**Use:** groundstroke from the back after a bounce, usually a controlled, low ball to the net players' feet or through the middle. Padel drives are compact and controlled; full tennis forehands are rare because pace into a net team is risky and the glass returns the ball.

- **Grip:** continental to eastern forehand.
- **Stance and footwork:** a unit turn first (shoulders and hips together, racket back), then set the feet side-on to semi-open. The back (right) foot plants first as the anchor, the left foot steps toward the ball. Players stay low and wait for the ball to drop into the waist zone.
- **Backswing (0.3–0.5 s):** compact: the racket head goes back to about shoulder height and no further than the back hip (a big loop hits the glass and costs time). Shoulders turn **70–90°** from facing the net, hips **40–60°**, so the X-factor is about **20–30°** [E/T]. Elbow flexed **90–120°**, wrist laid back **30–45°**. Knees **30–50°**, weight loaded on the back leg. The left arm extends across the body pointing at the ball.
- **Forward swing (0.18–0.30 s):** push from the back leg; the pelvis opens first (peak ≈ 75 ms before contact [T]), the trunk follows (≈ 55 ms [T]), the arm is dragged by the trunk with the racket lagging the hand, the elbow extends to **140–160°** (never fully straight). The left arm folds in to stop the trunk rotation, which is what speeds up the arm. Tennis forward swings are about 0.32 s [T, Landlinger 2010]; padel's are shorter [E].
- **Contact:** **0.2–0.4 m in front** of the front foot, **0.6–0.8 m to the right** of the body centre, at **0.8–1.0 m** (waist) height. Racket face near vertical; wrist still laid back. `Shots.json` preferred 0.9 m, band 0.2–1.5 m: right. GDD 5.2's "0.8 m to the side, 0.3 m in front" matches the drive.
- **Racket path and face:** flat drive: path low-to-high **5–15°**, face vertical. Topspin drive: path **15–30°** with the face vertical to 5° closed, and forearm pronation rolling the face over the ball after contact. Slice drive (common on low balls): path high-to-low, face open 10–20°.
- **Follow-through (0.3–0.4 s):** across the body, finishing at the left shoulder or above for topspin, lower and more forward for a flat drive. Shorter than tennis: the follow-through stops by the left shoulder and the racket comes back to ready fast.
- **Speed and spin:** 50–100 km/h. Pace is mostly leg drive and trunk rotation; topspin is path angle plus pronation.
- **Early:** contact well in front, the hips and shoulders already open, the face closing → pulled cross-court, dipping. **Late:** contact beside or behind the front hip, the elbow cramped, the face open → pushed down the line, floating. This matches GDD 5.1's early pull / late push.

### 2.3 Backhand drive (revés) — `drive`, back [SLICE]

**Use:** groundstroke on the left. Mostly **one-handed with slice** in padel; a two-handed topspin backhand exists but is a minority.

- **Grip:** continental (one-handed); the left hand supports the throat in the backswing and lets go before the forward swing.
- **Stance:** more side-on than the forehand: the right foot steps across toward the ball (closed stance), back turned toward the net about **90–110°** at the end of the backswing.
- **Backswing (0.3–0.5 s):** the left hand guides the racket back at shoulder height (slice) or hip height (topspin), elbow flexed **90–110°**, the racket head above the hand for slice. Knees **30–50°**.
- **Forward swing (0.18–0.28 s):** the shoulders uncoil but **stop short of facing the net**; the arm extends from the shoulder (horizontal abduction and external rotation), the forearm supinates slightly, the elbow ends nearly straight (**160–175°**) at contact. The wrist stays **extended and firm** through contact: experts do this, novices flex the wrist and load the lateral elbow [P, padel review 2024]. The left arm moves back as a counterbalance, the chest stays side-on.
- **Contact:** further in front than the forehand, **0.3–0.5 m in front** of the front (right) foot, **0.5–0.7 m to the left** of the body centre, at 0.6–1.0 m.
- **Racket path and face:** slice: high-to-low **20–35°**, face open **10–25°**. Topspin: low-to-high, face square.
- **Follow-through (0.3 s):** out toward the target and up, the arm extended, the left arm back; the body stays sideways ("the photograph"), then turns back to ready.
- **Speed and spin:** 40–90 km/h, slice by default.
- **Early:** contact too far in front, the arm straight and reaching, the face closing → cross-court. **Late:** contact beside the body, the elbow bent, the wrist breaking → a weak, open-faced ball down the line.

### 2.4 Volleys (voleas) — `volley`, fore and back [SLICE]

**Use:** at the net before the bounce; the most common shot in professional padel with the lob. Mostly **block and punch with slice** to push the opponents back; a few are angled winners.

- **Grip:** continental.
- **Stance and footwork:** split-step as the opponent hits, then a unit turn (shoulders turn **30–60°**, not the full groundstroke turn). The step **into** the volley is with the opposite foot: the left foot steps forward and across for a forehand volley, the right foot for a backhand. The step is what gives the volley its pace, not the arm.
- **Backswing (0.1–0.2 s):** tiny: the racket head stays in front of the body and above the hand, going back no further than the shoulder plane. Elbow flexed **90–110°**, wrist fixed laid back **20–40°**.
- **Forward swing (0.08–0.15 s) [E]:** a short punch forward and slightly down, driven by the step and the shoulder turn. The elbow extends only a little (to **120–150°**); the wrist does not move.
- **Contact:** **0.3–0.6 m in front** of the body, to the side (forehand **0.4–0.6 m right**, backhand **0.3–0.5 m left**). **Height varies most of all the shots** (0.2–2.0 m in the data): high volleys at shoulder height with the elbow up, low volleys at shin height with deep knee bend (**60–90°**), the body lowering rather than the racket dropping.
- **Racket path and face:** high-to-low **15–40°**, face open **10–30°** (more open for low volleys, nearly square for high punches). Block volley (against a hard ball): almost no swing, face set, grip firmer.
- **Follow-through (0.15–0.25 s):** short, toward the target, the racket head still above the wrist; then straight back to ready.
- **Speed and spin:** 30–90 km/h; slice from the open face and downward path.
- **Early:** contact far in front, arm extended, the face closing → into the net or cross. **Late:** contact beside the body, the elbow jammed into the ribs, the face open → a weak pop-up.

### 2.5 Lob (globo) — `lob`, fore and back [SLICE]

**Use:** the main defensive shot (in 76–86% of professional points [P]): over the net team, landing deep so they must retreat. Often taken low and after the back glass.

- **Grip:** continental.
- **Stance:** side-on or open; low centre of mass. Knees **50–80°** for low balls.
- **Backswing (0.25–0.45 s):** short, the racket back and **low**, below the ball, the face already open.
- **Forward swing (0.2–0.3 s):** a lift: knee and hip extension raise the body, the shoulder flexes, the arm swings up from below the ball with the elbow nearly fixed (**20–40°** flexion) and a firm wrist. The trunk rotates less than in a drive.
- **Contact:** **0.2–0.4 m in front**, **0.5–0.7 m to the side**, at **0.3–1.0 m** (often low; `Shots.json` preferred 0.8 m, band 0–1.6 m).
- **Racket path and face:** path low-to-high **40–60°**, face open **30–50°**. Topspin lob (attacking, rarer): a fast, steep low-to-high brush with a near-vertical face and pronation.
- **Follow-through (0.35–0.5 s):** long and **high**, the racket finishing above the head toward the target, the body rising onto the toes. This high finish is the lob's readable signature.
- **Speed and spin:** 25–55 km/h, apex 6–8 m; slice by default.
- **Early:** contact in front, the face closing → short, low lob, punished by a smash. **Late:** contact beside or behind, the face very open → a high, short "balloon".

### 2.6 Bandeja — `bandeja`, fore and back [SLICE]

**Use:** the signature padel overhead: a controlled slice overhead that keeps the net position against a lob that can't be smashed. Lands deep, bounces low off the back glass. The backhand version is a defensive backhand overhead.

- **Grip:** continental.
- **Stance and footwork:** turn sideways early (left shoulder to the net), move back with **side steps or crossovers**, never backpedal facing the net. Get slightly behind and to the right of the ball. The feet often keep moving through the shot: the body goes forward or sideways after contact rather than landing in place.
- **Backswing (0.35–0.55 s):** the "tray" carry that gives the shot its name: the racket comes up early in front of the body with the face open, the elbow at or a bit below shoulder height (shoulder abduction **80–100°**), elbow flexion **90–110°**, the racket head behind and above the head. The left arm points up at the ball. Trunk extension and rotation are **moderate**: no big arch, no full layback.
- **Forward swing (0.2–0.3 s):** the trunk rotates **less than a smash** (shoulders finish about side-on to 45°), the shoulder brings the arm forward and slightly down, the elbow extends (to **140–165°**, not fully straight). Little forearm pronation and a fixed wrist: the shot is about control, not head speed.
- **Contact:** **in front and to the right**, at about the 1 o'clock position as seen from behind, **0.3–0.5 m in front** of the head, at **2.1–2.5 m** (about 0.3–0.5 m below full reach) [E]. `Shots.json` preferred 2.3 m: right for a 1.8 m player.
- **Racket path and face:** from high-right to low-left and forward, **cutting** the ball: path high-to-low **15–30°** with an outside-in component, face open **15–30°**. Topspin bandeja (16–23% of points [P]): the same carry, face square or slightly closed, a brush up and over.
- **Follow-through (0.3–0.4 s):** forward and across toward the left hip but checked early; the body keeps going toward the net or sideways. The finish is balanced and upright.
- **Speed and spin:** 50–80 km/h; slice with a little side spin.
- **Backhand bandeja:** the back is turned to the net, the elbow leads up and out, the racket swings from above the left shoulder out to the left, with supination; weak and accurate rather than powerful (GDD: ×1.4 error).
- **Early:** contact too far in front and low, the face closing → a short ball that sits up. **Late:** contact above or behind the head, the arm bent back → no slice, a floaty ball that comes back off the glass.

### 2.7 Víbora — `vibora`, fore [SLICE]

**Use:** the aggressive cousin of the bandeja: heavy slice and side spin, aimed at the side glass so the ball skids and stays low.

- **Grip:** continental.
- **Stance:** as the bandeja, but players often let the ball drop a little more and are slightly further to the left of it, so contact is **more to the side**.
- **Backswing (0.35–0.5 s):** the racket goes higher and further behind than the bandeja; more trunk rotation (shoulders turn **90°+**), the elbow higher (abduction **90–110°**), the wrist cocked (extended and radially deviated).
- **Forward swing (0.15–0.25 s):** much more **trunk rotation and arm speed** than the bandeja. The arm comes through from high-right to low-left with forearm **pronation** and a wrist snap (flexion and ulnar deviation) through contact: the whipping motion the padel injury literature links to elbow stress [P, padel review 2024].
- **Contact:** **further to the right** and a little lower than the bandeja, about the 2 o'clock position, around head height to **0.3 m above it** (**1.9–2.4 m**); in front of the body by **0.2–0.4 m**. `Shots.json` preferred 2.4 m is high: about 2.1–2.2 m reads more like a real víbora [E].
- **Racket path and face:** a steep diagonal, **30–60°** high-to-low and strongly outside-in; face open **15–25°** with the edge leading, brushing the outside of the ball (its 3 to 4 o'clock side, seen from behind).
- **Follow-through (0.3–0.4 s):** across the body to the **left hip**, the racket finishing low; the right shoulder comes through toward the net.
- **Speed and spin:** 70–110 km/h; the heaviest combined slice and side spin in the game.
- **Early:** contact in front, the face closing → into the net or the middle. **Late:** contact behind the head → no side spin, sails long.

### 2.8 Smash: flat, por 3 and por 4 — `smash`, fore [SLICE]; kick smash / rulo [post-slice]

**Use:** finishing overhead when **set** under the ball. Flat smash in 20–33% of professional points; por 3 (bounce out over the back wall) in 4–9% [P]. Measured smash speeds: semi-professionals **133 km/h**, amateurs **125 km/h** without an opponent, falling to 121 and 105 km/h with one [P, Rivilla-García 2019]; trained players 133 → 146–156 km/h after a training block [P, Sáez de Villarreal 2023].

- **Grip:** continental.
- **Stance and footwork:** turn side-on early, retreat with crossover steps, get **behind** the ball so it will come down 0.3–0.5 m in front of the head. Feet about shoulder-width, the right foot back. On a deep lob, a jump with a **scissor kick** (the right leg drives up and forward, the left leg goes back, landing on the left foot).
- **Backswing (0.4–0.6 s), to the "trophy position":** left arm up pointing at the ball, the racket dropped behind the head; shoulder abducted **90–100°**, elbow flexed **90–110°**, the shoulder **externally rotated** (layback) toward **150–170°** at its peak as the trunk starts forward (tennis serve ≈ 170° [T, Fleisig 2003]). Knees bent **20–40°** if standing (less than the 60–70° of a tennis serve: less time), hips and shoulders turned 90°+ from the net, trunk tilted back and to the left.
- **Forward swing (0.2–0.3 s):** the full chain, as in the tennis serve: leg drive (and the jump), pelvis then trunk rotation and **lateral flexion** toward the left, the left arm pulls down to the chest, then shoulder internal rotation (the fastest joint motion in sport, ≈ 2,400°/s in the serve [T]), elbow extension (≈ 1,500°/s [T]), forearm pronation and wrist flexion (≈ 1,950°/s [T]). The racket drops behind the back (the "back scratch") and whips up.
- **Contact:** at **full extension**, the arm straight above and slightly right of the head (shoulder abduction **≈ 110°** with trunk tilt making the arm near vertical), **0.2–0.5 m in front** of the head, at about standing reach plus racket: **≈ 1.35 × body height + 0.3 m** [E], i.e. **2.6–2.8 m** for our 1.78–1.86 m players, plus 0.3–0.4 m with a jump. `Shots.json` preferred 2.7 m and `JumpExtraHeightM` 0.4: right.
- **Racket path and face:**
  - **Flat:** path forward and down **10–30°**, face square to slightly closed, pronation through contact. Most of the racket speed goes into pace.
  - **Por 3 (x3):** the flattest and hardest: contact as high and as far in front as possible, the path steeply down so the ball bounces near the service line and rises high off the back glass and out over it. Maximum leg and trunk drive; the body falls forward after contact.
  - **Por 4 (x4) and the kick smash (rulo, post-slice):** contact further **behind and to the left** of the head, the back more arched, the path **low-to-high and across** (the "brush up the back" of a kick serve), strong pronation and ulnar deviation, face square to closed. Topspin and side spin make the ball kick toward the side and exit through the door or over the side wall.
  - **Gancho (hook, post-slice):** a forehand-side overhead hit from beside the head with the arm more horizontal, the trunk side-bending, at the net.
- **Follow-through (0.4–0.5 s):** down and across the body to the **left hip**, the arm pronated (the palm facing out at the end); the right leg comes through to land, the body continues forward. On a jump, land on the left foot then the right.
- **Speed and spin:** 90–140 km/h in the data, up to about 150+ for strong players [P]. Speed is legs, trunk and internal rotation; it's limited mostly by contact height and being set.
- **Early:** contact in front and low, the trunk already folded → into the net. **Late:** contact behind the head, the back over-arched → a sky-high ball with no pace.

### 2.9 Chiquita — `chiquita`, fore and back [SLICE]

**Use:** a soft, low ball to the net team's feet so they must volley up; how a back team wins the net.

- **Grip:** continental.
- **Stance:** low, side-on to semi-open; knees **40–70°**.
- **Backswing (0.2–0.35 s):** short, the racket at or below hip height, face slightly open.
- **Forward swing (0.15–0.25 s):** slow and controlled: little trunk rotation, the shoulder and a firm wrist do the work, the racket **decelerates** into contact ("soft hands", a lighter grip).
- **Contact:** **0.2–0.4 m in front**, **0.5–0.7 m to the side**, at **0.3–0.8 m**.
- **Racket path and face:** nearly level or slightly low-to-high, face open **5–15°**; a little slice to keep the ball low after the bounce.
- **Follow-through (0.2–0.3 s):** short, toward the target, at about chest height.
- **Speed and spin:** 20–40 km/h; racket-head speed only about **6–10 m/s** [E].
- **Early:** contact in front, the face closing → into the net. **Late:** the face opens → the ball rises and sits up for a volley winner.

### 2.10 Off-the-glass returns: back wall, side wall, double wall [SLICE as drive/lob/chiquita]

**Use:** any groundstroke after the ball rebounds off the player's own glass. Back-wall forehands and backhands are in 30–48% of points, side-wall backhands in about 20%, double-wall returns in 11–23% [P]. The **stroke** at contact is the drive, lob or chiquita above; the difference is everything before it.

- **Reading and prep:** the player lets the ball pass, **turns with it** (a pivot on the back foot so the chest faces the side glass), and moves back **with** the ball, keeping about **1.0–1.5 m** from the glass. Then they **wait**, low, for the ball to come off the glass and drop into the contact zone, and step **forward** into the shot. Prep takes longer than for a direct ball, and the feet move toward the ball after the turn.
- **Back wall:** the ball comes off the back glass toward the net; the player hits it as it comes forward, side-on, with the backswing shortened by the glass behind (**racket head no further back than the back hip**).
- **Side wall:** the ball comes off the side glass toward the court's centre; the player stays **further from the side glass** than the ball's line (about 1.5 m) and hits it as it comes across, often with an open stance.
- **Double wall:** "open" (side glass, then back glass) and "closed" (back glass, then side glass). Two rebounds slow the ball and change its direction twice; the player must wait the longest and stay low, and often hits a lob or chiquita.
- **Animation:** the *prep* is the off-the-glass part (turn, retreat, wait, step forward); the swing clip is the base shot with a **compact backswing** (`WindupS` earlier and less racket travel).

### 2.11 Bajada (wall exit) — post-slice (`RB + A`)

**Use:** attack a high ball that rebounds off the back glass: instead of a defensive lob, the player waits for the ball to come down off the glass at shoulder-to-head height and hits down and forward, like a smash from the back (14–25% of points [P]).

- **Grip:** continental.
- **Footwork:** turn side-on, retreat **with** the ball toward the glass, let it come off the glass and **fall**, move forward with it, plant the right foot under or behind the ball and step forward with the left.
- **Backswing:** high and behind, like the bandeja or smash carry but taken lower (elbow at shoulder height).
- **Forward swing:** weight transfer forward plus trunk rotation; arm action between a forehand drive and a smash.
- **Contact:** **1.4–2.2 m** high, **0.3–0.5 m in front**, to the right.
- **Path and face:** high-to-forward-and-down **10–25°**; flat or topspin (closed face, pronation), sometimes slice.
- **Follow-through:** across to the left hip, the body moving forward toward the net.
- **Speed:** 70–110 km/h.

### 2.12 Contrapared — post-slice

**Use:** emergency (7–13% of points [P]): the ball has got past the player and is too close to the back glass to return, so the player hits it **into their own back glass** hard enough to rebound over the net.

- **Footwork:** the player is **facing the back glass**, often running back; the last step plants close to the glass.
- **Swing:** short and upward: a forehand flick or (more often) a backhand, the racket going **low-to-high at 30–60°** into the glass at about **1–2 m** height. Mostly arm and wrist; the trunk can't rotate much.
- **Contact:** close to the body, **0.3–0.8 m** high, in front of the player as they face the glass.
- **Face:** open, to send the ball up the glass.
- **Speed:** 30–60 km/h. Recovery: turn back to face the net fast.

### 2.13 Dejada (drop shot) — post-slice (`X`)

**Use:** at or near the net, a ball that dies after the bounce, often off a slow ball or a weak bandeja return. Also a drop volley.

- **Grip:** continental, held **loosely**.
- **Swing:** a bandeja- or volley-like preparation (disguise), then a very short forward swing that **slows down** into contact, the face very open (**30–50°**), cutting under the ball with a high-to-low path. The knees bend to absorb the ball.
- **Contact:** in front, at **0.5–1.5 m** (ground stroke) or the volley heights.
- **Speed and spin:** 15–35 km/h with heavy backspin; the racket-head speed is only **4–8 m/s** [E].

---

## 3. Timing summary against the current clips

Durations in seconds. "Real" ranges are from section 2 ([E], with tennis [T] as the envelope). "Clip" values are today's placeholder rows in `Data/AnimClips.json` (perfect version): backswing = `WindupS`, forward = `ContactS − WindupS`, follow = `EndS − ContactS`.

| Shot | Real backswing | Clip backswing | Real forward | Clip forward | Real follow-through | Clip follow | Verdict |
|---|---|---|---|---|---|---|---|
| Drive | 0.3–0.5 | 0.45 | 0.18–0.30 | 0.17 | 0.3–0.4 | 0.63 | forward a little short |
| Volley | 0.1–0.2 | 0.20 | 0.08–0.15 | 0.12 | 0.15–0.25 | 0.38 | ok |
| Lob | 0.25–0.45 | 0.40 | 0.20–0.30 | 0.20 | 0.35–0.5 | 0.70 | ok |
| Bandeja | 0.35–0.55 | 0.50 | 0.20–0.30 | 0.25 | 0.3–0.4 | 0.65 | ok |
| Víbora | 0.35–0.5 | 0.50 | 0.15–0.25 | 0.22 | 0.3–0.4 | 0.68 | ok |
| Smash | 0.4–0.6 | 0.55 | 0.20–0.30 | 0.25 | 0.4–0.5 | 0.70 | ok |
| Chiquita | 0.2–0.35 | 0.30 | 0.15–0.25 | 0.15 | 0.2–0.3 | 0.50 | ok |
| Serve | 0.3–0.5 | 0.50 | 0.20–0.35 | 0.30 | 0.3–0.5 | 0.60 | ok |

The clip's follow segment (`ContactS` → `EndS`) includes the settle back to ready, so it is longer than the follow-through itself; the shot's `FollowThroughS` (0.2–0.4 s in `Shots.json`) is the gameplay part and sits inside the real ranges.

---

## 4. Mapping onto the game

### 4.1 Shot system (`PadelCore/Shots`, `Data/Shots.json`)

The selector's shot list and the GDD's context rules match how real players choose shots: the overhead family is chosen by **contact height** and **being set** (behind the ball, feet still), which is exactly what separates a smash from a bandeja in real play. Specific checks:

| Topic | What the code does | What the biomechanics says | Fit |
|---|---|---|---|
| Ideal contact point | One point for all shots: 0.8 m to the side, 0.3 m in front (`ShotModel.json`) | Right for the drive and serve. Volleys and the backhand meet the ball further in front (0.3–0.6 m); overheads 0.2–0.5 m in front of the **head**, closer to the body sideways (0.2–0.5 m right for the bandeja and smash, 0.4–0.6 m for the víbora) | **Proposal 5.1** |
| Contact heights | Per shot: drive 0.9, volley 1.1, lob 0.8, bandeja 2.3, víbora 2.4, smash 2.7, chiquita 0.6, serve 0.75 m | All inside the real ranges for a 1.8 m player. Víbora is a bit high (2.1–2.2 m reads better). Overhead heights should scale with player height (1.78–1.86 m in the roster ≈ ±0.06 m) | Good; **5.2** |
| "Set" for the smash | Slower than 1.2 m/s and not behind the ball | Matches: a real smash needs the feet planted **behind** the ball | Good |
| Speeds | Table in GDD 4 | Smash 90–140 km/h against measured 105–156 km/h [P]: right for the slice. Others are inside coaching ranges | Good |
| Early / late | Early pulls cross-court, late pushes the other way | Matches the body: early contact = shoulders open, face closing, pulled across; late = face open, pushed | Good |
| Spin magnitudes | `BaseSpinTop` drive +25, víbora −30 / side +30, smash +10 rad/s (≈ 240–290 rpm) | No published padel spin measurements found; tennis forehands are 1,500–3,000 rpm [T]. Padel's solid face grips less, but real drives, bandejas and víboras still look like **600–2,500 rpm (60–260 rad/s)** [E]. `BallSim` uses a physical Magnus term (`MagnusCoefficient` 0.0007 is the right order for a real ball, C_L ≈ 0.6 × spin ratio), so with today's numbers the flight curve from spin is probably **4–8× too weak** | **Flag for the ball/shots owners (5.5)** |
| Swing on the run | Movement slowed to `SwingMoveMult`; running error ×1.3 | Matches: real players plant for drives; volleys, bandejas and bajadas are often hit while moving forward or sideways | Good; volley and bandeja could allow more movement |

### 4.2 Animation model and contact-frame timing (0005)

0005 drives every clip from one swing progress `P` and shows `ContactS` on exactly the contact tick; the backswing is scrubbed by charge. That matches the body well: **the backswing is where a real player waits** (they hold the loaded position while the ball comes), and **the forward swing is a fixed-duration ballistic movement** once it starts. Two consequences:

1. **The forward swing should never be shorter than about 0.1 s.** The real forward swing lasts 0.08–0.30 s and can't be sped up much by the player: it is already at full speed. Today a **late or perfect release** goes from the backswing to the contact frame in one tick (0005 flags this as the one place to watch). Biomechanically the fix is the one 0005 suggests, in the controller: treat the release as the **start** of the forward swing and make contact `ForwardS` later, with `ForwardS` per shot from section 3 (`ContactS − WindupS`). Grading would then compare `release + ForwardS` with the ideal moment. For the player nothing changes once they learn the timing; the windows and grades stay the same. This changes GDD 5.1's meaning of "release", so it needs a GDD PR first. **Proposal 5.3.**
2. **A charge held at full** holds the trophy or loaded position. Real players can hold the bandeja carry and the smash trophy position for a moment, but not a drive backswing for long. The hold is fine for a 0.6 s charge; a longer hold should add a small idle sway so the pose doesn't freeze.

**Early, perfect and late clips (0005's three versions):** section 2 lists what each looks like per shot. Use them as the direction notes for the mocap takes. The early version is the body **already rotated through** at contact (hips and shoulders open, the racket further along its arc); the late version is the body **still closed**, the contact cramped beside or behind the hip.

**Contact point warp.** A clip meets the ball at one authored point; the game's ball arrives anywhere inside the reach (0.9–1.2 m plus slack) and the shot's height band. Five clips per shot cannot cover a volley band from 0.2 to 2.0 m. Real players adapt with the **legs and trunk** (knee bend for low balls, trunk side-bend for wide ones) more than with the arm. So the pose node needs a contact warp: the offset between the clip's authored racket contact point and the ball's predicted contact point, applied with a weight that rises through the forward swing, peaks at `ContactS` and falls through the follow-through. Distribute it: **vertical** mostly to the pelvis height (knee bend) and spine flexion, **lateral** to spine side-bend and arm two-bone IK, **forward** to a small pelvis shift and the arm. The two-bone IK on the right arm already in `FAnimNode_PadelPlayerPose` (agent-7/metahuman-motion) is the arm part of this. **Proposal 5.4.**

**Height variants.** For volleys and the drive, record low, mid and high versions (section 6) and blend by contact height, then warp the remainder. Overheads need no height variants (the height band is narrower), the warp covers them.

### 4.3 MetaHuman retarget (0006)

- The retarget copies the placeholder skeleton's pose; the **racket is attached to `hand_r` of the MetaHuman**. Different body heights (1.78–1.86 m) mean different arm lengths, so a retargeted clip's racket is a few centimetres off the authored contact point. The contact warp (4.2) runs on the placeholder skeleton, which still decides where the racket is in the simulation; it should use the **MetaHuman's arm length** when computing the IK target, or the racket on screen will miss the ball by the difference. Simplest: compute the warp after the retarget, on the MetaHuman, using the racket socket position.
- The retargeter's IK pass (feet and hands) runs only at LOD 0–1 (`MetaHumanIkLodThreshold`). The contact warp needs the hand IK at every LOD where a swing is visible; that is the broadcast distance of 8–25 m, i.e. LOD 1–2. Keep the contact warp separate from the retargeter's IK, or raise the threshold during swings.
- **Forearm twist:** pronation and supination (smash, víbora, topspin drive, backhand) are rotations of the forearm. MetaHumans have forearm twist bones; the retarget chain should map the forearm roll to the twist bones, or the wrist will candy-wrap on every overhead. Check the smash clip in the retargeter preview first.
- **Left-handers (Ruben):** the mirror must mirror the arm reach too (0006 lists that the arm reach uses the right arm today).

### 4.4 What the AI needs

Agent 4's AI calls the same controller, so it gets the same contact model. The biomechanics adds one thing for realism: the **prep footwork** of section 2 (unit turn, side-on before overheads, the turn-and-wait for off-the-glass balls). That's a movement concern and belongs to the movement thread; the stroke needs only that the feet are set by the start of the forward swing.

---

## 5. Proposals (each in its own PR)

1. **Per-shot ideal contact offsets.** Add `IdealContactSideM` and `IdealContactForwardM` (and backhand overrides) to each `Shots.json` row, defaulting to `ShotModel.json`'s values. Starting values from section 2: drive fore 0.7 / 0.3, drive back −0.6 / 0.4, volley fore 0.5 / 0.45, volley back −0.4 / 0.45, lob 0.6 / 0.3, bandeja 0.4 / 0.4 (in front of the head), víbora 0.5 / 0.3, smash 0.3 / 0.35, chiquita 0.6 / 0.3, serve 0.7 / 0.4. Schema change: its own PR, touching `PadelCore/Shots` and `Data/`.
2. **Scale overhead contact heights by player height.** `PreferredContactHeightM` × `HeightM / 1.82` for the overhead family, or a per-shot `ContactHeightPerBodyHeight`. Lower the víbora's preferred height to about 2.2 m.
3. **A minimum forward swing.** Contact at `release + ForwardS(shot)` instead of at the release, graded the same way (section 4.2). GDD 5.1 change first, then `PadelCore/Shots` and 0005.
4. **Contact warp in the pose node.** Add the authored contact point to each `AnimClips.json` row (`ContactRightM`, `ContactForwardM`, `ContactUpM`, the racket sweet spot at `ContactS`, measured from the mocap take), and warp the pose toward the ball as in 4.2. Animation area only.
5. **Spin magnitudes.** Ask the ball and shot owners to compare `BaseSpinTop`/`BaseSpinSide` against 60–260 rad/s and retune with `MagnusCoefficient`; ideally measure spin on our own mocap takes with a high-speed camera (section 6).
6. **Clips for the post-slice shots** (bajada, contrapared, dejada, rulo, gancho, kick smash) and the off-the-glass prep when those shots are built.

---

## 6. Motion-capture shot list (biomechanics cues)

This extends [`MOCAP_SHOT_LIST.md`](MOCAP_SHOT_LIST.md) (setup, tool, locomotion and celebrations stay there). The take names are `AnimClips.json` row names. For every swing take:

- **Measure, don't guess:** a marker or IMU on the **racket** (head and handle), not only the hand, so the racket path, face angle and head speed at contact come out of the take. Record the contact frame (ball on a string, or audio of the impact; the impact sound is the most precise marker), the **end of the backswing** (`WindupS`), the end of the follow-through and the return to ready (`EndS`).
- **Write down** per take: ball speed off the racket (radar or a phone app), contact height, and a high-speed video (240 fps phone) from the side to read the face angle and estimate spin from the ball's logo/seam.
- **Direction notes:** "early" = contact 0.3–0.4 m further in front, body already rotated through; "late" = contact beside the front hip, body still closed. Five takes each, as today.
- **Body cues to check on playback:** hips open before shoulders, racket head lagging the hand, the left arm working (pointing, then folding), weight moving forward through contact, and the follow-through finishing where section 2 says.

### 6.1 Slice clips (already listed in `AnimClips.json`)

| Take | Cues from section 2 | Extra takes |
|---|---|---|
| `serve_flat_fore_*` | Drop from hip, bounce, pendulum backswing, contact 0.6–0.9 m, face open 10–25°, follow toward target, walk in | One to the body, one to the side glass |
| `serve_kick_fore_*` | Same, low-to-high brush, face square | |
| `drive_fore_*` | Unit turn, compact backswing (no further than the back hip), hips then shoulders, contact 0.2–0.4 m in front at waist, finish at the left shoulder | **Low** (0.4 m) and **high** (1.3 m) versions of the perfect take; flat, topspin and slice versions |
| `drive_back_*` | Closed stance, left hand guides back, wrist firm, contact 0.3–0.5 m in front, "photograph" finish | Low and high; two-handed if a player uses it |
| `volley_fore_*`, `volley_back_*` | Split-step, short turn, step with the opposite foot, punch, racket head above wrist | **Low** (0.4 m, deep knees), **mid** (1.1 m), **high** (1.7 m); a **block** volley against a hard ball |
| `lob_fore_*`, `lob_back_*` | Low, open face 30–50°, lift with the legs, finish high | Topspin lob |
| `bandeja_fore_*` | Turn side-on, crossover back, tray carry, contact 1 o'clock 0.3–0.5 m below full reach, cut, keep moving | Topspin bandeja; bandeja while moving back, and while moving forward |
| `bandeja_back_*` | Back to the net, elbow leads, supination | |
| `vibora_fore_*` | More rotation, contact at 2 o'clock near head height, pronation and wrist snap, finish at the left hip | |
| `smash_fore_*` | Trophy position, layback, full extension, pronation, finish at the left hip | **Flat**, **por 3** (hardest, steep), **por 4 / kick** (behind the head, brush across), with and without a **jump and scissor kick** |
| `chiquita_fore_*`, `chiquita_back_*` | Low, short, slow racket, soft finish at chest | |

### 6.2 Off the glass (prep variants for the same strokes)

Record each as one continuous take starting from the ready stance at the back of the court, with a real ball off real glass (or a wall of the same height):

| Take | What |
|---|---|
| `glass_back_drive_fore`, `glass_back_drive_back` | Pivot, retreat with the ball, wait, step forward, compact drive |
| `glass_back_lob_fore`, `glass_back_lob_back` | Same, lob |
| `glass_side_drive_fore`, `glass_side_drive_back` | Stay off the side glass, open stance, compact drive |
| `glass_double_open_lob_back`, `glass_double_closed_lob_fore` | Two rebounds; the longest wait, low |
| `glass_back_chiquita_fore` | Off the back glass to the net team's feet |

### 6.3 Post-slice strokes (record when convenient, not needed for the slice)

| Take | What |
|---|---|
| `bajada_fore_*`, `bajada_back_*` | Retreat with the ball, let it fall off the glass, attack at 1.4–2.2 m moving forward |
| `contrapared_back_*`, `contrapared_fore_*` | Facing the back glass, upward flick into it at 1–2 m, turn back fast |
| `dejada_fore_*`, `dejada_back_*` | Bandeja-like disguise, very open face, slow hands; also drop volleys |
| `rulo_fore_*` | Topspin overhead near the net |
| `gancho_fore_*` | Hook overhead from beside the head |

---

## 7. Sources

Padel, measured [P]:

- Martín-Miguel, I., Almonacid, B., Muñoz, D., Sánchez-Alcaraz, B. J., Courel-Ibáñez, J. (2024). Game dynamics in professional padel: shots per point, point pace and technical actions. *Sports*, 12(8), 218. https://doi.org/10.3390/sports12080218
- Rivilla-García, J., Muñoz Moreno, A., Lorenzo, J., van den Tillaar, R., Navandar, A. (2019). Influence of the opposition on overhead smash velocity in padel players. *Kinesiology*, 51(2), 206–212. https://doi.org/10.26582/k.51.2.6
- Sáez de Villarreal, E., Ramos-García, D., Calleja-González, J., Alcaraz, P. E., Ramírez-Campillo, R. (2023). Comparison of two 8-week training interventions on the athletic performance of padel players. *Kinesiology*, 55(1), 38–48.
- Musculoskeletal disorders in padel: from biomechanics to sonography (2024), review. https://pmc.ncbi.nlm.nih.gov/articles/PMC11178742/ (smash wrist flexion and pronation, forehand pronation for topspin, expert backhand wrist extension).
- Demeco, A., de Sire, A., Marotta, N., et al. (2023). Risk of injury and kinematic assessment of the shoulder biomechanics during strokes in padel players. *Gait & Posture*, 105(S1), S20–S21 (muscle activation per stroke, abstract).

Tennis, transferred [T]:

- Landlinger, J., Lindinger, S., Stöggl, T., Wagner, H., Müller, E. (2010). Key factors and timing patterns in the tennis forehand of different skill levels. *J Sports Sci Med*, 9(4), 643–651. (Pelvis peak ≈ 541°/s at −75 ms, trunk at −57 ms, shoulder internal rotation peak after impact, racket 31–33 m/s, forward swing ≈ 0.32 s.)
- Fleisig, G., Nicholls, R., Elliott, B., Escamilla, R. (2003). Kinematics used by world class tennis players to produce high-velocity serves. *Sports Biomechanics*, 2(1), 51–64. (Peak angular velocities and layback quoted in 1.1 and 2.8.)
- Wong, F. K. H., et al. (2014). Effects of body mass index and full body kinematics on tennis serve speed. *J Hum Kinet*, 40, 21–28. (Knee extension velocity r = 0.75 and elbow extension velocity r = 0.71 with serve speed.)
- Elliott, B. (2006). Biomechanics and tennis. *Br J Sports Med*, 40(5), 392–396.
- Kovacs, M., Ellenbecker, T. (2011). An 8-stage model for evaluating the tennis serve. *Sports Health*, 3(6), 504–513.

Rules and equipment: the GDD (sections 2 and 3) for the court and the serve; racket dimensions from the international padel rules (max 45.5 cm × 26 cm × 38 mm).

**Gaps worth filling with our own capture:** full-body joint angles and angular velocities for every padel stroke, racket-head speed at contact, and spin rates. None are published that we could find; the mocap sessions in section 6 can measure all three.
