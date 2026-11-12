# {{PROJECT}} - agent rules (Epic AI MegaJam 2026)

Lake Fight: absurd physical-comedy boat brawls on a small fishing lake. Paddle fights, swimming, boarding,
stealing boats. Stylized low-poly. The human is the director; you execute one bounded task at a time.

## The task
- Work only from `Docs/CurrentTask.md`. Do not start anything it does not list.
- Stop at the task's stop boundary and report. Nearby problems go under Follow-ups, not into the diff.
- Report = files changed, commands run, results, what the human must check by hand. No claims without evidence.
  "It compiles" is not "it feels good" - feel is accepted only by the human in PIE.

## Jam rules you must protect
- Everything in this repo is made on or after 12 Nov 2026 05:00 UTC. Never copy code, prefabs, scripts, models or
  textures from any other project on this machine (there is an older Unity "Lake Fight" - do not open it).
- Original content only for models: every mesh comes from a script in `Art/BlenderScripts/` (see Blender below).
- Third-party assets (Fab, audio libraries) only if the human says they are licensed; log each in `Docs/ASSETS.md`.
- The packaged game must run without extra software, single-player against AI. Split-screen is an extra.

## Unreal conventions (UE 5.8, C++ + Blueprint)
- Prefix `LF` for C++ classes (`ALFBoat`, `ULFBuoyancyComponent`). Content under `/Game/LakeFight/...`.
- Gameplay logic and tunables in C++ (`UPROPERTY(EditDefaultsOnly, Category="LF|...")`); Blueprints for wiring,
  look and tuning. Components over deep inheritance.
- Water is OURS, not the Water plugin: a flat lake plane + `LF::Water::GetHeightAt(Pos, Time)` (a few summed
  sine/Gerstner waves) used by BOTH the material (same constants via a Material Parameter Collection) and the
  buoyancy component. Deterministic, cheap, no plugin.
- Physics: Chaos rigid bodies. Boats are physics-simulated pawns; buoyancy = per-pontoon spring + damping toward the
  water height; drag split into forward/lateral/angular so boats coast but don't slide sideways.
- Comedy over realism: knockback, splashes, wobble, ragdoll-lite falls. Every hit gets a sound + splash + camera kick.
- Enhanced Input; keyboard/mouse AND gamepad from day one (split-screen needs it).
- After C++ changes run `Tools/ue.ps1 -Action build -Project <uproject>` with the editor closed, or ask the human to
  trigger Live Coding. Never leave the project in a state that doesn't compile.

## Unreal MCP (editor must be open)
- Endpoint `http://127.0.0.1:8000/mcp`. Calls run serially on the game thread: never overlap calls.
- Use MCP for placing actors, setting properties, importing meshes, inspecting levels. C++ goes through files.
- Save levels/assets after MCP edits and list what you saved.

## Blender MCP (Blender 5.1, port 9876)
- Only ONE MCP client may hold the Blender connection. If Codex is connected to Blender, Claude Code must not be,
  and vice versa. The human decides which agent owns Blender for a task.
- Every asset = one idempotent Python script `Art/BlenderScripts/<asset>.py`, committed, run through
  `execute_blender_code`. The script clears its own collection, builds the mesh, applies modifiers, sets the
  origin, assigns materials by name, and exports FBX to `Art/Export/<asset>.fbx`. Re-running it gives the same file.
- Scale: 1 Blender unit = 1 m; export with Apply Scale = FBX All, forward -X, up Z. Origin at the waterline centre for
  boats, at the base for props. Keep boats under 3k tris, props under 1k.
- Style: flat-shaded low-poly, solid colours via a small shared palette (material names `M_LF_<Colour>`), no textures
  unless the task says so.
- Do NOT use Sketchfab, Poly Haven, Hyper3D or Hunyuan3D tools (they are denied in settings).
- Import into Unreal with `Tools/import_fbx.py` (run in the editor via MCP or the Python console).

## Logs you keep
- `Docs/DirectorsLog.md`: one line per task: asked / done / kept, changed or cut by the human. This feeds the
  "Director's Cut" write-up - be honest about rejected work.
- `Docs/DecisionLog.md`: only decisions the human made.
