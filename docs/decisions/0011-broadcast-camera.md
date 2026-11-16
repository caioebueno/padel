# 0011: Long-lens broadcast camera

Status: accepted (Caio, 2026-11-16). GDD 9.

**Decision.** The match camera copies the composition of a televised padel frame: a long lens from far behind the human team's back glass. 7.25 m high, 14 m behind the glass (X = ±24), aimed at the foot of the net, 26° vertical field of view held on every screen shape (floored at 40° horizontal). All values in `Data/Camera.json`; maths in `Padel::Presentation::FCameraRig`.

**Why.** The old camera (8 m high, 7 m behind, 50° wide) cut the near baseline off the bottom of the frame, so the human's pair vanished whenever they were back at the glass. A camera fitted to the reference frame (14 landmarks, 7 px RMS on 1476 px) sits 17.5 m behind the glass with a 37° lens; that is outside the arena (end wall at X = ±25), so 14 m is the closest that fits, within 1 to 3% of frame height on every landmark. The arena's camera gantries move to the X = ±24 roof truss.

**Tracking.** Sideways only, never turning: a 1 m dead zone around the centre line, 0.3 m of camera per metre beyond it, at most ±1 m, 0.25 s lead, a critically damped spring (0.5 s half-life) capped at 1.2 m/s. The camera reads the ball actor every frame (it used to hear only bounces). Each serve starts centred (held from the serve call to the serve strike); after a point it holds 0.6 s, then eases to the centre over 1 s.

**Change of ends.** A cut to a net-side transition shot (2.5 s, slow 2° pan), then a cut to the other end. The old half-orbit would have swung the camera through the hall's side walls at this distance.

**Tests.** `Tests/Native/UI/CameraRigTests.cpp` checks the framing against the reference's proportions at 16:9, 16:10, 21:9 and 4:3, both pairs in frame at every tracking offset, the tracking feel, the serve and point lifecycle, the cut, and determinism.
