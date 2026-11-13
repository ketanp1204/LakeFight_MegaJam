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
