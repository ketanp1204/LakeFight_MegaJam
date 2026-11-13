# B08 — T04 game-round assets

Blender MCP connected successfully: Blender **5.3.0**, add-on 1.8 / protocol 15 (server expects 16). All execution, rendering and FBX re-import checks succeeded through the existing connection. No add-on update performed.

## Delivered

| Mesh | Triangles | Dimensions (m) | Materials |
|---|---:|---|---|
| SM_LF_FishSpot | 200 / <400 | 2.46965 × 2.23138 × 0.116 | M_LF_PaintCream |
| SM_LF_BankZone | 240 / <600 | 4.97466 × 2.96415 × 0.15 | M_LF_Rope, M_LF_PaintOrange |

FishSpot: ten flattened foam blobs, slight size/height variation, origin at the waterline centre (0,0,0); Z extent −0.029 to +0.087 m. No animation authored. BankZone: four thin rope spans, eight orange floats, open centre, base at Z=0. Both exports contain exactly one flat-shaded mesh, identity scale, no modifiers and no UCX. Metres; FBX Apply Scale = FBX All, forward −X, up Z.

Eight icons in `UI/`: `ICON_LF_Fish.png`, `ICON_LF_FishGold.png`, `ICON_LF_Clock.png`, `ICON_LF_Paddle.png`, `ICON_LF_Crew_Red.png`, `ICON_LF_Crew_Blue.png`, `ICON_LF_Crew_Green.png`, `ICON_LF_Crew_Yellow.png`. Each is 256×256, 8-bit RGBA, transparent, orthographic, flat-shaded geometry with the same two softbox lights, world illumination and Standard colour transform. Cream sphere proxy heads and team vests/hats; Blue has a cream captain crown, Blue band, black peak and beard; Green has a moustache. Red and Yellow wear plain team bucket hats. Original geometry, no third-party assets or textures. Icon-stage FBXs are also included as script outputs; the requested UI deliverables are the PNGs.

`UI/B08_ContactSheet.png`: top row Fish / FishGold / Clock / Paddle; bottom row Red / Blue / Green / Yellow. Checkerboard is baked into the contact sheet only, not the individual icons.

## Files changed (all new, under Art/)

- `BlenderScripts/SM_LF_FishSpot.py`, `SM_LF_BankZone.py` and eight `ICON_LF_*.py` entrypoints.
- `BlenderScripts/lf_b08.py`: geometry, explicit deterministic merge, palette reuse, FBX export and orthographic stage.
- `BlenderScripts/B08_Validate.py`: repeat-run checks, FBX round trips, PNG assertions and contact-sheet generation.
- `Export/SM_LF_FishSpot.fbx`, `SM_LF_BankZone.fbx` and eight `ICON_LF_*.fbx`.
- `Export/UI/`: eight transparent icon PNGs and contact sheet.
- `Export/_shots/B08_before.png`, `SM_LF_FishSpot_B08.png`, `SM_LF_BankZone_B08.png`.
- `Export/B08_before.json`, `B08_runs.json`, `B08_verified.json`, `B08_execution.log`, `B08_Review.blend`, this report.

## Commands and evidence

Read `AGENTS.md`, `Docs/CurrentTask.md`, palette and existing in-project script conventions. `CurrentTask.md` still describes T03.1; the director's explicit B08 message supplied this task's scope. Ran `git status --short`, `rg --files Art`; created files with apply_patch / PowerShell. Local `python` was unavailable; all geometry, renders, image composition and validation ran in Blender Python via serial MCP `execute_blender_code` calls.

Final MCP execution:

```python
v = runpy.run_path(r'C:\Unreal\Projects\LakeFight_MegaJam\Art\BlenderScripts\B08_Validate.py')
v['run_assets'](v['NAMES'])  # runpy.run_path for each of ten asset entrypoints, twice
v['finish']()
v['finish']()
```

`B08_before.json` records the default cube/camera/light scene and absence of every target export. `B08_before.png` renders that scene. The two mesh after renders and icon contact sheet were inspected through MCP `look`.

`B08_runs.json` records both final runs per asset: identical vertices, faces, material assignments, dimensions, origin, triangle count and PNG SHA256. Earlier development checks caught Blender PNG timestamp metadata and unstable primitive polygon ordering; final scripts remove non-pixel PNG text chunks and explicitly sort polygons. FBX binary files embed timestamps; byte-for-byte FBX identity is not claimed.

`B08_verified.json` records actual exported-FBX re-import checks for all ten assets: geometry including per-face materials at 0.1 mm precision, triangle counts, dimensions, zero origins, and no UCX. Eight PNG checks passed size, RGBA channels, fully transparent background, opaque foreground and unclipped margins. Final validation/contact-sheet creation passed twice. `B08_execution.log` records the final full run.

`B08_Review.blend` preserves the original default scene and adds B08_Review with a spaced review layout. Layout offsets exist only in this review scene; exported asset origins are zero.

## Human checks / stop boundary

Please judge the marker silhouette/readability on the lake and dock, and the icon/crew readability at actual HUD size. Visual approval and PIE feel remain the director's decision. No Unreal imports, gameplay edits, animation or C++ builds were performed. Docs logs were not edited because this task explicitly restricts changes to Art/. Director-log entry for later: “B08 asked: two round markers + eight UI icons / done: original Blender scripts, exports, twice-run checks and evidence / kept or changed: awaiting director review.”

Follow-up: the Blender add-on reports an older protocol; updating it is outside B08. Stopped after assets, report and contact sheet.
