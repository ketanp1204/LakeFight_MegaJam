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

**Stage 1 feedback fixes (director, 13 Nov)**
- "I can't control the camera while driving": the boat camera now free-looks - mouse (template IMC_MouseLook / IA_MouseLook, same feel as on
  foot) and gamepad right stick (runtime boat-only context) orbit it; after `CameraRecenterDelay` 2 s without look input it eases back behind
  the boat with the normal lag (0 = never recentre). Pitch clamped `CameraMinPitch` -60 / `CameraMaxPitch` 10. Not testable through MCP
  (no mouse) - please check, incl. gamepad pitch direction.
- "The opponent tries to hit me while I'm swimming": swimmers/climbers are no longer AI targets (out of the fight); with no target in a boat
  the AI goes Idle and its boat coasts. Verified: `RAM -> Idle` when the player went overboard, no swings while swimming.
- "His boat passes through me": character capsules are query-only (T02 rule: never push a boat), so hulls never touched a swimmer. A swimmer
  overlapping any hull outline is now pushed out to the edge; a boat moving > `RunOverSpeed` 250 cm/s toward him runs him over (small ragdoll
  launch, `[swim] ... run over` log). Verified the push-out (boat teleported onto the swimmer -> pushed to the hull edge + capsule radius);
  the run-over needs a moving boat - please test.
- "Holding W/S/A/D while pressing E keeps the boat moving": the boat's input came only from Triggered/Completed events, and a key already held
  when the boat's mapping context is added never sends Completed on release. While a player drives, the boat now polls the live action
  values every tick (`BindActionValue`), so releasing the key always stops it; AI input (SetDriveInput) is untouched.
- "Pitching the camera low takes it under the water (white bottom)": both cameras now clamp pitch from the arm geometry so the lens stays
  >= `CameraWaterClearance` 40 cm above the wave crest at the pivot (boat: 850 cm arm + 10 deg max put the lens ~28 cm under). Free-look also
  clamps the controller pitch (no dead zone looking back down).
- Build OK; PIE 63 s clean. Not testable through MCP (held keys / mouse) - please check both.

**Director: stage 1 accepted. Order: T02 step 5 first, then T03 stage 2. Keep Manny + Codex's gear (option 1).**

## T02 step 5 - hooks, crew gear, paddle grip, character look (Claude Code, 13 Nov evening)

**Files**
- New `Source/LakeFight/Brawler/LFBrawlerAnimInstance.h/.cpp` - native `ULFBrawlerAnimInstance` (replaces ABP_Unarmed on the brawler;
  MCP can't author AnimGraphs): plays the director's retargeted clips by state with a 0.2 s crossfade - idle/walk (template) by speed,
  `Sitting_Rowing` when driving, `Treading_Water` <-> `Swimming` by swim speed, `Swimming_To_Edge` scrubbed over the climb, `Dizzy_Idle`
  while stunned, `Great_Sword_Slash` as the swing base (wind-up held while charging, strike over the swing, rest over the recover) -
  then **Two Bone IK on both arms** (`AnimationCore::SolveTwoBoneIK`, pole = current elbow) onto the paddle grips while standing.
- `Source/LakeFight/Brawler/LFBrawler.h/.cpp`:
  - Hooks (BlueprintImplementableEvent, empty): `OnPaddleHit`, `OnSplashIn` (existed), new `OnClimbAboard(Boat)`, `OnSteal(Boat)`,
    `OnSwimStroke()` (~every 0.7 s while swimming and moving).
  - Crew gear: `Vest` (SM_LF_LifeVest_<Team>, spine_03) and `Hat` (SM_LF_Hat_Bucket_<Team>, head) + `Face` piece slot; absolute scale 1,
    NoCollision, attached to the bones (they follow the ragdoll), aligned from the reference pose (the meshes are authored in character
    space around the bone pivots). Personality overrides for T03 stage 2: `HatOverride`, `FacePiece`. Player = Red, LF_Crew_Blue = Blue.
  - Paddle by state: Standing = in the hands (pivot moved to the right-front of the waist so both arms reach; idle held diagonally,
    blade low front-left); Driving / Swimming / Climbing = stowed diagonally on the back (`PaddleBack`); Ragdoll = on `PaddleGrip_R`
    (hand), NoCollision. `PaddleScale` 0.85 absolute. Grip points `GripROffset` (4 cm, T-grip) / `GripLOffset` (59 cm down the shaft).
  - Manny materials: slot 0 = `MI_LF_Char_Body` (cream/skin neutral), slot 1 = `MI_LF_Char_Joints` (WoodDark) - component overrides.
  - Debug: `LFCam <yawOffset> <pitch>` (camera angle for screenshots).
- `Source/LakeFight/Boat/LFBoat.h`: debug `LFDrive` while driving (the controller possesses the boat then).
- `LakeFight.Build.cs`: + AnimationCore.
- Assets: `SKM_Manny_Simple` + sockets `PaddleGrip_R` (hand_r) and `PaddleBack` (spine_03) (identity transforms; the aligned offsets are
  applied in C++); new `Materials/M_LF_CharacterFlat` (BaseColor vector param, Roughness 0.9, Metallic 0, faceted normal
  `normalize(cross(ddx(P), ddy(P)))` with Tangent Space Normal OFF, used with skeletal meshes) + `MI_LF_Char_Body`, `MI_LF_Char_Joints`.

**Deviations:** Grip_R / Grip_L are C++ offsets, not sockets on SM_LF_Paddle - static-mesh sockets can't be added through MCP (Codex
could add `SOCKET_Grip_R/L` empties in `SM_LF_Paddle.py`; I'd then read them). The anim instance replaces ABP_Unarmed (no jump/fall
clips - jumping isn't in the game yet).

**Screenshots** (`Docs/Shots/`): `T025_before_manny_default.png` (old grey Manny, shoulder-held paddle) -> `T025_idle_front.png`,
`T025_idle_side.png` (flat material, both hands on the paddle, held diagonally), `T025_swing.png`, `T025_drive.png` (seated rowing),
`T025_drive_paddle_on_back.png`, `T025_swim.png` (treading water). Ragdoll unchanged from 2.1 (paddle rides the hand) - the ragdoll shot
missed him (he flew out of frame). Builds Succeeded; PIE runs clean (no warnings/errors).
**Known:** hands are positioned on the shaft by IK but not rotated to wrap it; the swing reads as the sword slash + paddle arc - check feel.

## T03 stage 2 - three crews, personalities, AIs fight each other (Claude Code, 13 Nov evening)

**Files**
- New `Source/LakeFight/AI/LFCaptainPersonality.h/.cpp` - `ULFCaptainPersonality` (UPrimaryDataAsset): look (Hat, FacePiece) + behaviour
  (reaction delay, aim error, swing cooldown, charged-swing chance, fight enter/exit range + give-up time, board range, ram range/angle,
  target weights for drivers / empty seats, own-boat weight when swimming, flee hits/time).
- `Source/LakeFight/AI/LFCaptainAI.h/.cpp` - reads the brawler's `Personality` on possess; **fight hysteresis** (enter within
  `FightEnterRange` 1.9 m, leave only beyond `FightExitRange` 3.5 m or after `FightGiveUpTime` 2 s out of reach); targets = any OTHER
  team in a boat (nearest/most tempting by personality weights, with stickiness) - AIs fight each other and the player; swimmers are
  never paddle targets, but a boat whose crew is all overboard is a **ram target**; a swimming Thief prefers enemy empty boats (steal);
  `lf.ai.spectate` console command.
- New `Source/LakeFight/AI/LFSpectatorCamera.h/.cpp` - spectate camera following the centroid of the crews still in play.
- `Source/LakeFight/Brawler/LFBrawler.h/.cpp` - `Personality` property (its hat/face piece override the team look), `SetSpectating()`;
  a stolen boat becomes the thief's home (re-taking its seat isn't another steal).
- Assets: `AI/DA_LF_Personality_Bully` (captain hat + beard), `_Thief` (team bucket hat + moustache), `_Hothead` (team bucket hat);
  `L_Lake01`: three crews spread around the lake, each in a home boat in its team colour - **LF_Crew_Blue = Bully** (boat at 30, 22 m),
  **LF_Crew_Green = Thief** (-30, 22 m), **LF_Crew_Yellow = Hothead** (0, -30 m); the player (Red) starts at the centre.

**Personalities (DataAsset values)**
| | Bully (Blue) | Thief (Green) | Hothead (Yellow) |
| --- | --- | --- | --- |
| Look | captain hat + beard | bucket hat + moustache | bucket hat |
| Ram | range 30 m, 35 deg | 5 m, 12 deg | 12 m, 20 deg |
| Fight | enter 1.5 m, never boards | 1.7 m, boards 1 m | 2.3 m, boards 2.2 m, exits 4.2 m |
| Swing | 1.2 s cd, 30 % charged | 1.3 s, 15 % | 0.6 s, 60 %, sloppy aim 20 deg |
| Targets | drivers x0.6 | empty seats x0.5; own boat x1.8 when swimming (steals) | nearest |
| Flee | after 3 hits | after 1 hit | never |

**Acceptance run** (`lf.ai.spectate`, AI-only, 12:18:57-12:21:12 UTC = 134 s, **0 warnings/errors**): **1 steal** (Thief swam to the abandoned
player boat, climbed in, `[steal] ... steals BP_LF_Boat_C_3`), **8 overboards**, 23 rams, 15 paddle hits, 7 climbs back aboard.
Per personality (first 2-min run): Bully 16 Ram decisions / 10 rams landed / 1 swing; Hothead 13 Fight decisions / 12 swings;
Thief 7 Steal + 2 Flee decisions. Screenshots: `Docs/Shots/T03_spectate_1.png`, `_2.png` (follow camera).
**Please check:** whether the personalities read without debug text (the Bully's captain hat + beard and constant ramming, the Thief's
moustache and stolen boats, the Hothead standing and swinging early); `lf.ai.spectate` (restart PIE to play again).
**Known:** steals happen only via swimming (no boat-to-boat jump until step 4); spectate is one-way per PIE session.
