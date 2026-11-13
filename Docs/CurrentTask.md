# T03 - AI rival captains (Fri 13 Nov afternoon) - agent: Claude Code

## Goal
Three AI rivals, each in their own boat, play the same game as the player: fish, ram, board, brawl, steal, flee.

## In scope
- `ALFCaptainAI` (AIController) driving an `ALFBrawler`, using the same inputs as the player (no cheating physics).
- Simple utility/state choice re-evaluated every 0.5 s: GoFish, ReturnToDock, Chase (target with most fish),
  Ram, Board, Fight, RecoverBoat (when swimming: nearest empty boat, else own boat), Flee.
- Personalities via a DataAsset: Bully (rams a lot), Thief (boards and steals), Fisher (avoids fights until robbed).
- Boat steering: steer toward a point with lookahead; avoid shore with 3 line traces; no navmesh on water.
- Debug: `lf.ai.debug 1` shows each AI's state above its head.

## Acceptance (manual)
- [ ] Watching 2 minutes of AI-vs-AI produces at least one steal and one man-overboard without the player.
- [ ] I can tell each personality apart without debug text.

## Director's T03 brief (13 Nov evening) - overrides the prompt above where they differ
Simple, combat-only AI (fish/scoring in T04). Use ONLY the brawler/boat actions from T02 step 3 + ALFBoat drive input - same rules and
physics as the player. Stage 1: one Blue crew (replaces LF_Dummy): Chase (lookahead steering, 3-trace shore avoidance), Ram, Board/Fight,
Steal, RecoverBoat, Flee; 0.5 s decisions, reaction delay + aim error; `lf.ai.debug 1`. STOP for test. Stage 2: Blue/Green/Yellow crews,
personalities from a DataAsset (Bully rams, Thief steals, Hothead paddles first), AIs fight each other too; looks: Bully = Hat_Captain +
Beard, Thief = bucket hat + Moustache, Hothead = bucket hat; player = plain bucket hat (absolute scale 1). Acceptance: 2 min AI-only
(`lf.ai.spectate`) gives >= 1 steal and >= 1 overboard; personalities readable without debug text.
(T02 is in this file's git history; T02 step 5 - hooks + team vests/hats - was not started: the director moved on to T03.)

## Stage 1 - one AI crew (Claude Code, 13 Nov ~16:40-16:50 IST)

**Files**
- New `Source/LakeFight/AI/LFCaptainAI.h/.cpp` - `ALFCaptainAI` (AAIController), console var `lf.ai.debug`.
- `Source/LakeFight/Boat/LFBoat.h/.cpp` - `SetDriveInput(Throttle, Steer)` (same inputs as the player's actions); a placed boat with a stale
  `SplashSystem = None` override now falls back to the class default (the blue boat drew cyan debug spheres on rams instead of splashes).
- `Source/LakeFight/Brawler/LFBrawler.h/.cpp` - `ELFTeam Team` (player Red), `CanSwing()`, `IsNearGunwale()`, `RecentHits()`, hit history;
  the dummy auto-return-home now only applies to controller-less brawlers; brawler mesh + paddle write custom stencil 2 (same value as hulls)
  so a swimmer's submerged body no longer shows through the water as a pale ghost (lake material and stencil value unchanged).
- `L_Lake01`: LF_Dummy -> **LF_Crew_Blue** (Team Blue, AIControllerClass = LFCaptainAI, Auto Possess AI = Placed in World or Spawned);
  its boat -> **LF_Boat_Blue** with the SM_LF_Rowboat_Blue hull mesh (same UCX).

**Behaviour** (decisions every 0.5 s, applied after a random 0.15-0.35 s reaction delay; steering/aim every tick):
- Swimming -> **RecoverBoat**: nearest boat with an empty seat (own boat weighted 0.6x), `StartSwimTo`, `TryGrabHull` within 0.9x grab range.
- Standing -> **Fight** if an enemy is within ~1.9 m (or the enemy's boat is alongside): face (+-12 deg aim error) and `Swing` - charged
  (0.8-1.0) if the target `IsNearGunwale`, else light; 1.1 s swing cooldown; otherwise walk toward them inside the boat. Else walk to the
  seat and `TryDriveBoat` (**Steal** when it isn't its home boat).
- Driving -> **Ram** when lined up within 20 deg, < 15 m and > 150 cm/s, else **Chase** (steer to the target boat + 0.8 s of its velocity);
  leaves the seat to Fight when hulls are close. 3 shore traces (centre, +-35 deg; 5 m + 1.5 s of speed) bend the steering away from walls.
- **Flee** for 4 s after 2 hits within 3 s (drive away from the attacker).
- Tunables: `LF|AI` on the controller (reaction, aim error, ranges, flee).

**Verification (PIE, me not touching the player):**
- 11:10 run (35 s): Idle -> Chase -> drove -> RAM (x4, strength 0.11-0.55) -> Fight -> light hit (knockback+stun) -> ... -> charged swing 0.86
  (target near gunwale) -> player OVERBOARD (splash 115 cm out).
- 11:16 run (73 s, no warnings/errors at all): another charged overboard, then it re-engaged the swimming player (Chase -> ram -> Fight swings).
- Builds: 1 compile error (helper named like `AController::StateName`) fixed -> Succeeded.

**Please test:** fight it - is it beatable? (`ReactionDelay`, `AimErrorDeg`, `SwingCooldown` on the controller). `lf.ai.debug 1` in the console
shows its state above its head (HighResShot doesn't capture debug text, so I couldn't screenshot it).
**Known:** it toggles Fight/Chase (stand <-> sit) every few seconds when a target drifts in and out of paddle range; it rams a lot
(stage 2 personalities will vary this); Steal is only reachable after it ends up swimming next to an empty enemy boat (no boat-to-boat jump).

**Stage 1 fix (director: "the blue boat is driving itself in a circle after I knocked over the opponent")** - an AI never possesses its
boat; it drives through `ALFBoat::SetDriveInput`, and leaving the seat (knocked out, or standing up to Fight) kept the last throttle/steer, so
the empty boat circled (the player path resets on UnPossessed). `ALFBoat::SetDriver(nullptr)` now zeroes the drive input: an empty boat
coasts to a stop. Rebuilt; a 40 s AI run still chases/rams/fights normally.

**Queued (director, 13 Nov):** step 5 now also includes the paddle grip work (PaddleGrip_R / PaddleBack sockets, two-hand Two Bone IK to
Grip_R / Grip_L sockets on SM_LF_Paddle, paddle placement per state, ragdoll paddle on hand_r, paddle absolute scale 0.85).
