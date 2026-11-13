# B09 — oars and rowboat oarlock sockets

Completed in live Blender **5.3.0** through serial Blender MCP calls. Original scripted geometry only; all changes under Art/.

## Oar

`SM_LF_Oar.fbx`: **216 triangles**, 2.10 m long, no UCX, one flat-shaded render mesh and one child Empty. Origin is the leather collar centre at (0,0,0), local +X toward the blade. Handle end x=−0.70 m; blade tip x=+1.40 m. Round octagonal grip and loom; narrow rounded blade spans x=0.95..1.40 m, maximum width 0.12 m. The thin blade lies in local XY, normal +Z. Dark brown leather collar spans x=−0.075..+0.075 m, with raised end seams. Materials: existing `M_LF_Wood`, original solid-colour `M_LF_Leather` (roughness 0.9). No textures or modifiers.

`SOCKET_Grip`: child of `SM_LF_Oar`, location **(−0.70,0,0) m**, identity rotation and scale, exactly at the handle end.

## Rowboats

All four variants re-exported with two Empty children of their render mesh:

| FBX helper | Local position (m) | Rotation / scale |
|---|---|---|
| SOCKET_Oarlock_L | (−0.12, −0.672, 0.33) | identity / (1,1,1) |
| SOCKET_Oarlock_R | (−0.12, +0.672, 0.33) | identity / (1,1,1) |

These are the **pin tips**: the existing pins are centred at Z=0.30 m with height 0.06 m. Both socket +Z axes point up; +X follows the boat bow. L corresponds to the script's Port/negative-Y oarlock. Each FBX contains one render mesh, the original twelve UCX meshes and exactly two sockets. Socket names in every FBX are exact; Blender's live scene gives earlier variants qualified helper names to avoid global name collisions during sequential exports.

Archived pre-B09 FBXs were re-imported and compared against the new exports. **Every render mesh and UCX piece matches** in vertices/faces, per-face material assignments, material slot names, triangle count, origin and scale at 0.1 mm coordinate precision. No boat render or UCX edits were made. Boat script changes are its project-relative import path and adding the socket empties after existing mesh checks. All FBXs use metres, FBX All scale, forward −X, up Z.

## Commands, runs and evidence

Read the rowboat script, B06 helper and current task. Inspected the live Blender scene. Used PowerShell to archive existing exports and apply_patch to write scripts. No Unreal calls or C++ changes.

Executed through `execute_blender_code`:

```python
v = runpy.run_path(r'C:\Unreal\Projects\LakeFight_MegaJam\Art\BlenderScripts\B09_Review.py')
v['before']()   # archived FBX baselines and before screenshots
v['assets']()   # runpy.run_path(SM_LF_Oar.py), SM_LF_Rowboat.py, each twice
v['after']()    # actual FBX re-import assertions, posed screenshots and saved review
```

The full workflow and review/audit functions were repeated during verification. Final asset-script runs match canonical geometry/material signatures and socket reports; polygon ordering from Blender bmesh is excluded from equivalence. FBX byte identity is not claimed because the format includes export timestamps. Oar exported geometry also matches its generated source. Assertions verify the grip and all eight boat sockets' names, parents, positions, rotations and scales. `B09_runs.json`, `B09_verified.json`, `B09_before.json` and `B09_execution.log` hold the evidence.

Screenshots, inspected using MCP `look`:

- `_shots/B09_before_top.png`, `B09_before_side.png`: archived rowboat before sockets/oars.
- `_shots/B09_oars_top.png`, `B09_oars_side.png`: re-imported red rowboat with both oars parented at the exported sockets, collar origins at local zero. Mid-stroke staging: yaw ±108°, pitch 16°, blade roll 90°; no posed oars were added to rowboat FBXs. The top view sees the working blades edge-on; the side view shows blade faces.

`B09_Review.blend` saves the posed review scene and build scenes. The screenshots use the actual exported/re-imported meshes. `git diff --check` is run before commit.

## Files changed

- Modified `BlenderScripts/SM_LF_Rowboat.py`; re-exported `Export/SM_LF_Rowboat.fbx`, `_Blue.fbx`, `_Green.fbx`, `_Yellow.fbx`.
- Added `BlenderScripts/SM_LF_Oar.py`, `B09_Review.py`, `Export/SM_LF_Oar.fbx`.
- Added `Export/_before_B09/`: original rowboat script and all four original FBXs.
- Added four `Export/_shots/B09_*.png`, `Export/B09_Review.blend`, the four evidence/log files above, and this report.

## Human checks / stop

Please judge oar proportions and mid-stroke fit in the top/side screenshots. After the director's Unreal import, verify `Grip`, `Oarlock_L` and `Oarlock_R` appear as runtime sockets and animate the oars around the collars. No engine import or animation was performed here. Art-only scope kept; Docs logs were not edited. Director-log line for later: “B09 asked: 2.1 m oar and oarlock sockets on four boats / done: 216-tri oar, socket-only boat updates, twice-run and FBX invariance checks, posed top/side evidence / kept or changed: awaiting director review.”

Stopped at B09's screenshot/report boundary.
