# CurrentTask - T01: Boat on water (Thu 12 Nov, ~10:50-12:45 IST) - agent: Claude Code (Unreal MCP)

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
- [x] Editor target compiles.
- [x] PIE on `L_Lake01` runs 60 s with no errors or warnings from LF classes in the Output Log.
- [x] Boat left alone settles and floats level at the waterline within 3 s (log its Z and pitch/roll once settled).

## Acceptance (manual - human)
- [ ] Driving feels heavy-but-responsive: I can line up a ram on the second boat on the first try.
- [ ] Ramming at full speed makes both boats rock and slide in a way that makes me smile.
- [ ] The boat never clips through or flies off the water during normal driving.

## Stop boundary
Stop when the above passes or at 12:45 IST. Commit, report, add one DirectorsLog line.

## Report (Claude Code, 12 Nov ~11:20-11:37 IST)

**Files changed**
- New C++: `Source/LakeFight/Water/LFWater.h/.cpp` (`LF::Water::GetHeightAt`, wave table, `PushToMPC`, `LogLF`),
  `Water/LFBuoyancyComponent.h/.cpp`, `Water/LFLakeSurface.h/.cpp` (procedural 120x80 m grid, 1 m cells, pushes the MPC at BeginPlay),
  `Boat/LFBoat.h/.cpp` (physics pawn, stern thrust, turn, outward lean, chase cam, Enhanced Input, boat-vs-boat hit + `OnSplash` debug-sphere hook, `[settle]` log).
- `LakeFight.Build.cs` + `LakeFight.uproject`: added `ProceduralMeshComponent` (module + plugin).
- Assets (via unreal-mcp, all saved): `/Game/LakeFight/Maps/L_Lake01` (copied from the engine Basic template; floor removed; lake, 4 shore banks,
  lake bed at -4 m, greybox dock on the west shore, PlayerStart at origin, unmanned target boat 15 m ahead; World Settings GameMode = BP_LF_GameMode),
  `Materials/MPC_LF_Water`, `Materials/M_LF_Lake`, `Blueprints/BP_LF_Boat`, `Blueprints/BP_LF_GameMode` (DefaultPawn = BP_LF_Boat),
  `Input/IA_LF_Throttle`, `Input/IA_LF_Steer` (dead zone), `Input/IMC_LF_Boat` (W/S, Up/Down, RT/LT; A/D, Left/Right, left stick X).
- `Docs/CurrentTask.md` (T01 copied from `prompts/T01-boat-on-water.md`), `Docs/DirectorsLog.md`.

**Wave sync:** the constants live ONLY in `Source/LakeFight/Water/LFWater.cpp` (`WaveTable`, 3 waves, amplitude sum 12.5 cm).
`ALFLakeSurface::BeginPlay` pushes them into MPC_LF_Water (`Wave0..2` = DirX, DirY, Amp, Wavelength; `WaveSpeeds` = S0, S1, S2, BaseHeight).
M_LF_Lake's Custom node evaluates the same formula into World Position Offset. The MPC defaults are a copy, so the editor viewport matches;
if you edit `WaveTable`, the runtime stays in sync automatically (only the editor preview would drift).

**Commands / results**
- `Tools/ue.ps1 -Action build -Project LakeFight.uproject -EngineDir D:\UE_EpicGames\UE_5.8` -> first try: 1 error (const UObject* passed to PushToMPC), fixed -> **Succeeded**.
- PIE on L_Lake01 via MCP, 06:06:12-06:07:3x UTC (>60 s). LogLF:
  - `[settle] BP_LF_Boat_C_1 (player) t=3.3s Z=-4.6 water=2.2 (diff -6.8 cm) pitch=-0.33 roll=0.45 -> level`
  - `[settle] BP_LF_Boat_C_0 (target) t=3.3s Z=1.9 water=3.7 (diff -1.9 cm) pitch=0.20 roll=-0.91 -> level`
  - No warnings or errors of any category in the Output Log while PIE ran. (The only LakeFight-related warnings came from my own failed MCP property sets before PIE.)

**You must check by hand (PIE; I could not drive the boat through MCP):** throttle, turning and coast feel; the bow lifting under power; the outward
lean in turns; ramming the target at full speed (shove + roll + cyan debug sphere); keyboard AND gamepad; the camera in a 30 s clip; and how the lake looks once Lumen settles.

**First 3 tuning knobs** (BP_LF_Boat class defaults)
1. `LF|Drive > TurnAccelDeg` = 140 -> lower (100) if turning is twitchy and lining up a ram is hard; raise if it feels like a barge.
2. `Buoyancy > LF|Drag > ForwardDrag` = 0.45 -> lower (0.35) for a longer coast; raise for a quicker stop. Top speed ~ ThrustAccel (330) / ForwardDrag.
3. `LF|Hit > HitRollScale` = 0.45 -> raise (0.7) for sillier rocking on rams; lower if boats flip. `HitShoveScale` (0.5) is its partner.

**Follow-ups**
- SM_LF_Rowboat.fbx (B01/B01.1, Codex) exists but is NOT imported: unreal-mcp here has no Python/console exec. Run in the editor console:
  `py "D:/Projects/LakeFight/Tools/import_fbx.py" SM_LF_Rowboat`, then set BP_LF_Boat > VisualMesh = SM_LF_Rowboat at scale 1,1,1 (keep the box Hull for physics).
- `Tools/ue.ps1` doesn't find the engine at `D:\UE_EpicGames\UE_5.8`; pass `-EngineDir` or set `UE_ROOT`.
- The project default map / GameMode is still ThirdPerson (`Config/DefaultEngine.ini`). PIE uses the L_Lake01 World Settings override.
- The lake material has a flat normal (waves move the geometry but add no lighting detail); compute a normal from the wave derivatives later.
- A `BuoyancyManager_0` actor came from the engine template; check whether the Water plugin is enabled by default (we don't use it).
- No camera kick / sound on hits yet (`OnSplash` is the hook).
- `git add -A` (as instructed) also committed pre-existing untracked files: `Tools/__pycache__/`, `Art/BlenderScripts/__pycache__`, `prompts/`, `submission/`,
  `templates/`, `Tools/init-jam-repo.ps1`, and Codex's B01 files (`Art/BlenderScripts/SM_LF_Rowboat.py`, `Art/Export/`).

## T01.1 - boat handling + cleanup (Claude Code, 12 Nov ~11:50-12:05 IST)

**Files changed**
- `Source/LakeFight/Boat/LFBoat.h/.cpp`: rudder = lateral force at `RudderPoint` scaled by speed-based authority (the direct yaw torque is removed);
  throttle spool; reverse 40%; squat; camera yaw lag separate from pitch lag (manual arm rotation); hull fitted to 300x135x50 cm at the waterline origin;
  SM_LF_Rowboat as the default VisualMesh (scale 1, NoCollision); `HullPhysMaterial` (+ runtime fallback); clamped hit shove/roll.
- `Source/LakeFight/Water/LFBuoyancyComponent.h/.cpp`: drag moved to `LF|Handling`; LateralDrag 3.0->2.0, yaw AngularDrag 1.8->1.0; pontoons at the hull corners (+-140, +-60).
- `LakeFight.Build.cs`: + PhysicsCore. `Config/DefaultEngine.ini`: GameDefaultMap / EditorStartupMap = L_Lake01, GlobalDefaultGameMode = BP_LF_GameMode.
- `.gitignore`: `__pycache__/`, `*.pyc`; untracked 3 pycache folders (`Tools/`, `Art/BlenderScripts/`, `templates/Art/BlenderScripts/`).
- Assets: `M_LF_Lake` (WPO node untouched; opaque; base colour lerps deep teal 0.02,0.10,0.12 -> crest 0.05,0.20,0.22 by wave height; Metallic 0,
  Specular 0.5, Roughness 0.3; faceted normal `normalize(cross(ddx(P), ddy(P)))` with Tangent Space Normal OFF; **Two Sided ON** - see below),
  `PM_LF_Hull` (created by the director; I set Friction 0.15 Min, Restitution 0.35 Max), `BP_LF_Boat` (HullPhysMaterial = PM_LF_Hull),
  `L_Lake01` (SkyLight Real Time Capture ON). Also `Content/LakeFight/Meshes/*` (SM_LF_Rowboat + Blue + materials, imported by the director).

**Incident:** following the spec, I turned Two Sided OFF and the water vanished (the director caught it): the procedural grid's triangles face
down, so the surface was back-face culled. Fixed by setting Two Sided back ON. The proper fix is to flip the winding in `LFLakeSurface.cpp` (follow-up).

**Commands / results**
- `Tools/ue.ps1 -Action build ... -EngineDir D:\UE_EpicGames\UE_5.8` -> Succeeded on the first try.
- PIE run 1 (water culled): `[settle] player t=3.0s Z=-2.9 water=-0.7 (diff -2.1 cm) pitch=-0.81 roll=1.07 -> level`; target diff -0.6 cm, pitch 0.20, roll -0.90.
- PIE run 2 (after the fix), 06:30:12-06:31:26 UTC (>60 s), no warnings/errors of any category:
  `[settle] BP_LF_Boat_C_1 (player) t=3.0s Z=-2.9 water=-0.7 (diff -2.2 cm) pitch=-0.81 roll=1.07 -> level`
  `[settle] BP_LF_Boat_C_0 (target) t=3.0s Z=4.5 water=5.1 (diff -0.6 cm) pitch=0.20 roll=-0.90 -> level`
  (I can't confirm from logs that the waterline is just below the red stripe - check by eye.)

**New / changed tunables (defaults)** - ALFBoat `LF|Handling`: ThrustAccel 330, ReverseThrustScale 0.4, ThrottleRiseTime 0.6 s, ThrottleFallTime 0.4 s,
ThrustPoint (-140,0,-20), ThrustMaxAirGap 10, SquatAccelDeg 60, RudderPoint (-130,0,-20), RudderForceAccel 85 cm/s^2, RudderFullSpeed 400 cm/s,
MinTurnAuthority 0.15, ReverseRudderScale 0.5, LeanAccelDeg 120. Buoyancy `LF|Handling`: ForwardDrag 0.45, LateralDrag 2.0, AngularDrag (yaw) 1.0, TiltDrag 1.0.
`LF|Camera`: CameraPitch -16, CameraYawLagSpeed 2.0, CameraPitchLagSpeed 8.0, CameraPitchFollow 0.25. `LF|Hull`: HullPhysMaterial PM_LF_Hull,
HullFriction 0.15, HullRestitution 0.35. `LF|Hit`: HitShoveScale 0.35, HitPopScale 0.06, MaxHitImpulse 250 cm/s, HitRollScale 0.3, MaxHitRoll 1.2 rad/s.
Removed: TurnAccelDeg, StationaryTurnScale, FullEffectSpeed.

**If it still feels like a car, try first**
1. `RudderFullSpeed` 400 -> 550 (the rudder needs more speed before it bites; slow turns get lazier).
2. `Buoyancy.AngularDrag` 1.0 -> 0.7 (the turn carries on longer, so you have to counter-steer).
3. `ThrottleRiseTime` 0.6 -> 1.0 (heavier pull-away).

**Check by hand:** stern swing / bow pivot in turns, counter-steer feel, reverse steering, squat + bow lift, the camera swinging in turns,
the full-speed ram (rocks, never flips or launches), the waterline sitting just below the red stripe, and the lake look (it reads lighter than "deep teal" in
the editor viewport - probably auto-exposure).

**Follow-ups:** **water shows inside the rowboat** (director spotted it): the waterline (origin) is above the hull's inner floor and nothing occludes
the lake inside an open hull. Proposed fix: a custom-stencil water mask (an invisible cap mesh in BP_LF_Boat writes custom depth/stencil; M_LF_Lake becomes Masked
and clips water pixels behind it; needs r.CustomDepth=3). Flip the LFLakeSurface triangle winding, then Two Sided can go OFF; exposure/post-process for the lake colour; MCP still has no Python exec
(the import step needs the human).
