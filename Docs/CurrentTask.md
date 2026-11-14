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
- Director (14 Nov): forward throttle = PUSH stroke (handles pushed out with the blades in, drawn back with them lifted - blades
  sweep sternward, the boat moves where he faces), reverse throttle = PULL stroke; the torso lean follows the hands, so it reverses
  with the stroke. Verified: PIE `LFRow 1`, slomo 0.1 - submerged blades move sternward, lifted on the return (blades 5-26 cm under
  water on the push); shot `Docs/Shots/T031_rowing_push_stroke.png`.

## A02 - wire audio (Claude Code, 14 Nov 07:46-09:00 IST)

**Files**
- New `Source/LakeFight/Audio/LFAudio.h/.cpp` - `ULFAudioSubsystem` (tickable world subsystem, game worlds only).
  - Events (`ELFSound`): PaddleWood, PaddleHit, PaddleCharged, Stun, Overboard, Surface, BoatCreak, RamWood, RamBass, HitCreak, OarDip,
    VO_Attack, VO_Yell, VO_Pant, VO_Taunt, VO_Whistle. Each loads its `<Prefix>_01.._06` waves from `/Game/LakeFight/Audio/` at begin play.
  - Per play: random variation, never the same one twice in a row; pitch +-6 % (SFX), +-3 % (voices), OarDip +-12 % around 1.15;
    a `USoundConcurrency` per event (MaxCount 2-4, StopOldest); SFX + Voice spatialised (full inside 2 m, linear falloff to 0 at 40 m).
  - Categories `ELFSoundCategory` Master/SFX/Voice/UI/Ambience/Music; `SetCategoryVolume` / `GetCategoryVolume` (BlueprintCallable) for the
    options menu; Master multiplies the rest; live ambience follows immediately.
  - Ambience (2D loops, random start offsets): Lake always (dips 40 % at a bank), Shore crossfades in (~2 s) as the listener gets within
    15 m of land (full at 3 m) - rings of downward WorldStatic traces every 0.5 s, land = ground > 30 cm above the water; Birds 0.3 +
    Meadow 0.25 constant under both.
  - Polled per boat: hull roll rate > 70 deg/s = BoatCreak (0.8 s cooldown, louder with rate); oar blade centre crossing into the water
    while throttle != 0 = OarDip (SFX_LF_Surface_01, soft).
  - `lf.audio.log 1` (default on) logs every sound: `[audio] <Event> <wave> vol pitch at (x,y,z) <- why`.
- `LFBrawler.cpp/.h` hook sites: light hit = PaddleWood + PaddleHit; charged (strength >= ChargedMinStrength) = PaddleCharged + VO_Attack
  30 %; knockback (not overboard) = Stun; ragdoll start = VO_Yell; splash-in = Overboard; upright in the water = Surface; swim stroke =
  Pant (max every 1.5 s); climb aboard = BoatCreak; hanging + towed = Pant x1.6 every 1-1.5 s + Surface 30 %; steal = VO_Taunt + Whistle;
  paddle on a hull = HitCreak.
- `LFBoat.cpp`: bump = BoatCreak x0.35; ram = RamWood + RamBass (scaled by ram strength); capsize = RamBass (each thrown crew member
  then gets their own Overboard on splash-in); righted = Surface x1.8 at pitch 0.7.
- `Config/DefaultGame.ini`: `DirectoriesToAlwaysCook=/Game/LakeFight/Audio` (waves are loaded by path, so packaging must include them).

**Deviations (no asset-creation tool in this MCP; no time for hand-made assets):** no SoundClass / SoundMix / MetaSound / SoundCue
assets - the subsystem does the randomisation, no-repeat, pitch, concurrency, attenuation and category volumes in C++ (transient
attenuation/concurrency objects). If you want real assets later: one SoundClass tree + mix, and the subsystem would assign them.
UI_*, Final30, EndCheer, Win, Fish* are not referenced (T04).
- Pile-up fix after run 1: long tails (> 1.5 s: RamBass) are capped at 2 live instances and a new one is skipped (PreventNew) instead of
  cutting the oldest; burst ducking: beyond 3 one-shots inside 0.25 s each extra plays 25 % quieter (oar dips excluded).

**Verification** (2 builds Succeeded, editor closed; PIE with the player idle, AIs playing):
- Run 1 (~70 s, player at the centre - the AIs pile up there): every sound logged; a 4-boat pile-up requested RamBass x4 overlapping
  (2 s tails) -> the fix above.
- Run 2 (300 s, player spawned at (0, -33 m) near the left bank; 0 warnings/errors): `[audio] ready: 16 events, 41 wave variations`;
  fired: OarDip 375, BoatCreak roll 57 / bump 15 / climb aboard 12, RamWood 40 + RamBass 40, VO_Pant (swim) 16, VO_Yell 14 + Overboard 14
  + Surface (surfacing) 14, PaddleCharged 12 + VO_Attack 4 (~33 %), PaddleWood 8 + PaddleHit 8, Stun 8, capsize RamBass 1 + righted
  Surface 1, HitCreak 1. Shore: `shore weight -> 0.42 (nearest bank ~10 m)` near the bank, 0 farther out (2 s crossfade).
  Overlap from the log (requests, before concurrency): at most 9 sounds live at once; per event <= 2 except OarDip 6 (cap 4) and
  RamBass 4 (cap 2, extra skipped).
- NOT seen in PIE (rare in AI play, no console access through MCP this session): steal (VO_Taunt + Whistle), hang + tow (loud Pant +
  Surface). Both are one-line calls next to the existing `[steal]` / HANGING code paths.
- Not measurable through MCP: actual loudness / clipping - the log shows requested volumes, not the mixer output.

**Please check by ear (PIE):** overall levels (SFX -16 LUFS, VO -18, ambience -28 sources); a pile-up for clipping/mud; oar dips
(soft, every blade entry - maybe too frequent?); the shore loop fading in when you swim to a bank; stealing a boat (taunt + whistle).
**Follow-ups:** real SoundClass/SoundMix assets if the options menu wants them (subsystem has `SetCategoryVolume` now); a master limiter
submix; finer shore probe rings (now 3/6/10/15 m steps).

## A02.1 - music states + ducking (Claude Code, 14 Nov ~09:16-09:25 IST)

**Files**
- `Source/LakeFight/Audio/LFAudio.h/.cpp`: new `ULFMusicSubsystem` (game-instance subsystem = one per session).
  - `SetMusicState(Off | Menu | Main | Final)` (BlueprintCallable). One 2D UAudioComponent per track, created once, persistent across
    level transitions, never auto-destroyed, `bIsUISound` (keeps playing when paused). Nothing in gameplay touches it, so respawn /
    overboard / capsize can't restart it. The first game world to begin play starts **Main** (PIE default until T04 / the menu).
  - State change: equal-power crossfade (`FadeOut` / `FadeIn` with the Sin curve) over one 4/4 bar of the incoming track (Main/Menu
    106.893 BPM = 2.25 s, Final 110 BPM = 2.18 s - from AUDIO_REPORT.md; 2.0 s fallback); the incoming track starts at 0 s (the WAVs are
    pre-looped, whole-file loop).
  - Ducking `Duck(Why)`: -4 dB, attack 0.05 s, hold 0.4 s, release 0.6 s (real time, so hitstop doesn't stretch it); a duck during an
    active one only extends the hold. Called from `ALFBoat` on Ram-tier impacts and on capsize.
  - Category volumes moved here (they now survive level changes); `ULFAudioSubsystem::SetCategoryVolume/GetCategoryVolume` forward to
    it, Music = Master x Music x duck.
  - `lf.music <menu|main|final|off>`; `[music]` log: state changes, loop wraps (from the component's playback-percent callback - for a
    looping wave UE counts past 1.0, so a wrap = crossing a whole number), duck start/extend/release.
- `Source/LakeFight/Boat/LFBoat.cpp`: `Duck("ram")` in the Ram tier, `Duck("capsize")` in Capsize().

**Verification** (builds Succeeded with the editor closed; PIE, AIs playing, `lf.music` typed into the editor console box via MCP):
- Main started with play (`Off -> Main, 2.25 s`), looped 6 times at 18.00 / 35.98 / 53.93 / 71.90 / 89.85 / 107.82 s (wave 17.962 s:
  a steady 17.95-17.98 s cadence, i.e. no gap or restart) - straight through a capsize (39.9 s) and 7 overboards.
- `lf.music final` -> `Main -> Final (2.18 s = 1 bar at 110 BPM)`, Final looped at 17.50 s (wave 17.455 s); `menu` -> Menu looped at
  9.00 s (wave 8.981 s); `off`, then `main` -> Main again, looped at 17.99 s.
- Ducking: 27 ducks (ram + one capsize) in the run; a lone duck releases 1.03 s after it starts (= 0.05 + 0.4 + 0.6); overlapping
  rams/capsize extended the hold (e.g. 4 ducks inside 1.7 s -> one duck, released after 2.68 s), never deeper. No new warnings/errors.
- First run found the loop-wrap log broken (it expected the percent to drop back to 0; UE keeps counting past 1.0 for looping waves)
  - fixed and re-verified above; the raw-percent diagnostic stays at Verbose.
**Please check by ear:** the crossfades (esp. Main -> Final at bar length), the -4 dB duck depth on rams, music vs SFX balance.
**Known:** the crossfade starts immediately, not on the next bar line of the outgoing track (no beat sync).

## T03.2 - rowing polish (Claude Code, 14 Nov 10:35-11:00 IST; director: ignore the 10:30 limit)
Brief: 1 real oars (SM_LF_Oar at Oarlock_L/R, pivot = collar, aim at the hand, light two-bone IK onto Grip, push stroke kept, shipped
when nobody drives, flip with capsize); 2 seat height measured from the mesh + two-bone foot IK onto the floor boards (AI too); 3 head:
per-bone layered blend on neck_01/neck_02/head, look-around fades out with rowing (0.3 s), looks ahead + steering bias, idle returns;
4 vest higher on spine_03, light-swing first frame blends in over 0.08 s, .gitignore += .mcp.json .cursor/ .gemini/ .vscode/.

**Files**
- `Boat/LFBoat.h/.cpp`: oars = SM_LF_Oar (C++ default, unscaled), attached at `Oarlock_L` / `Oarlock_R`, collar at the socket. Rowing:
  `AimOars(L, R)` (called by the driver's anim instance each update) turns each oar so its handle points from the oarlock at the hand
  target, blade squared; `GetOarGrip` = the oar's `Grip` socket (70 cm inboard), `GetOarGripReach`. Shipped (no driver / capsized):
  blade aft along the gunwale, 5 deg inboard, collar slid 8 cm in / 2 cm up; 0.3 s blend. Everything hull-local -> flips with a capsize.
  The stretched-paddle placeholder is gone. `FloorTopZ` -9.35 / `ThwartTopZ` 18.25 (from SM_LF_Rowboat.py, traced in PIE: identical).
- `Brawler/LFBrawlerAnimInstance.h/.cpp`: stroke path unchanged (push stroke), but each hand goal is put at the oar's reach (its sideways
  offset from the oarlock solved for 70 cm), the oar aims at it, the arm IK pulls the hand onto Grip. Left hand rows 14 cm (mesh) above the
  right (`RowHandStagger`) - the handles meet at the centreline. `PlantFeet`: per foot, sole (ankle/ball minus their ref-pose heights) moved
  onto the floor plane, two-bone IK thigh->calf->foot, foot rotation kept (`bSitFootIK`). `ApplyHeadLook`: neck_01/neck_02/head local
  rotations blend toward the ref pose by `HeadLookWeight`, then the head is turned (30/30/40 %) to look level ahead, -5 deg, + steer x 12 deg
  (`LF|Anim|Head`); weight eases 0.3 s with stroke intensity, 0 when the boat is idle. Swing: torso yaw/pitch/lean blend from the release.
- `Brawler/LFBrawler.h/.cpp`: driver placed so the backside rests on the thwart (`SitButtHeight` 37.7, measured); `VestOffset` (0, 0, 8);
  `SwingBlendInTime` 0.08 (paddle pivot blends from its pose at release; was snapped to the wind-up); anim view + `DriveSteer`,
  `SwingBlendIn`; `lf.measure` (editor builds): CPU-skins LOD0 with the current pose and logs backside vs the traced thwart, soles vs the
  traced floor, vest extent vs the spine bones.
- `.gitignore`: + .mcp.json, .cursor/, .gemini/, .vscode/.

**Verification** (2 full builds Succeeded with the editor closed; PIE + Simulate, editor started with `-nosound` per the director)
- Seat (lf.measure, 3 AI drivers): before - backside 37.36 / 37.53 / 38.15 cm above the capsule bottom = 10.1-10.9 cm ABOVE the thwart;
  after - 0.36 / 0.38 cm above the traced thwart top (18.25). Feet: soles -0.10..-0.20 cm vs the traced floor; foot IK lifts them
  6.7-12.4 cm (knees bend more). Sitting1's feet hovered 2-4 cm before.
- Oars: `[oars] ... oarlock sockets found ... grip socket found (X -70.0)` on all 4 boats. Hand target -> grip: first version (fixed
  hand spread) 0.2-24 cm; with goals solved onto the oar's reach 102 of 122 hand samples < 1 cm, the rest ~21 cm at the stroke extremes
  (out of reach - the IK takes it up) or during the 0.3 s ship->row blend. `[ik]` palm -> grip 0.0 cm (one 1.3 cm sample).
- Vest (standing player): bottom -16.4 -> -10.1 cm, top 37.2 -> 44.3 cm vs the pelvis bone (shoulder line 47.0).
- 0 LogLF warnings/errors. Shots: `Docs/Shots/T032_rowing_side_midstroke.png`, `T032_rowing_front_midstroke.png`, `T032_seated_idle.png`
  (look-around back, hands resting on the handles), `T032_rowing_side_two_crews.png`.
**Please check in PIE:** rowing look/feel with real oars (hands crossing at the centreline, stagger), seat + knees, head while rowing vs
idle + steering bias, vest height, the light-swing start (not filmed - needs a player swing).
**Known / follow-ups (not done):**
- The B09 socket names are mirrored: `Oarlock_L` imports at hull +Y (starboard). Code pairs hands to oars by distance, so nothing
  depends on the names; the rowboat script's "Port"/"Starboard" labels are also swapped (Codex, if it matters).
- Blade depth: during simultaneous full-throttle launches the blades read 40-74 cm under water for ~2 s (oars angled far aft + hull pitch);
  otherwise -30..+20. The session before these changes had a similar share (~10 % of readings below -30). Not tuned here.
- `.mcp.json`, `.cursor/mcp.json`, `.gemini/settings.json`, `.vscode/mcp.json` are already TRACKED (f0cbf0c), so .gitignore doesn't
  affect them. `git rm --cached` would stop tracking them but deletes them on the other machine at its next pull - your call.
- Audio during my test runs: the editor ran with `-nosound`. A music on/off option in a menu = separate task (ULFMusicSubsystem::
  SetMusicState(Off) / SetCategoryVolume already exist for it).

**Playtest 007 fix (director: "the left arm and the oar are glitching", `Playtest/videos/007 - rowing bug.mp4`)**
- Cause (mine, T03.2): the rower sits ~60 cm aft of the oarlocks; at the finish the hand goal's fore-aft/height offset from its oarlock
  reached ~65 of the oar's 70 cm, so the sideways solve collapsed (oar swung fore-aft, hand flung to the gunwale) and past 70 cm it
  switched off (hand snapped back). His left hand (Oarlock_R side) rowed 7 cm higher (stagger), so it hit the edge first. The playtest
  log shows it: grips 13 <-> 94 cm apart between samples, wrists out to 3.7 shoulder half-widths, hand target 21 cm off the grip.
- Fix: that offset is soft-clamped (tanh knee at 0.55 of the reach, limit 0.75 - `RowReachKneeFrac` / `RowMaxReachFrac`), continuous;
  "inboard" from the boat's centreline. The hand stagger is now fore-aft (12, left hand ahead) instead of vertical - the vertical
  stagger tilted the oars opposite ways (one blade mostly out of the water: L +3..+22 cm vs R -16). Stroke heights +6 mesh units
  (`RowDriveZ` -8, `RowRecoverZ` -18, `RowRestZ` -20) to undo the blade lift from the 10.4 cm seat drop. New in the `[oars]` log:
  largest frame-to-frame grip jump.
- Verified (player, `LFRow 1` then `-1`, AIs frozen; 1 build Succeeded each step): forward 25 samples - hand target -> grip 0.0 cm,
  grips 13.3-41.2 cm apart, largest grip step 3.1 cm/frame, wrists within 1.8; blades L -18..+7 (under in 17/25), R -15..+6 (19/25).
  Reverse 16 samples - 0.0 cm, 13.5-41.2 apart, step 1.0 cm/frame. 0 LogLF warnings. Strip: `Docs/Shots/T032_fix_rowing_strip.png`.

## L01 - lake layout (Claude Code, 14 Nov 11:22-11:55 IST)
Brief: replace the walled "pool" with a natural ~160 x 120 m lake for a 4-crew brawl (shore modules, B10 surroundings, docks,
islands/channels, reeds, dressing, FishZones, collision, evidence), then B11 extras. Lake material / hull stencil untouched.

**How it was built.** All placement over Unreal MCP from `Tools/L01_layout.py` (idempotent per pass: clears its outliner
folder `L01/<pass>` and re-places; one `SCALE`, the base ellipse `A_ELL/B_ELL`, `DOCK_R`, points/bays/islands at the top -
re-scale or reshape in a later pass by editing those and re-running). Outline = ellipse 86 x 58 m + noise + Gaussian
bumps solved so each dock sits in a bay at exactly 73.5 m shoreline radius, headlands frame each bay, E/W/S points.
Note: the 6cf172d rule (Claude commits Source/Config/Content/Docs) landed after I had committed `Tools/L01_layout.py` in
9e65fdb / e2b678d - tell me if it should move (e.g. to Docs/) or stay.

**Numbers**
- Outline 150 x 116 m (bbox), shoreline 453 m; water plane 171 x 139 m (LF_Lake Size), lake bed resized to match.
- Boat top speed (player, PIE, full throttle): **6.98 m/s** measured (code limit ThrustAccel/ForwardDrag = 7.33).
- Docks: all four **70.0 m** from the centre (water end) = **10.0 s** at top speed; measured from a standstill at the Red
  spawn (66 m out at the time) to the centre: **~11.5 s** (reached 14 m out at 9.5 s, 7.1 m/s).
- Spawns: 58 m out, 12 m in front of each dock, bow to the centre (was 4 m off the dock - the chase camera sat over the dock,
  director's report; fixed in 5820e17).
- Centre open water: clear radius 28.5 m (57 m across). Channels (island waterline -> shoreline): **N 17.0 m, E 19.5 m,
  W 19.5 m**. Tightest concave bay radius 7.1 m (boats 3.2 m).
- FishZones (TargetPoints): FishZone_Centre (72 m from the nearest dock), FishZone_ChannelN (57.7 m), FishZone_ChannelE (43.3 m).
- Actors (L01 folders): Shore 74 (69 Straight, 3 CornerOut, 2 CornerIn), Ground 226, Forest 177 (57 trees + 120
  TreeCluster), Clumps 46, Hills 9, Docks 8 (4 Dock_Team + 4 TeamFlag), Islands 11 (3 islands + dressing), Reeds 4, Dressing 32
  (logs, bushes, fences, 6 lily pads, 4 stumps, 6 buoys), Shack 8, FishZones 3, Sky 10, Wildlife 1 -> **609 L01 actors**,
  635 in the level.
- GPU (`ProfileGPU`, PIE, editor viewport): **3.41 ms -> 3.70 ms**, draws 307 -> 645, prims 418k -> 603k (before: player at the
  old centre start; after: player at the Red dock looking across the lake - not the identical view).

**Collision** (asset defaults over MCP - placed components use `bUseDefaultCollision`, so no per-actor overrides):
NoCollision on SM_LF_Hill_Far_01..03, Bush_01..02, TreeCluster_01, ReedBed, ReedBed_v1, Reeds, LilyPads, GroundPatch_01..02,
Logo_LakeFight, Logo_DirectorsCut (+ step 6: the 19 imported B11 meshes). Docks, shore, islands, rocks, stumps, logs, buoys,
crates, shack, fence, trees keep their default (BlockAll). Verified in PIE: player boat driven through reed bed 0 at speed ->
straight through (y 42.9 -> 55.0 m, reed at 48.1); driven at island N -> stopped at its waterline (28.9 vs 28.5 m) and deflected
(yaw 90 -> 138). Not driven: the shore and a rock (they block by the same asset default; the AI run bounced off nothing).

**AI**: no code change needed - its 3-trace WorldStatic avoidance sees shore/islands and ignores the NoCollision reeds. AI-only
run (`lf.ai.spectate`, ~42 s logged): 28 state changes, 10 rams, 3 fights, 2 capsizes, 3 climbs back aboard, 0 shore
wash-backs, 0 "fell" snaps, 0 warnings; all AI boats in open water 43-52 m inside the shoreline. Spectate camera fixed
(director: "way too high"): follows the closest pair of brawlers, 16-45 m (c466aa7). Earlier "everything is standstill" =
`lf.ai.freeze 1` left on from my tests (cvars persist across PIE sessions) - reset.

**Deviations / findings**
- **SM_LF_BankZone is not imported** (nor SM_LF_FishSpot, SM_LF_Swan_HeadNeck) - "imported assets only", so no BankZones.
- **SM_LF_GroundPatch_02 renders invisible from above** (holes showing the water plane / void; faces likely point down) -
  all ground uses _01 (105 tiles swapped). For Codex.
- Ground tiles sit at +0.36..0.42 m (local edges at Z=0), just under the shore crest, so the water plane (max wave 12.5 cm)
  can't show through. GroundPatch is NoCollision as asked: a brawler knocked far onto land would fall through it to the
  lake bed (the shore pieces keep collision 3 m inland).
- Editor-viewport dock shots (Blue/Green/Yellow) show editor icons (PlayerStart billboard, capsule cages); Red is a real PIE
  frame.

**Shots** (`Docs/Shots/`): `L01_top.png` (labelled: docks, islands, channels, reeds, FishZones, shack), `L01_dock_Red.png` (PIE
start view, with clouds), `L01_dock_Blue/Green/Yellow.png`, `L01_spectate.png`, `L01_heron.png`.

**Step 6 (B11):** NoCollision on all 19 imported B11 meshes; 10 clouds (Cloud_01..04) 252-372 m out, 95-136 m up, scale
1.3-2.3, random yaw - read from the chase cam, above the hills; SM_LF_Heron at the lake-side edge of reed bed 0, feet at the
waterline. No ducks/swans/junk/gulls/insects.

**Please review:** overall size and feel (one SCALE knob), the bays/points shape, channel widths, island dressing, spawn
distance, cloud sizes, the spectate camera.

## L01.1 - layout pass 2 (Claude Code, 14 Nov 11:57-12:55 IST)
Brief: 4-lobed lake with headlands, 4 islands on the routes home with lanes, reeds in lanes / bay mouths, fair fish zones,
lower/further hills, clouds +20-30 m, heron reed-bed fix, ground collision + land wash-back, import BankZone / FishSpot /
Swan_HeadNeck (+ GroundPatch_02 reimport and _01/_02 mix). Director's choices on the way: deeper peninsulas (option 3) -> the
symmetric X was rejected ("odd shape") -> option C with constraints (arms rotated 10-25 deg, lobes differ, wide rounded wooded
headlands, docks 70 m +/-5%, lanes ~9-12 / ~15-20 m); reed beds must not read as squares.

**How it's built:** `Tools/L01_layout.py` now runs inside the editor (`py "D:/Projects/LakeFight/Tools/L01_layout.py" [passes]`,
unreal API) - each run 4-6 s, logs `[L01] ...`. Arms (angle / dock distance / lobe width) and headlands (angle / tip radius /
width) are the shape knobs at the top; the outline is solved to those targets, then smoothed over 7 m.

**Numbers** (generator log + PIE):
- Arms: Blue 52 deg (+15 off diagonal), Green 160 (+17), Red -128 (+15), Yellow -50 (-13). Dock water ends Blue 72.0 / Green 68.5 /
  Red 71.0 / Yellow 69.5 m from the centre (= 10.3 / 9.8 / 10.2 / 10.0 s at the measured 6.98 m/s; within +/-3%). Lobes: Green
  narrowest (sigma 10 deg), Red fattest (17 deg).
- Headlands: tips at 28.6 / 30.4 / 37.2 / 29.4 m, sharpest tip radius 4.8 m, tightest water pocket 8.7 m. Open centre ~57 m across.
  Outline 120 x 126 m, perimeter 489 m (smaller than L01's 150 x 116 - the arms/headlands take the area).
- Islands (Island_M x1.1 = 12.1 m, trees + rock + bush): lanes narrow / wide - Blue 12.0 / 16.7, Green 9.3 / 17.5, Red 9.4 / 15.1,
  Yellow 9.6 / 15.3 m; 38-44 m from the centre.
- Reeds: 6 organic patches (one in each island's wide lane, two at the Green / Yellow bay mouths) = 317 SM_LF_Reeds clumps on a
  jittered 0.75 m grid inside noisy ellipses - no ReedBed meshes (they read as 4 x 3 m squares, director).
- Sight lines (centre, 2.5 m eye height, `Docs/Shots/L01_sightlines.png`): every dock is partly screened by its island's trees and
  framed by wooded headlands; Blue's is the most open (shack visible past the island). Physics traces to 5 points per dock (colliders
  only - TreeClusters / reeds are NoCollision): blocked Yellow 5/10, Blue 2/10, Red 2/10, Green 0/10.
- AI-only run (`lf.ai.spectate`, 94 s logged): 54 state changes, 18 rams, 10 fights, 1 overboard + 2 recoveries / climbs, 1 AI
  washed back from land by the new rule, 0 stuck boats (all three AI boats moved 10-40 m between every 20 s sample), 0 warnings.
- GPU (`ProfileGPU`, PIE, Red start view): 3.47-3.99 ms (two samples), 646-650 draws, 671-699k prims (L01: 3.70 ms / 645 / 603k).
- Ground: 303 + 42 gap tiles, GroundPatch_01 / _02 ~50/50 with random 90-deg yaw; a tile is only placed if all 9 points of its square
  are on land (106 skipped); two gap-fill passes (6 m, 3 m). Hole check: a pixel scan of the 1600 px top-down for water/void colour
  on land found 0 holes (9 samples = the editor axis gizmo + one blue prop pixel) - was 213 before the 3 m pass.
- Item 6: ground patches have one convex hull + BlockAll. `LFRagdollAt 80 -4600` (south headland): "[shore] ... lay on land 2.0 s
  (ground 81 cm above the water) -> into the water 1100 cm away, swimming" (`LandWashDelay` 2 s, `ShoreWashOffset` 2 m).
- Hills: 7 (was 9), Z 0.8-1.1, XY 1.6-2.0, 420-570 m out; ground extended to 640 m. Clouds 105-175 m up (+25).

**Findings / not done**
- **Fish zones can't be fair with two lane zones**: the picker (centre + 2 lane points >= 140 deg apart, minimising the spread of each
  dock's nearest lane zone) gives Green 37 / Yellow 36 / Red 63 / Blue 81 m. Suggest one lane zone per island (fair by construction).
- **The "green triangle"** = one of 8 broad low "leaves" (kite-shaped Moss shells, 0.42-0.58 x 0.6-0.9 m) built into SM_LF_ReedBed (v2)
  by its Blender script; ReedBed_v1 has broad stem leaves too. Both are no longer used (reed patches use SM_LF_Reeds). Codex could
  drop / shrink them in SM_LF_ReedBed.py.
- **SM_LF_Swan_HeadNeck**: no FBX in the repo - not imported. BankZone, FishSpot (NoCollision) and GroundPatch_02 imported.
- No CornerOut shore pieces now (their R4 turn made squared notches on the wide headlands); CornerIn at the dock bays.
- **Feel:** from the chase camera the arms read noticeably tighter than L01 (lanes 9-17 m) - see the dock shots before deciding.
- **Two mistakes of mine** while regenerating: my console helper pressed Ctrl+A / Backspace when the console box wasn't focused, which
  selected and deleted every actor in the open level (twice - "the level looks empty"). Nothing was saved; the map was reloaded from
  disk both times. The helper no longer sends those keys. My wait loops also mis-detected finished runs, so "runs taking long" was
  idle waiting, not the generator.
- Director note: the `lf.ai.spectate` tracking camera = marketing-video tool (recorded in Claude memory).
- **Sky (director):** the default Unreal realistic sky still clashes with the game's low-poly look. The level uses the template
  `SkyAtmosphere` + `VolumetricCloud` (+ `SM_SkySphere`, `SkyLight`, `ExponentialHeightFog`): soft photoreal volumetric clouds
  behind our faceted SM_LF_Cloud meshes. Not changed in L01.1. Suggested next pass: remove the VolumetricCloud, replace the sky with
  a simple stylized gradient (flat-shaded sky sphere / sky material driven by the existing light), keep only the low-poly clouds,
  re-tune fog + SkyLight so the hills and forest edge keep their haze.

**Shots** (`Docs/Shots/`, PIE game frames, no editor icons): `L01_top.png` (labelled, from a temporary evidence camera),
`L01_dock_Red/Blue/Green/Yellow.png` (each crew's game camera at its spawn), `L01_sightlines.png`, `L01_heron.png` (editor view of
the reed patch + heron).
