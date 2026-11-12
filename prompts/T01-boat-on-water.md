# T01 - Boat on water (Thu 12 Nov, ~10:50-12:45 IST) - agent: Claude Code (Unreal MCP)

Copy into `Docs/CurrentTask.md`.

## Goal
On a small greybox lake, the player drives a rowboat that bobs, coasts and turns with arcade weight. A second,
unmanned boat floats nearby and can be rammed, and both react with a satisfying shove and rock.

## In scope
- Map `/Game/LakeFight/Maps/L_Lake01`: ~120 x 80 m lake plane, simple shore (greybox cubes/landscape is fine),
  directional light + sky. One small dock (greybox).
- Water: `LF::Water::GetHeightAt(FVector Pos, float Time)` in C++ (3 summed directional sine waves, amplitude
  <= 0.15 m). Material on the lake plane uses the SAME constants via a Material Parameter Collection `MPC_LF_Water`
  so the visible surface matches the physics.
- `ULFBuoyancyComponent`: N pontoon points (default 4, editable); each applies an upward spring + damping force
  based on depth below `GetHeightAt`. Separate linear drag (forward low, lateral high) and angular drag.
- `ALFBoat` (Pawn, physics-simulated hull - use a box/capsule hull until the Blender mesh lands):
  throttle forward/back + turn; thrust applied at the stern below the waterline so the bow lifts slightly.
  Coasts for 2-3 s after release. Turning at speed leans the boat outwards a little.
- Camera: spring-arm chase cam with lag; looks good in a 30 s clip.
- Input: Enhanced Input, WASD + gamepad left stick/triggers.
- Collision response between boats: impulse scaled by relative speed + extra roll torque so hits look comedic.
  Placeholder splash: Niagara burst or debug sphere at the contact point.
- When `Art/Export/SM_LF_Rowboat.fbx` exists (Blender task B01), import with `Tools/import_fbx.py` and swap it in
  as the visual mesh (keep the simple hull for physics).

## Out of scope
Characters, paddles, swimming, AI, fish, UI, audio, Water plugin.

## Acceptance (automated)
- [ ] Editor target compiles.
- [ ] PIE on `L_Lake01` runs 60 s with no errors or warnings from LF classes in the Output Log.
- [ ] Boat left alone settles and floats level at the waterline within 3 s (log its Z and pitch/roll once settled).

## Acceptance (manual - human)
- [ ] Driving feels heavy-but-responsive: I can line up a ram on the second boat on the first try.
- [ ] Ramming at full speed makes both boats rock and slide in a way that makes me smile.
- [ ] The boat never clips through or flies off the water during normal driving.

## Stop boundary
Stop when the above passes or at 12:45 IST. Commit, report, add one DirectorsLog line.
