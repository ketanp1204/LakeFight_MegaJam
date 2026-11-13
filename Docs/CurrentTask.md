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

## T03.1 - playtest fixes (director, 13 Nov; stop 19:15 IST)
1 rowing follows the boat; 2 paddle grip on Grip_R/Grip_L sockets with hand alignment + finger curl, lf.debug.ik; 3 climb relative to the
hull, 1.2 s at 150-350 cm/s, hang + tow above 350; 4 capsize rule (roll > 70 deg for 0.3 s -> everyone overboard, no driving, rights itself
after 3 s over ~1.2 s with a big splash, [capsize] log). Commit per item.

**Item 1 - rowing follows the boat.** `ULFBrawlerAnimInstance`: while driving, `Sitting` when |spooled throttle| < 0.1 and boat speed
< 50 cm/s, else `Sitting_Rowing` with play rate = throttle x `RowRateAtFullThrottle` (1.0; negative throttle plays it backward); 0.2 s
crossfade. Same path for AI drivers (they drive through SetDriveInput -> the same spooled throttle). `ALFBoat::GetThrottleOutput()`.
Verified: builds; PIE with the player seated at idle + 25 s of AI spectate, no warnings/errors; screenshots show seated drivers (too small
to judge the stroke rate - please check rowing speed vs throttle and reverse). Also committed: the director's saved BP_LF_Brawler and
L_Lake01 changes.

**Item 3 - climb relative to the hull.** The climb was already a hull-local lerp (moving boats carry you). New: the boat's speed at the
grab picks the climb: < 150 cm/s keeps `ClimbTime` 0.8 s; 150-350 cm/s takes `ClimbTimeMoving` 1.2 s; > 350 cm/s (`HangSpeed`) you HANG
on the gunwale in hull space, facing the boat, and get towed until it drops below 350, then climb from the rail (1.2 s). A paddle hit
knocks a hanger off (the existing Climbing branch of ReceivePaddleHit -> ragdoll); Interact lets go (-> swimming); a capsize drops you.
Logs `[climb] ... HANGING` / `slowed down - climbing in` / `lets go`. Verified: normal climbs in PIE (AI re-boarding after the capsize
test). The hang/tow path compiles but was NOT seen in PIE - please try grabbing a fast boat.

**Item 4 - capsize as a rule.** `ALFBoat`: tilt > `CapsizeRollDeg` 70 deg for `CapsizeHoldTime` 0.3 s = Capsized: `[capsize]` log,
everyone aboard thrown overboard via the new `ALFBrawler::KnockOverboard` (ragdoll, driver leaves first), throttle/steer forced to 0,
TryDrive refuses, no drive/climb prompts, grabbing a capsized hull refused, AI skips capsized boats when recovering. After
`CapsizedTime` 3 s a righting torque (proportional to tilt, damped) over `RightingTime` 1.2 s; upright (< 25 deg) ends it with a big
splash (2.5x Niagara + OnSplash). If still over it pushes again. The pontoon buoyancy already self-rights; it's left as is (a boat
that pops upright early still counts as capsized for the 3 s). Debug: `lf.debug.capsize [i]` rolls boat i 150 deg. Verified with
AI spectate: all four boats capsized (1 crew thrown each, 0 for the empty player boat), each righted at 4.2 s, AIs swam back and
climbed aboard, no warnings. Shots: `Docs/Shots/T031_capsize_over.png`, `T031_capsize_righted.png`.
Items 3+4 share LFBoat/LFBrawler edits, so they're one commit.

**Item 2 - paddle grip: not started** (out of time before 19:15 IST). The re-imported SM_LF_Paddle (Grip_R/Grip_L) is left uncommitted
for that step.

**T03.1 status at the machine move.** Item 1 (rowing anim follows the throttle) is DONE and committed in `29499aa`. Items 3+4 are done
in `d1d7ec0` (hang/tow not yet seen in PIE). The re-imported SM_LF_Paddle (B07 Grip_R/Grip_L sockets) and the four rowboats (B06 UCX)
are committed separately ("Import B06 rowboat UCX + B07 paddle sockets"). **Item 2 (paddle grip) is not started: it's the first task on
the laptop.**

**Item 2 - paddle grip (Claude Code, laptop, 13 Nov evening).**
- Hands go to SM_LF_Paddle's `Grip_R` / `Grip_L` sockets (B07; the old C++ offsets are only a fallback). The shaft axis is taken from
  the two socket positions: the imported sockets' own X axis is NOT along the shaft (FBX empty axis conversion) - using it turned the
  hands 90 deg off the shaft ("arms mixed up").
- `ULFBrawlerAnimInstance`: each hand is turned so its knuckle line runs along the shaft (right thumb toward the blade, left toward
  the T-grip = overhand), the wrist is IK'd so the PALM lands on the socket, fingers + thumb curl around the shaft (`LF|Anim|Grip`).
  No X (director: "X cross still occurs"): while the paddle points out to the right (wind-up, charge hold, early strike) the left
  hand's place on the shaft is right of the right hand, so the left hand lets go (one-handed back-swing) and re-grips over 0.12 s
  when the shaft comes back across to the left. Otherwise, if the lower socket is out of reach, the left hand slides up the shaft.
  Charge hold (director: "the right hand in front of the left shoulder"): the chest twisted +35 deg but the paddle pivot stayed on
  the capsule, so relative to the chest the right hand ended up past the left shoulder (measured: right wrist -1.37 shoulder
  half-widths). The pivot now turns around the body axis with the torso twist (`FollowTorsoTwist`) - this moves the strike's arc a
  little (starts further right/back, ends further left), timing unchanged - and the free left hand rests beside the left hip
  (`FreeHandOut/Forward/Up`) instead of wherever the clip + twist put it. Idle pivot (25, 26, 28.8) balances the wrists on the chest.
  Debug: `LFCharge <s>` holds a charge from the console; the `[ik]` log also gives each wrist's position across the body.
  Final grip spec (director, 13 Nov late): hands at EQUAL height (shaft level, `HoldPivot` (28.5, 17, 31), `HoldRotation` (0, -86, 0),
  both now tunable on the brawler, `LF|Paddle`), held out from the body (arms ~0.7-0.85 of reach); palm grip = right hand overhand
  (palm down, `bRightOverhand`), left underhand (palm up, `bLeftUnderhand`), both thumbs toward the blade. Light punch: both hands
  stay on the paddle (the earlier "no X" auto-release is removed). Charged punch: once the hold passes `ChargeMinTime` the left hand
  lets go and rests by the hip, the right holds the charge pose (pivot pulled back toward the right shoulder, `WindupPivotOffset`)
  and fires; the left re-grips in the recover. Measured: idle both 0.0 cm on the sockets; light swing left grip 1.00 throughout;
  charge hold left grip 0.00, right wrist +0.6..0.75 (right of the chest); re-grip 0.0 cm by recover. Shot:
  `Docs/Shots/T031_level_hold_and_charge.png` (idle behind / idle side / charge hold). Known: a light swing's first frame snaps
  (pivot reads the torso twist one frame late).
- Standing hold (director: "hold it more horizontal, centred on the chest"): `PaddlePivot` (25, 16.6, 28.8), `PoseIdle` (-10, -90, 0):
  shaft ~10 deg across the chest, grips symmetric about the body centre (was: pivot at the right waist, left grip out of reach -
  the left hand never reached the paddle, also before this item). The swing poses are unchanged, but the pivot moved ~17 cm up/left.
- Swing (director): the Great_Sword_Slash clip is gone (it fought the IK). The arms follow the paddle arc; the spine
  (spine_01/02/03 = 20/30/50 %) twists +35 deg at the wind-up -> -45 deg at the follow-through, pitches forward over the strike
  (10 deg x strength), charged swings lean in (pelvis 6 cm forward + 6 deg) (`LF|Anim|Swing`).
- Seated (director: "Sitting1 is the new idle sitting animation"; "hands go inside the thighs"): the sit clip is `Anim/Manny/Sitting1`;
  while sitting/rowing each palm is pushed out to the thigh surface + 2 cm (`LF|Anim|Sit`, thigh radius 9 cm mesh space).
- Debug: `lf.debug.ik 1` draws sockets (green/blue), palm grip points (red) + palm normal (yellow) / thumb side (cyan) and logs
  `[ik]` from the solved pose itself (palm -> socket, distance off the shaft, left grip weight; seated: palm -> thigh surface).
  `lf.ai.freeze 1` (director's idea) stops the AI captains so the player can be tested alone.

**Verification** (PIE, `lf.ai.freeze 1`, `LFSwing 1` / `LFSwing`, slomo 0.03 for the shots; builds Succeeded, Live Coding for .cpp-only steps):
idle hold palm -> socket R 0.0 / L 0.0 cm for the player and all three AI crews; charged swing: right hand 0.0 cm on its socket
throughout, left hand lets go in the wind-up/strike (grip weight -> 0.16) and is back on the paddle (0.0 cm) by the follow-through;
seated in Sitting1: palms 1.5 cm outside the thighs (world) every sample. Shots: `Docs/Shots/T031_grip_hold_front.png`,
`T031_swing_no_cross.png` (wind-up / strike / follow-through / recover, from behind), `T031_swing_hold_windup_strike_follow.png`
(the version before the no-X release), `T031_grip_v1_diagonal_*` = the rejected first diagonal hold.
**Please check in PIE:** the hold and the swing feel (torso twist amounts, lean), Sitting1 hands on the thighs, the hit reach/feel
after the pivot move. **Known:** the debug drawing can lag a frame in a fast swing (the log is exact); during the strike the left hand
visibly slides along the shaft.
**Follow-ups (not done):** paddle in the hands while rowing (driving still stows it on the back, T02 step 5 rule; needs the new
sitting/rowing clips); the life vest sits low on the hips in the shots (spine_03 attach offsets).

**Driving paddle + punch speed + camera (director, 13 Nov late):**
- Charged punch fires faster than a light one: `ChargedSwingTime` 0.3 -> 0.15 s (light stays 0.22 s).
- The paddle stays in the hands while driving (was stowed on the back): no throttle = seated hold, level across the lap in front of
  the chest (`SitHoldOffset` (30, 22, -16) from the shoulder midpoint, `SitHoldRotation` (0, -86, 0)) - no longer behind the right
  shoulder / through the floor, hands off the vest. Throttle = a procedural canoe stroke on the left side (`LF|Paddle|Drive`: blade
  catches 45 cm forward, pulls to -25 cm, 12 cm deep, lifts 18 cm on the recovery; the shaft leans toward a point above the right
  shoulder; 0.3 s blend between hold and stroke). Both hands IK onto it.
- Stroke rate follows the boat: strokes/s = speed / 300 cm/s, at least 0.6 x |throttle|, at most 1.6; reverse strokes backward.
  The rowing clip is sampled at the same stroke phase (`RowClipPhaseOffset` aligns the body's pull with the blade's).
- Boat camera no longer recentres behind the boat by itself (`CameraRecenterDelay` default 0).
- Measured (PIE, AIs driving): hands 0.0 cm on the paddle seated and rowing; 0.50 strokes/s at 69 cm/s, 1.0 at ~300, 1.6 (cap) at
  519; blade 7-13 cm under water pulling, ~16 cm above on the recovery. Shot: `Docs/Shots/T031_seated_hold.png` (player, idle).
  Not filmed close: the player's own stroke (needs throttle input) - please check the stroke look, `RowClipPhaseOffset` sync, and
  that the hands clear the vest.

**Superseded + new (director, 13 Nov late): oars, size, chest-anchored hold, walk.**
- The procedural paddle stroke / seated paddle hold above are REMOVED (director: "the rowing animation is bad... rowing on both
  sides like the FBX"). The paddle is stowed on the back while driving again.
- Oars on the boat (`ALFBoat` OarL/OarR, `LF|Oars`): pivot at `Oarlock_L/R` hull sockets when they exist, else placeholders
  `OarlockLocalL/R` (-12, -/+67.2, 36) from the rowboat script's oarlock forks. Placeholder oar = SM_LF_Paddle x (1.2, 0.85, 0.85)
  until `OarMesh` (SM_LF_Oar, Codex) is assigned on BP_LF_Boat - convention assumed: origin at the grip, blade +X. While someone
  drives: each oar's grip sits in the matching hand (hand -> middle knuckle midpoint), shaft through the oarlock, blade squared;
  the clip drives the hands, no hand IK. Nobody driving (or capsized): shipped along the inside of the gunwale (grip 75 cm forward
  of the oarlock, blade aft). 0.3 s blend. They're hull children, so they flip with a capsize.
- Rowing clip play rate = boat speed / 300 cm/s, at least 0.6 x |throttle|, capped at 1.25x natural speed; reverse plays backward.
- Brawler size `BrawlerScale` 0.75 -> 0.85 (director: try 0.85, then 0.9); crew gear scaled with it (authored for 0.75).
- Standing hold anchored to the shoulder midpoint (`HoldPivot` now mesh-unit offsets from it), so the paddle and hands ride the
  idle breathing / walk bob (director: "artificially stiff").
- Walk: the only unarmed walk in the project is the mannequin's MF_ (feminine) walk; its pelvis sway is damped 60 %
  (`WalkHipDamp`, thighs/spine counter-rotated) and it now plays at the body's scale (a smaller body steps faster for the same
  cm/s) - it looked slow before. A male walk clip (Mixamo) would replace it properly.
- Charged punch 0.15 s (faster than light 0.22 s); boat camera never auto-recentres.
- Verified (PIE + Simulate, AIs rowing): oar grips in the hands, blades 13-33 cm under water while rowing, ~20 cm above at sit idle;
  shots `Docs/Shots/T031_oars_rowing_side.png`, `_top.png` (mid-stroke, Simulate), `T031_oars_shipped_side.png` (nobody rowing),
  `T031_oars_seated_idle.png`. Known: the driver faces the bow while the clip PULLS, so the blades sweep forward on the pull
  (real rowers face the stern) - flip the clip direction or the driver if it reads wrong.

**Playtest 006 fixes (director, 13 Nov late) - BUILT, NOT YET VERIFIED IN PIE:**
- Walk clip = director's `Anim/Manny/Wheelbarrow_Walk` (replaces MF_Unarmed_Walk_Fwd); `WalkHipDamp` default 0.
- Paddle lagging / sliding out of the hands while walking: the mesh now ticks after the brawler (`AddTickPrerequisiteActor`), so the
  hands are solved onto this frame's paddle.
- Close-range hits never landed: the hit sweep now runs at 3 points along the paddle (35 % / 65 % shaft, blade centre); hull
  hits stay blade-only.
- Knocked onto the shore = stuck: at ragdoll recovery, if the ground under the pelvis is above the water, `FindWaterNear` searches
  rings (16 dirs, up to 25 m) for open water and the brawler is washed back there swimming (`[shore]` log).
- Debug: `LFRagdollAt X Y` (e.g. `LFRagdollAt 0 -4300` = left shore) to test the wash-back.

**Procedural oar rowing + walk in place (director, 14 Nov early):**
- Walk: Wheelbarrow_Walk carries forward travel in the hips (not an in-place export): the body ran ahead of the capsule, snapped
  back at the loop and circled when turning. `bWalkInPlace` keeps the root/pelvis at the idle pose's horizontal position (vertical
  bob kept). No re-source needed.
- Rowing (director: no usable rowing clip, "procedurally animate torso + hands"; option 3 without art changes): the driver sits on
  the FORWARD thwart (`DriverSeatLocal` X +40; thwart at +0.52 m in the mesh) and faces the STERN (`bDriverFacesStern`); the
  oarlocks (X -12) are ~50 cm in front of him. Sitting1 is the base pose; a stroke phase drives both hands together (IK onto the
  handles, overhand, thumbs inboard) - catch: lean 22 deg toward the stern, arms out; drive: pull to the ribs, lean back 15 deg,
  blades in; recovery: handles down (blades up) and back out. Elbows flare out to the sides (explicit IK pole; director: they
  converged behind the back). Strokes/s = boat speed / 500, at least 0.45 x |throttle|, at most 0.9; reverse rows backward; no
  throttle = hands resting on the handles. The boat's oars pair with the NEAREST hand (facing the stern, left hand = starboard oar).
  Tunables: `LF|Anim|Row`. The Sitting_Rowing clip is no longer used.
- Debug: `LFRow <throttle>` (while driving) holds a throttle without keys; `LFRagdollAt X Y`.
- Verified: PIE, player rowing with `LFRow 1` at slomo 0.1 - `Docs/Shots/T031_rowing_stroke.png` (6 phases, elbows out); [oars]
  blades 11-22 cm under water on the drive; shore test: `LFRagdollAt 0 -4300` -> "[shore] ... on land (ground 49 cm above the water)
  -> washed back 400 cm into the lake", swimming. NOT yet measured: close-range hit rate (3-point sweep), walk in place in PIE.
- Director: W moved the boat behind the stern-facing rower -> the rower faces the BOW again (`bDriverFacesStern` off, seat back on
  the aft thwart X -55); the procedural pull stroke stays (oars push water the "wrong" way - reads as rowing).

**T03.1 item 2 status at commit (14 Nov):** committed as reviewed in PIE by the director ("looks great" for rowing). Not measured:
close-range hit rate with the 3-point sweep, walk-in-place feel. Next task: A02 audio.
