# Motion capture shot list

What to record to replace the placeholder clips (docs/decisions/0005-player-animation.md). Every clip in `Data/AnimClips.json` is listed here; the row name is the take name.

## Tool

The brief left Rokoko or Move.ai open. The default here is **Rokoko (Smartsuit with gloves)** for swings: an inertial suit isn't blinded by a fast racket arm or by the body hiding the hitting hand, and a swing is exactly that. **Move.ai** (markerless, phones) is fine for locomotion and celebrations if it's easier to set up. The list itself works with either. Change the default in this file if you pick the other.

## Setup for every take

- A real racket (any plain one, no logo visible), and a ball fed by hand or a machine, so the contact is real and the body reacts to it.
- **Mark the contact frame:** a ball on a string at the contact point, or a clap/foot stamp on a separate track. Write the contact frame in the take sheet; it becomes `ContactS` (frame ÷ frame rate), and the full backswing frame becomes `WindupS`.
- Start every swing from the **ready stance** (knees bent, racket in front at waist height) and end back in it, so clips blend.
- Face the "net" throughout. Record at 60 fps or more.
- One right-handed player is enough: left-handers are a mirror of the same clips.
- No imitation of a real player's signature motion (CLAUDE.md: original content only).

## Locomotion (Motion Matching database)

Long, continuous takes are better than short loops for Motion Matching. About 2 to 3 minutes each, moving the way a padel player does in the back of the court and at the net.

| Take | What | Notes |
|---|---|---|
| `loco_ready_idle` | Ready stance, small weight shifts, bouncing on the toes | Also a relaxed between-points idle, walking back to position |
| `loco_shuffle` | Side shuffles left and right, short and long, stops and changes of direction | The most common movement in padel |
| `loco_backpedal` | Backpedalling and crossover steps back toward the glass, then forward again | Chasing lobs |
| `loco_run` | Runs forward, diagonal and across, with plants and stops | Run cycle authored at about 4 m/s (`RunClipSpeedMps`) |
| `loco_sprint` | Full sprints, including to the side glass and back | About 6 m/s (`SprintClipSpeedMps`) |
| `loco_split_step` | Split-steps from walking and from standing, then a first step in each direction | GDD 6.3; the hop is about 0.18 s |
| `loco_net_approach` | Moving up to the net after a lob or a bandeja, and recovering back | |

## Swings

What each body should do in these takes (phases, contact points, racket path, early and late versions), plus the extra height variants, off-the-glass takes and post-slice strokes, is in [`BIOMECHANICS_SHOTS.md`](BIOMECHANICS_SHOTS.md) section 6.

Three versions each: **early** (contact well in front, racket comes across the body), **perfect** (contact at the ideal point, about 0.8 m to the side and 0.3 m in front), **late** (contact beside or behind the body, racket pushed). Five good takes per version, so the best can be picked.

| Shot | Sides | Takes | What to capture |
|---|---|---|---|
| Drive | fore, back | `drive_{fore,back}_{early,perfect,late}` | Groundstroke after the bounce at waist height, from the back of the court |
| Volley | fore, back | `volley_*` | Short punch at the net, little backswing, no bounce |
| Lob | fore, back | `lob_*` | Low contact, open racket face, swing finishing high |
| Bandeja | fore, back | `bandeja_*` | Controlled overhead with slice, side-on, staying balanced to keep the net; the backhand version is the backhand overhead |
| Víbora | fore | `vibora_fore_*` | Aggressive slice-and-side-spin overhead, more wrist and rotation than the bandeja |
| Smash | fore | `smash_fore_*` | Full overhead set under the ball, with and without a small jump |
| Chiquita | fore, back | `chiquita_*` | Soft, low push to the opponents' feet, short follow-through |
| Flat serve | fore | `serve_flat_fore_*` | Bounce the ball, underhand swing below the waist (GDD 3.2) |
| Kick serve | fore | `serve_kick_fore_*` | Same toss, brushing up the back of the ball |

Also useful, a few takes each:

- **Off the glass:** a drive and a lob after letting the ball come off the back glass and off the side glass (the same clips play for now; these will tell whether off-the-glass shots need their own).
- **Mishits and whiffs:** a stretched, off-balance swing that misses or frames the ball.
- **Swing on the run:** the drive and volley taken while still moving, to check the upper-body layer against a real one.

## Celebrations and reactions

`A_Celebrate_FistPump`, `A_Celebrate_RacketTap` (tapping the racket with the hand or strings), `A_Celebrate_Shout`, `A_Dejected_HeadDown`, `A_Dejected_RacketDrop` (racket arm dropping). About 1.8 s each (`CelebrationS`), starting and ending in a neutral stance. Later: a high-five and racket-touch with the partner, which need both players recorded together.
