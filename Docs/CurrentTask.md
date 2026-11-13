# T02 - The cut features: on foot, paddle, overboard, swim, board, steal (Fri 13 Nov) - agent: Claude Code

These are the features the original Lake Fight deferred. They are the headline of the Director's Cut.

## Goal
The player is a character standing IN the boat. They can swing a paddle, knock people overboard, swim, climb into
any boat and drive it away.

## In scope
- `ALFBrawler` (Character): states InBoat-Driving / InBoat-Standing / Swimming / Climbing / Ragdoll-lite.
  While InBoat, the character is based on the boat (moves with it, sways with roll; no sliding off unless hit).
- Toggle drive/stand (one button). Driving uses T01 controls; standing frees the character to move inside the boat.
- Paddle swing (light, charged): sweep trace; hit = knockback impulse + short stun; strong hit or hit near the
  gunwale = falls overboard. Hitting a boat hull rocks the boat.
- Overboard: splash, Swimming state (slow, bobbing on `GetHeightAt`), can grab any boat's side within 1.5 m ->
  Climbing (0.8 s, interruptible by a paddle hit) -> InBoat.
- Stealing: if you climb into a boat with no one driving, you can drive it. If someone is driving, you must knock
  them out first.
- Jumping boat-to-boat when hulls are within 2.5 m.
- Placeholder anims are fine (mannequin + simple procedural lean). Comedy beats > fidelity.

## Out of scope
AI, fish/scoring, menus, final art/audio.

## Acceptance (manual)
- [ ] Knocking a dummy overboard is funny the first ten times.
- [ ] Swim -> climb -> drive away in someone else's boat takes under 10 s and feels like a heist.
- [ ] Standing in a rocking boat never launches the character into orbit.

## Director's T02 brief (13 Nov, stop 17:30 IST)
Order: 1 stand/drive in boat -> 2 paddle swing + dummy -> 3 overboard/swim/climb/steal -> 4 boat-to-boat jump -> 5 audio hook events.
STOP after steps 1, 2 and 3 for a PIE test by the director. Respect T01.x: hull = SM_LF_Rowboat UCX, 180 kg, CoM -15 cm; a character
must not add mass or push its boat; camera feedback on real time; look locked (PPV_LF_Look_B, MI_LF_Lake, hull stencil untouched);
paddle mesh SM_LF_Paddle. (T01.x reports are in git history of this file.)

## Step 1 - ALFBrawler standing/driving (Claude Code, 13 Nov ~12:27-12:35 IST)

**Files**
- New `Source/LakeFight/Brawler/LFBrawler.h/.cpp` - `ALFBrawler` (Character), `ELFBrawlerState` (None/Standing/Driving/Swimming/Climbing/Ragdoll;
  only Standing/Driving used so far).
- New `Source/LakeFight/Input/LFInput.h/.cpp` - runtime Enhanced Input actions shared by brawler and boat: Interact (E / gamepad Y)
  and Swing (LMB / right trigger, for step 2). Move/look reuse the template's IA_Move, IA_Look, IA_MouseLook + IMC_Default, IMC_MouseLook.
- `Source/LakeFight/Boat/LFBoat.h/.cpp` - crew API: `DriverSeatLocal` (-55, 0, -9 cm = on the floor UCX), `Get/SetDriver`, `HasDriver`,
  `GetDriverSeatWorld`, `GetHull`; Interact while driving = leave the seat; `UnPossessed` removes the boat's mapping contexts.
- Assets: new `Blueprints/BP_LF_Brawler` (child of ALFBrawler, defaults from C++); `BP_LF_GameMode` DefaultPawn BP_LF_Boat -> BP_LF_Brawler.
- Also in this commit: the director's own saved `L_Lake01` (PPV_LF_Look_B enabled) and `MI_LF_Lake` changes (were uncommitted).

**How it works**
- Player start: the brawler is the default pawn; on possession it spawns its own BP_LF_Boat on the water under the PlayerStart and stands
  at the driver seat spot. (The placed LF_Boat_Target stays empty.)
- Standing: the CMC walks on the hull (movement base = HullMesh) but the capsule is **QueryOnly** - sweeps hit the hull, physics never does -
  and `bEnablePhysicsInteraction` is off: no weight, no push, no impulses on any boat. `bCanWalkOffLedges` off and step height 15 cm
  (floor-to-gunwale ~40 cm), so you can't walk or step out. Walk speed 220. The mesh leans with the boat's tilt about the feet
  (`SwayScale` 0.7, `SwayFollowSpeed` 8); the capsule stays upright.
- Drive/stand toggle = **E / gamepad Y**. Driving: brawler attached at the seat, collision off, the PlayerController **possesses the boat**
  (T01 controls + existing boat camera, ram feedback works). Leaving: unattach, stand at the seat, possess the brawler.
- Camera (standing): third-person spring arm on control rotation (mouse/right stick orbit), length 650, pivot = 40 % of the way from
  brawler to boat centre, lagged on REAL time (`CameraBoatFrame`, `CameraLagSpeed` 6).
- Jump is deliberately not bound yet (step 4); there's no swimming until step 3, so leaving the boat would strand you.

**Commands / results**
- Build: 1 compile error (`FActorSpawnParameters::SpawnCollisionHandlingOverride` name) fixed -> **Succeeded**. Editor closed/reopened.
- PIE 07:03:34-07:04:58 UTC (84 s), no warnings/errors of any category:
  `[brawler] BP_LF_Brawler_C_0 Standing in BP_LF_Boat_C_0` / `spawned home boat` / `[settle] ... -> level` (both boats, same as T01) /
  `Driving BP_LF_Boat_C_0` (E pressed via the Slate inspector) / `Standing in BP_LF_Boat_C_0` (E again). Screenshots checked: standing view
  frames brawler + boat; driving uses the boat camera with the brawler at the stern.

**Please test (PIE):** walk around inside the boat (WASD/left stick, mouse/right stick to orbit) - can't leave, never launched;
drive with E, steer/ram, E again to stand; while standing, the boat rocking and you leaning with it; whether the brawler's weight
"not counting" reads OK (boat doesn't tilt when you walk to one side - by design for now).
**Known:** the mannequin is big for a 3 m rowboat (comedy, or scale it later); idle/walk anims only, no sitting pose when driving.

**Director: step 1 accepted.**

## Step 1 adjustments (a, b) + Step 2 - paddle (Claude Code, 13 Nov ~12:40-13:00 IST)

**a. Scale 0.75** - capsule 22.5 x 66 (was 30 x 88), mesh scale 0.75 at -66, camera boom 650 -> 500 (offset z 90 -> 70), walk speed
220 -> 180, boat step height 15 -> 12. Boat unchanged.

**b. Fake weight tilt** (`LF|Brawler`): while Standing, a PURE torque = (smoothed horizontal offset of the brawler from the hull's CoM)
x (`FakeWeightKg` x g) - no net force, so no sinking and still no added mass. It fades to zero as the boat's lean toward the brawler
approaches `MaxFakeTiltDeg` (4 deg), so by itself it can never tilt past 4 deg or flip the boat; the brawler is never launched (it doesn't
touch the physics). `FakeTiltFollowSpeed` 3/s smooths it; Driving (seated) and overboard decay it to zero.
`FakeWeightKg` 120: measured pitch with a brawler standing at the stern seat = 2.05 deg (vs 0.82 with nobody; 40 kg gave only ~0.6 deg).
The T01 `[settle]` check now allows 6 deg (2 waves + 4 crew tilt) - it flagged the deliberate 2 deg as "NOT level".

**Step 2 - paddle**
- `ALFBrawler`: SM_LF_Paddle on a right-shoulder pivot (grip end, ~130 cm reach to the blade centre). Placeholder procedural animation:
  idle (held down at the side) -> wind-up while holding -> arc from behind-right to the left -> recover. The brawler turns to the camera's
  yaw when swinging (aim with the camera).
- Input: **LMB / right trigger**. Tap (< 0.2 s) = light swing (strength 0.35, 0.22 s arc); hold = charged (0.55 -> 1.0 at 0.8 s hold,
  0.3 s arc, paddle trembles at full charge).
- Hit detection: each frame of the arc, a 28 cm sphere sweep from the blade's previous to current position (Pawn + PhysicsBody objects,
  ignores self and own boat). One brawler per swing.
- Hit on a brawler: stun 0.4-1.0 s (no move/swing) + knockback 180-320 cm/s + hop. **Overboard** if strength >= 0.75 (charged) OR the victim
  stands near the gunwale (|local Y| > 20 cm, or near bow/stern): launched up and over the nearest side (450 out, 520 up), tumbling, splash
  (NS_LF_Splash) on hitting the water. Hitting someone who is driving knocks them out of the seat first.
- Hit on another boat's hull: velocity change at the hit point (120 cm/s x strength) - the boat rocks/turns.
- Feedback (real time, same approach as T01.3): one decaying sine camera shake + FOV punch, rumble, hitstop 0.04-0.09 s.
- Temporary until step 3: an overboard brawler lies in the water and is put back in its boat after 2.5 s (so the dummy can be hit again).
- Hooks: `OnPaddleHit(Victim, Strength, bOverboard)`, `OnSplashIn(Location)` (BlueprintImplementableEvent; step 5 adds the rest).
- Debug console (works in PIE): `LFSwing [strength]` (no arg = light), `LFDrive` (toggle).
- Level: `LF_Dummy` (BP_LF_Brawler, no controller) placed in LF_Boat_Target via `StartBoat` (L_Lake01 saved).

**Commands / results**
- 3 builds (adding console commands; reach/weight; diagnostics) - all Succeeded.
- PIE 07:27:16-07:29:21 UTC (125 s); LF log, with the target boat teleported alongside (y = 150) through MCP:
  `[settle] ... pitch=2.05 ... -> level` (fake weight) /
  `swing light strength=0.35` -> `hit at local (-52, -1)` -> `HIT ... -> knockback+stun` /
  `swing CHARGED strength=1.00` -> `hit at local (-53, 24) ... >= overboard strength near gunwale` -> `OVERBOARD` -> `splash at (-59, 359)` ->
  `reset into BP_LF_Boat_C_1`. No LF warnings; the only errors were from my MCP calls during PIE ("Editor is currently in a play mode").
  Screenshot: dummy tumbling through the air over the far side.

**Please test (PIE):** the dummy is 15 m ahead in LF_Boat_Target - drive over (E), pull alongside, E to stand, aim with the camera,
tap / hold LMB. Is it funny the first ten times? Reach, knockback, stun and overboard thresholds are all under `LF|Paddle` on BP_LF_Brawler.
**Known:** procedural paddle/no arm IK (the arms stay idle); the tilt is subtle by design (raise FakeWeightKg).

**Director: step 2 paddle + hits accepted; overboard rejected** (rigid upright dummy launched 5-8 m like a cannonball, feet-first landing
holding the paddle up, teleport back). -> step 2.1.

## Step 2.1 - ragdoll overboard (Claude Code, 13 Nov ~13:05-13:30 IST)

**Changes** (`Source/LakeFight/Brawler/LFBrawler.h/.cpp`; tunables under `LF|Overboard`):
1. Overboard = real ragdoll: the CMC stops, the capsule's collision goes off, and the mesh simulates with Manny's physics asset
   (PA_Mannequin, 22 bodies) on collision profile `Ragdoll`. The paddle re-attaches to `hand_r`. (The old LaunchCharacter + spinning
   rigid mesh are gone.)
2. Launch: every body gets a mostly HORIZONTAL velocity away from the hitter (`OverboardHorizSpeedMin/Max` 230/320 cm/s) with a small
   upward part (`OverboardUpSpeedMin/Max` 180/240); spine_03/spine_05/neck/head get `OverboardToppleSpeed` +160 cm/s extra, so the upper
   body leads, the legs catch the rail and he topples backward over it.
3. Ragdoll vs boats: bodies use `RagdollMassScale` 0.25 (they can't shove a 180 kg hull) and collide with hulls only for
   `RagdollHullCollideTime` 0.3 s (the rail trip), then ignore PhysicsBody/Vehicle/Pawn.
4. Water: every body below `GetHeightAt` gets buoyancy (`WaterBuoyancyAccel` 1500 cm/s^2 for pelvis/spine/head, x0.6 for limbs, ramping
   over 25 cm of depth) and strong damping (`WaterDamping` 4/s): a limp, settling bob. Overboard splash at the entry point, scaled
   `OverboardSplashScale` 1.8x, + `OnSplashIn`.
5. Recovery: after `FloatTime` 1.3 s (or once the pelvis is nearly still after 0.6 s) the capsule is placed upright at the pelvis, half
   in the water (facing the pelvis yaw), and the physics blend weight falls 1 -> 0 over `RecoverBlendTime` 0.3 s (no teleport: the bodies
   blend into the animated pose), then simulation stops and the mesh/paddle re-attach. Then upright bobbing in the water ->
   `OverboardResetDelay` 2.5 s -> back into the boat (placeholder until step 3; not teleported from mid-air any more).
6. Boat-vs-boat: unchanged by T02 (the brawler capsule is query-only; the ragdoll ignores PhysicsBody after 0.3 s). **Verified in PIE:**
   the target boat teleported into a 30 cm overlap with the player's boat was pushed apart (centres 1.0 -> 1.7 m in 1.5 s) - hulls block.
   The visible overlap in the recording is most likely the UCX inset: the side UCX walls sit ~5-9 cm inside the visual hull (outer face
   y ~0.578 m vs gunwale 0.65-0.67 m amidships; ~4 cm at the bow), so two hulls can look 10-15 cm overlapped while their collision
   only just touches. Fix = widen the UCX walls in `Art/BlenderScripts/SM_LF_Rowboat.py` (Codex / Art) - not changed here.

**Verification (PIE, `LFSwing 1`, boats side by side 1.5 m apart; 3 full-charge runs):**
| Run | Apex above gunwale | Landed outside hull | Flight | Float |
| --- | --- | --- | --- | --- |
| 1 | 65 cm | 157 cm | 0.69 s | 1.30 s |
| 2 | 70 cm | 141 cm | 0.97 s | 1.30 s |
| 3 | 69 cm | 142 cm | 0.98 s | 1.30 s |
Targets: apex <= ~1 m, landing ~1-1.5 m (charged a bit further) - met. Then `upright in the water` -> `reset into BP_LF_Boat_C_1`.
A screenshot strip from the player's camera shows the dummy horizontal at the water's edge after toppling; I could not get a second camera
on the float in PIE (MCP can't set the PIE control rotation; the viewport capture shows the editor world). Light near-gunwale hits use
the Min values (lower and shorter) - not separately measured.
PIE 07:55:42-07:57:46 UTC (124 s): no LF warnings; the only errors came from my MCP calls during PIE.
Build: 1 compile error (missing `PhysicsEngine/BodySetup.h`) fixed -> Succeeded.

**Please test:** whether the topple over the rail reads, the limp float (face-down-ish is not forced - it depends on how he lands), and the
blend to upright. If he flies too far/near: `OverboardHorizSpeedMin/Max`; too high: `OverboardUpSpeed*`; not tipping: `OverboardToppleSpeed`.

**Director: step 2.1 accepted. Step 4 (boat-to-boat jump) deferred to Saturday. Next: step 3, then step 5 only.**

## Step 3 - swim, climb, steal (Claude Code, 13 Nov ~16:16-16:30 IST)

**Files:** `Source/LakeFight/Brawler/LFBrawler.h/.cpp`, `Source/LakeFight/Boat/LFBoat.h/.cpp` (`GunwaleDistance`). No asset changes.

- **Swimming** (`LF|Swim`): the ragdoll recovery (2.1) now ends in Swimming - the 2.5 s reset placeholder is removed. CMC in Flying mode,
  `SwimSpeed` 120 cm/s, move input camera-relative; every tick the capsule is pinned to `GetHeightAt` (centre `SwimSubmerge` 0.5 x half-height
  under the surface: head and shoulders out, bobbing). Placeholder swim pose: the mesh leans forward up to `SwimLeanDeg` 45 with speed.
  Camera stays third-person, framing just the brawler while not in a boat. No stamina.
- **Grab**: `ALFBoat::GunwaleDistance` = signed distance to the real gunwale outline (RowboatPlan, through the hull transform). Within
  `GrabRange` 150 cm of ANY hull an on-screen prompt shows "[E / Y] climb aboard <boat>" (dev-build debug text until there is UI);
  Interact = grab the nearest.
- **Climbing** (`ClimbTime` 0.8 s): kinematic, in hull space (a moving/rocking boat carries the climb): rise at the side above the rail
  (`ClimbOverHeight` 35 cm), then a small hop over and down onto the floor just inside the side nearest you. Any paddle hit while
  climbing or swimming -> ragdoll overboard again (0.6x strength; logs `[climb] ... knocked off`).
- **Steal**: Interact drives only within `DriveReach` 80 cm of the driver seat and only if the seat is empty (prompt "[E / Y] drive" /
  "[E / Y] STEAL this boat"). Driving a boat that isn't your home boat logs `[steal]`. A driver must be paddled out of the seat first (2.x).
- **AI-ready** (BlueprintCallable): `StartSwimTo(Target)`, `StopSwim()`, `TryGrabNearestHull()`, `TryGrabHull(Boat)`, `TryDriveBoat()`,
  `Swing(Strength)`, `FindGrabbableHull()`, `GetHomeBoat()`. Player input calls the same paths.
- **Dummy** (no controller, `bAutoReturnHome`): after `AutoReturnDelay` 0.6 s of swimming it swims to its home boat (the first boat it
  stood in) and grabs/climbs in, ready to be hit again.
- Debug console: `LFOverboard [side]` (throw yourself overboard, +1 right / -1 left), plus `LFSwing`, `LFDrive` (= Interact: grab when swimming).

**Verification (PIE, logs):**
- Dummy: `LFSwing 1` -> ragdoll, splash 143 cm outside -> floated 1.30 s -> `[swim] swimming` -> `[climb] grabs BP_LF_Boat_C_1 (119 cm from
  the gunwale, starboard side)` -> `aboard BP_LF_Boat_C_1 (home)`.
- Player heist: `LFOverboard -1` -> apex 54 cm, landed 105 cm out, floated 1.31 s -> swimming -> (target boat moved next to the swimmer via
  MCP) `LFDrive` -> `grabs BP_LF_Boat_C_1 (64 cm from the gunwale, starboard side)` -> `aboard BP_LF_Boat_C_1 (seat empty: steal it!)` ->
  `LFDrive` -> `[steal] BP_LF_Brawler_C_1 steals BP_LF_Boat_C_1` -> `Driving BP_LF_Boat_C_1`. Screenshot: player driving the stolen boat.
- PIE 10:53:34-10:54:43 UTC (69 s), no LF warnings (only my MCP teleport's "Editor is in play mode" errors). 2 builds Succeeded (one retry
  after the editor took >60 s to close).
- **Not exercised by me:** hitting someone mid-climb (timing via console wasn't practical), swimming with real WASD input, and the on-screen
  prompts (the shot was taken while driving).

**Please test:** swim feel (speed, bob height), the climb look, the heist time (target < 10 s), knocking a climber back off.
**Known:** a stealer and a non-driving owner can stand on top of each other at the seat; climb/swim are placeholder poses (no anims).
