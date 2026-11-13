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

## T01.2 - UCX collision, bump vs ram, ram juice, hull water mask (Claude Code, 12 Nov ~12:05-12:45 IST)

**Files changed**
- `Source/LakeFight/Boat/LFBoat.h/.cpp`: box hull + VisualMesh removed; new root `HullMesh` = SM_LF_Rowboat simulating with its own 6 UCX convexes;
  mass override 180 kg; COM placed at (0,0,-15) cm (the shape's own COM was (-4.4, 0, -2.0), so the nudge is computed at BeginPlay, `[hull]` log);
  `WaterMask` procedural cap (gunwale outline from `Art/BlenderScripts/SM_LF_Rowboat.py` PLAN/sheer, custom depth + stencil 1 only, built in
  OnConstruction so it also works in the editor); bump/ram tiers + `[hit]` log; ram: target shove/heave/roll/pitch kick, rammer recoil + speed loss,
  real-time hitstop, player-only camera shake + FOV punch + rumble, Niagara splash, `OnRamImpact` BlueprintImplementableEvent; `SpawnCollisionHandlingMethod = AlwaysSpawn`.
- `Source/LakeFight/Water/LFBuoyancyComponent.cpp`: pontoons (135, +-35) bow / (-135, +-55) stern, symmetric in X.
- `Source/LakeFight/Water/LFLakeSurface.cpp`: triangle winding flipped (now faces up).
- `Source/LakeFight/LakeFight.Build.cs`: + Niagara. `Config/DefaultEngine.ini`: `r.CustomDepth=3`.
- Assets: `M_LF_Lake` (Two Sided OFF; **Translucent** - see deviation; Opacity = hull water mask), `PM_LF_Hull` (Restitution 0.15),
  `BP_LF_Boat` (SplashSystem = NS_LF_Splash, AlwaysSpawn), new `FX/NS_LF_Splash` (3 emitters from engine templates: DirectionalBurst spray,
  OmnidirectionalBurst droplets, SimpleSpriteBurst "FoamRing" - **template defaults, not tuned**), `L_Lake01` (target boat re-placed, same transform).

**Deviation (needs your OK):** spec said "M_LF_Lake masked". UE refuses SceneTexture (custom depth/stencil) reads in opaque/masked materials
("except Single Layer Water", and this project uses Substrate). So the lake is **Translucent with opacity 1** (0 only where the mask clips). It looks opaque;
the trade-offs are no depth write (fine for now; matters if we add translucent FX under water or DOF) and translucency lighting instead of the deferred path.

**Mask check (screenshots, editor):** the cockpit shows the floorboards, not water, at roll 0 and at roll 15 deg (target boat temporarily rolled toward
the camera, then restored and saved). Screenshots: scratchpad `mask1.png`, `mask_roll15.png`.

**Collision check:** the physics body IS the UCX set of SM_LF_Rowboat (no box any more), so the outline matches the hull by construction. I could NOT
produce a collision-view screenshot or a rail-to-rail contact test through MCP (no console `show collision`, no way to drive or teleport PIE actors).
**Check by hand:** `show collision` in PIE, then push the two boats rail to rail.

**Commands / results**
- 2 builds (`Tools/ue.ps1 -Action build ... -EngineDir D:\UE_EpicGames\UE_5.8`), both Succeeded first try. I closed/reopened the editor myself
  (graceful CloseMainWindow after saving all; relaunched UnrealEditor.exe with the uproject).
- PIE #1 found 2 real bugs: the player pawn failed to spawn ("collision at spawn location") and the placed target boat had buoyancy disabled
  (LF warning). Fixed: AlwaysSpawn (BP + C++) and re-placed the target instance. Final PIE 07:07:01-07:08:11 UTC (70 s), **no warnings or errors of any category**:
  `[hull] BP_LF_Boat_C_0/1 mass=180 kg shapeCOM=(-4.39, 0, -1.99) -> COM=(0, 0, -15)`
  `[settle] BP_LF_Boat_C_0 (player) t=3.0s Z=-2.8 water=-0.7 (diff -2.1 cm) pitch=-0.81 roll=1.30 -> level`
  `[settle] BP_LF_Boat_C_1 (target) t=3.0s Z=4.6 water=5.1 (diff -0.5 cm) pitch=0.20 roll=-1.12 -> level`

**[hit] logging:** `LogLF: [hit] <rammer> -> <target> tier=bump|RAM closing=<cm/s> strength=<0..1>` for every boat contact with closing >= 30 cm/s
(once per pair per 0.3 s). **No contacts happened in my PIE runs** (I can't drive), so the tiers are unverified - ram the target a few times and grep the log for `[hit]`.

**New / changed tunables (defaults)**
- `LF|Hull`: HullMassKg 180, CenterOfMassLocal (0,0,-15), WaterMaskStencil 1, HullRestitution 0.15 (PM_LF_Hull 0.15).
- `LF|Hit`: MinContactSpeed 30, RamMinSpeed 150, RamFullSpeed 600, RamShove 250, TargetShoveShare 0.7, RammerRecoilShare 0.3, RammerKeepSpeed 0.25,
  RamHeave 120, RamRollKick 1.0 rad/s, RamPitchKick 0.5 rad/s, MaxHitImpulse 250, MaxHitRoll 1.2, HitCooldown 0.3, HitstopDilation 0.05,
  HitstopMinTime 0.06, HitstopMaxTime 0.10 (real s), ShakeDuration 0.25, ShakeAmplitude 14 cm, ShakeFrequency 22 Hz, FovPunchDeg 5, FovPunchTime 0.3,
  RumbleIntensity 0.8, RumbleDuration 0.2, SplashSystem NS_LF_Splash, bDebugSplash true (only used if SplashSystem is unset).
- Removed: MinHitSpeed, MaxHitSpeed, HitShoveScale, HitPopScale, HitRollScale, VisualMesh/Hull box, CenterOfMassOffset.

**Knobs for "bump too strong / ram too weak"**
1. Bump too strong -> it is pure physics now: lower `PM_LF_Hull` Restitution (0.15 -> 0.05) and/or raise `RamMinSpeed` (150 -> 200) so fewer contacts count as rams.
2. Ram too weak -> lower `RamFullSpeed` (600 -> 450) so typical rams reach full strength.
3. Ram too weak -> raise `RamShove` (250 -> 350) together with `MaxHitImpulse` (250 -> 350); `RamHeave` 120 -> 180 for more lift.

**Design notes:** the rammer = the boat moving faster into the contact; it resolves the pair once. The target shove is applied at the contact point's XY
but at the target's COM height (so off-centre hits spin it; roll comes only from the clamped RamRollKick, which keeps "never flips" enforceable).
Hitstop restores on FTSTicker (real time). Shake/FOV are applied in UpdateCamera, so they freeze during hitstop (reads as a freeze-frame).

**Follow-ups:** tune NS_LF_Splash (shape the foam ring, water colours, scale by `Strength` user param - not added yet); collision-view verification
by hand; the target boat placed in L_Lake01 had to be re-placed after the root-component change - future root changes need the same.

## T01.3 - real-time camera shake + stylized water (Claude Code, 13 Nov ~11:13-11:40 IST)

**Asked (director):** A. camera on rams: spring arm/lag + all feedback on real time; one smooth decaying ~12-15 Hz mostly-vertical
oscillation scaled by ram strength, restart not stack; per-pair ram cooldown 0.5 s with `[hit]` logs; FOV punch eases out over 0.3 s real time.
B. stylized M_LF_Lake (WPO formula + hull mask untouched): Surface ForwardShading, faceted normals, depth colour #4FD1C5 -> #1B6F8A at 2.5 m,
fresnel sky lift, 3-band toon light + hard specular, contact foam, crest foam, two panning Voronoi line layers fading with distance,
optional wake, everything on MI_LF_Lake. C. build, 60 s PIE, GPU time, 4 shots, commit.

**Files changed**
- `Source/LakeFight/Boat/LFBoat.h/.cpp`
  - Camera: the spring arm uses absolute location and its built-in lag is OFF. `UpdateCamera` gets `World->DeltaRealTimeSeconds`
    and does pivot position lag (`CameraLocationLagSpeed` 5), yaw/pitch lag and all feedback with frame-rate independent `1-exp(-k dt)`.
  - Shake: one decaying sine. `ShakeDuration` 0.3 s, `ShakeFrequency` 13 Hz, `ShakeAmplitude` 12 cm x ram strength (min 0.25),
    envelope (1-t)^2, starts at sin(0) (no pop). `ShakeHorizontalShare` 0.15 (half-frequency sideways). A new ram restarts it.
  - FOV punch: instant widen, cubic ease-out over `FovPunchTime` 0.3 real s, also restarts.
  - Hits: `HitCooldown` (per boat, game time) replaced by `PairHitCooldown` 0.5 s, per pair, REAL time. Every contact event of the
    pair refreshes it, so one sustained contact = one bump/ram. The pair is resolved once, from the rammer's side (`ResolveContact`).
    Log line: `[hit] A -> B tier=RAM|bump closing=.. strength=.. | t=<real s>, <s since last pair hit>, <n> contacts suppressed by 0.5 s pair cooldown`.
- `Source/LakeFight/Water/LFLakeSurface.h/.cpp`: now ticks (TG_PostPhysics, also in the editor viewport) and writes up to 4 boats
  into MPC_LF_Water `Boat0..3` = (X, Y, yaw rad, speed cm/s). That drives the boat foam ring and wake (see deviation 2).
- `Docs/T013_LF_StylizedWater.hlsl`: the shader source in M_LF_Lake's `LF_StylizedWater` Custom node (for review/diffs).
- Assets (MCP, saved): `M_LF_Lake` (new Custom node + 41 parameters; old base-colour lerp removed; BaseColor 0, Specular 0,
  Roughness 1, Emissive = toon result, TranslucencyLightingMode = Surface ForwardShading; WPO node, faceted-normal node and hull-mask
  opacity node untouched), new `MI_LF_Lake` (tuned values), `MPC_LF_Water` (+Boat0..3), `L_Lake01` (lake uses MI_LF_Lake; lake Size
  12000x8000 -> 14000x10000, see the bug below).
- `Docs/Shots/T013_chase.png` (PIE), `T013_topdown.png`, `T013_grazing.png`, `T013_dock.png` (editor viewport, game view).

**Deviations (please OK)**
1. Toon banding needs the lighting computed in the material, so engine lighting is neutralised (BaseColor 0, Specular 0) and the
   result goes to Emissive. It is lit by the atmosphere sun: `SkyAtmosphereLightDirection` + `SkyAtmosphereLightIlluminanceOnGround`
   x `Brightness`. Forward Shading is set as asked but contributes ~nothing. Side effect: no boat shadows on the water.
2. Contact foam around BOATS cannot come from depth fade. The T01.2 hull mask clips every water pixel behind the gunwale cap in
   screen space, and the hull flares, so the waterline is always under the cap. Boats therefore get a world-space foam ring around a
   capsule hull outline from the MPC boat slots. Depth-fade foam still does the shore and dock posts.
3. Wake (optional 11) is a straight foam V attached to the stern above `WakeStartSpeed` 200 cm/s. No path history (it swings with
   the boat when turning). Not seen moving: I can't drive in PIE.

**Changes from your mid-task feedback** ("foam looks solid"; "weird reflections / bottom of the lake artifacts")
- All foam (contact, boat ring, wake, crests) is now thresholded against fine 2-octave noise (`FoamFragScale` 10 cm,
  `FoamFragDensity`): flecks dense at the contact edge, sparser outward, with a solid line only `FoamSolidCore` 2 cm wide.
  Crest foam = flecks instead of cloud-like blobs.
- Found and fixed four artifact sources:
  - **Real lake-bottom bug (pre-existing):** the lake mesh was 120x80 m but the banks sit at +-70/+-50 m, leaving a 10 m strip of
    bare lake bed around the whole lake (the pale band at the shore + a hard edge). Lake Size is now 140x100 m.
  - **Pale columns under the dock posts:** depth was measured along the view ray. It is now converted to vertical depth (x|V.z|,
    clamped 0.25).
  - **Big hard white disc:** the sun glint is now sparkles (noise-masked) and fades with distance.
  - **Mint slab under each boat:** the depth colour measured the hull, not the lake. Within `BoatDeepMargin` 80 cm of a hull it is
    forced to the deep colour.
- **Still visible (needs you):** a mint/pale band right under each hull in the chase view. That's the hull's bottom paint, seen
  because the T01.2 hull mask clips the water in front of the hull (screen-space stencil), so boats look like they sit ON the water.
  You asked to keep the mask exactly as is, so it's untouched. Proposed fix: clip by world-space gunwale outline (the MPC boat
  capsule) instead of the screen-space stencil. Also: faint dashed rows near the far shore at grazing angles (from foam; reduced,
  not gone).

**Commands / results**
- 2 builds (`Tools/ue.ps1 -Action build ... -EngineDir D:\UE_EpicGames\UE_5.8`, editor closed/reopened by me): both **Succeeded** first try.
- Material compiled after every graph change (MCP `recompile` returned OK).
- GPU (`ProfileGPU` typed into the editor Cmd box during PIE, 1072x966 render res, TSR to 1350x1216):
  frame root 2.95-3.62 ms; **Translucency pass (= the lake) 0.12-0.13 ms**. **No "before" number:** I changed the material before
  I found a way to run console commands through MCP. The old material was a trivial lerp, so ~0.1 ms is the upper bound of the added cost.
- PIE: see the DirectorsLog line for the final run. `[settle]` both boats level; no LF warnings.
- `[hit]`: **not exercised** - I still can't drive or ram through MCP. Grep the Output Log for `[hit]` after ramming.

**You must check by hand:** ram a few times. The camera should never freeze-then-snap in the hitstop; one smooth vertical wobble
per ram; holding the boats together gives exactly one `[hit]` line; the FOV eases back. The look in motion (foam flecks drift, line
panning, wake above ~200 cm/s). The 4 shots in `Docs/Shots/`.

**First knobs (MI_LF_Lake)** `Brightness` 0.2 (overall exposure of the emissive water); `FoamFragDensity` 0.5 (lower = more foam);
`BoatFoamWidth` 35 cm; `LineOpacity` 0.035 / `LineWidth` 0.03; `FacetContrast` 10 (facet visibility); `DeepDepth` 250; `SpecThreshold` 0.93.

**Follow-ups:** hull mask in world space (above); AGENTS.md says Unreal MCP is on port 8000 but `.mcp.json` uses 9500; MCP still
has no console exec (I used the Slate inspector to type into the Cmd box - works, but fragile); boat shadows on water (deviation 1);
the wake needs a real trail (ribbon/decal history) if the straight V reads wrong.

**T01.3 review fixes (director videos `underwater graphics problem.mp4`, `line behind boat.mp4`)**
- "Jagged polygons moving underwater" = the toon facet bands: each 1 m wave triangle shaded lighter/darker (`FacetContrast` 10),
  moving with the waves. `FacetContrast` -> 0 on MI_LF_Lake: flat colour cells (confirmed by top-down before/after). Raise it again
  (2-3) for a hint of facets.
- "Two lines behind the boat" = my optional straight V-wake (rigid foam lines attached to the stern, swinging with the boat).
  Disabled: `WakeOpacity` -> 0. A proper wake needs a trail with history (Niagara ribbon / decals) - follow-up.
- Both are MI parameter changes only (no C++/material recompile). Updated `Docs/Shots/T013_topdown.png`.

**T01.3 review fixes, round 2 ("foam looks solid white"; "bottom of the boat visible translucently")**
- The pale/mint band under each hull was the WATER's shallow colour (proved by setting ShallowColor to red: the band turned red).
  The near-hull "force deep" override ramped linearly over 80 cm, so water hugging the hull was still half shallow. Now it is fully deep
  up to 75 % of `BoatDeepMargin`, then fades out. The hull now meets the water (chase shot updated).
- Hull water mask (director asked to hide the submerged hull, so the mask was changed): water behind the WaterMask cap is clipped
  only where the surface point lies inside a hull's gunwale outline in world XY (the RowboatPlan polygon x .958/.91, boat pos + yaw
  from MPC Boat0..3). Water outside the hull is never clipped, so it covers the submerged hull. Source: `Docs/T013_LF_HullMask.hlsl`.
  Limits: hull pitch/roll ignored (a few cm of leak at big rolls); assumes every ALFBoat uses the rowboat outline.
- Foam: outer flecks fade to `FoamOuterAlpha` 0.35 so water shows through and between them; `FoamFragDensity` 0.56 (more gaps).

**T01.3 review fixes, round 3 (`border lines.png`: hull bottom edges, far boat's bottom and shore-wall bottoms visible/flickering underwater)**
- Cause: the depth-based effects (shallow tint, contact foam) react to any submerged geometry. At low camera angles the water hiding a
  submerged hull stretches metres out (so the 80 cm "deep near hull" capsule could not cover it), and at depth discontinuities
  (hull bottom, shore-wall bottom, lake-bed edge) `fwidth` spiked and the soft foam threshold drew thin flickering lines.
- Fix 1 (C++): `ALFBoat::HullMesh` now renders custom depth with stencil `HullStencil` = 2. M_LF_Lake (param `HullStencil` 2) treats
  water in front of any hull as deep with no depth foam: exact at any angle and distance.
- Fix 2 (material): `aaF = clamp(fwidth(WDv), 0.5, 3)`: depth jumps no longer draw lines.
- Build: Succeeded (editor closed/reopened). PIE chase shot updated: no hull edges, no far-boat bottom, no far line.

**T01.3 review fixes, round 4 ("the ring around the boat feels like a white shadow under the minute foam")**
- Cause: boat foam was measured from a capsule ~30 cm wider than the pointed bow/stern; its solid-core fill covered the whole
  capsule-minus-hull area = a smooth white halo under the flecks.
- Fix: boat foam and the near-hull deep colour now use the signed distance to the real gunwale polygon (same 15-point RowboatPlan
  as the hull mask). Only flecks remain, hugging the hull with water between them. MI: `BoatFoamWidth` 60, `FoamFragDensity` 0.5.
  `BoatHalfLength`/`BoatRadius` are now unused (kept, harmless).
