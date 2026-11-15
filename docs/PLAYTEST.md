# Playtest notes

Owner: Agent 8 (QA). Updated each week. Caio plays, writes notes at the bottom, and QA turns them into issues for the right agent.

## How to run the build

1. Install **Unreal Engine 5.8** from the Epic Games Launcher, and Xcode on the Mac (see `README.md`).
2. `git pull` on `main`, then open `Padel.uproject`. Let it build the C++ modules when it asks.
3. Press Play in the editor. Use a gamepad if you have one; GDD section 7 has the controls.

If you can, also run `Tools/RunUnrealTests.sh` once: the cloud agents can't run the Unreal tests, so your Mac is where they run for now.

---

## Week of 2026-11-16

### What changed

- The project moved to **Unreal Engine 5.8** (C++), with realistic art as the target. The setup is merged: the court built from engine cubes at regulation size, the broadcast camera, the input actions, native tests on Linux and Mac in CI.
- The rule that a ball leaving the court without bouncing is "out direct" is now in the GDD.
- QA added CI checks: a PR fails if the simulation's reason codes or match events drift from the GDD, if a rule has no test, or if the simulation code uses unseeded randomness, the clock, Unreal objects or Chaos physics.
- The match engine, controls, AI, assets and UI are being built now. Nothing is playable yet.

### What to try

1. Open the project on your Mac and press Play. You should see the placeholder court from the broadcast camera, behind and above the near back glass.
2. Does it build and open without errors on the Mac? How long did the first build take?
3. Does the camera height and angle feel like the TV view you want (GDD 9)?
4. Do the court proportions look right: glass height, the 2 m step on the side glass, the door gaps, the net?

### Questions to answer after playing

- Did the Mac build work first time? Anything you had to install or click through?
- Is the camera too high, too low, or about right?
- Anything in GDD section 13 you want to change before the engine and AI agents build on it?

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
- [ ] 60 fps throughout, on the Mac and on the Windows target PC.
