# Playtest notes

Owner: Agent 8 (QA). Updated each week. Caio plays, writes notes at the bottom, and QA turns them into issues for the right agent.

## How to run the build

1. Install Godot 4.7.2-stable (see `README.md`).
2. `git pull` on `main`, then open `project.godot` or run `godot --path .`.
3. Use a gamepad if you have one; GDD section 7 has the controls.

---

## Week of 2026-11-16

### What changed

- Project setup is merged: the regulation court, the broadcast camera view, the input map, GUT tests and CI.
- QA added contract tests: CI now fails if the engine's reason codes or `MatchEvents` signals drift from GDD 3.3 and 3.4, or if engine code uses the global RNG, the wall clock or Nodes.
- The match engine, controls, AI, assets and UI are being built now. Nothing is playable yet.

### What to try

1. Run the project. You should see the empty court from behind and above the near back glass.
2. Does the camera height and angle feel like the broadcast view you want (GDD 9)? The ball and far players will need to read from here.
3. Look at the court proportions: glass height, the 2 m step on the side glass, the door gaps, net height. Anything that looks off against a real court?

### Questions to answer after playing

- Is the camera too high, too low, or about right?
- Does the court read as a padel court at a glance?
- Anything in GDD section 13 (open decisions) you want to change before the engine and AI agents build on it?

### Your notes

_Write anything here, or tell the coordinator. Rough is fine._

---

## Checklist for the slice sign-off (GDD 12 item 7)

Use this once a full match is playable. Answer each one with yes, no, or a note.

**Reads well**
- [ ] I can always tell the score, who is serving and from which side.
- [ ] I can always see where the ball is, including when it's high on a lob or near the glass.
- [ ] I know whose ball it is between me and my partner.
- [ ] The timing cue after a shot is readable and I understand why a shot went wrong.

**Feels like padel**
- [ ] Defending from the back glass and lobbing to win the net works and feels good.
- [ ] Points are won by position, lobs and the glass, not just by smashing.
- [ ] Bandeja, víbora and smash feel different and I can choose between them on purpose.
- [ ] The chiquita gets the opponents to volley up.
- [ ] Rebounds off the glass are predictable after a few games; the mesh feels dead.

**Controls**
- [ ] Every slice shot can be played on purpose: drive, volley, lob, bandeja, víbora, smash, chiquita, both serves, and each one off the glass.
- [ ] Movement responds immediately (target: under 100 ms from input to swing).
- [ ] The split-step and sprint feel useful, not noise.

**AI**
- [ ] My partner covers the middle and moves with me as a pair.
- [ ] Partner commands (net, back, middle, "yours") do what I expect.
- [ ] Easy, Medium and Hard feel clearly different; Medium is a fair fight.
- [ ] The opponents punish me when my pair is split.

**Stability**
- [ ] A full best-of-3 match from the main menu to the stats screen, gamepad only, with no crash or soft-lock.
- [ ] The score was right every point, including golden points and tiebreaks.
- [ ] 60 fps throughout.
