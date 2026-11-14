# B10 - beyond the shore

Delivered on 14 November 2026 in Blender 5.1.0, before the director's approximately 11:20 IST L01 layout session. Ten environment FBXs were delivered first; icon corrections and screenshots followed. All task changes are under Art/. The director's B10 message supplied the scope because Docs/CurrentTask.md still describes prior code tasks.

| Asset | Triangles / exclusive budget | Dimensions in metres |
| --- | --- | --- |
| SM_LF_GroundPatch_01 | 288 / <800 | 40 x 40 x 1.115 |
| SM_LF_GroundPatch_02 | 288 / <800 | 40 x 40 x 0.978 |
| SM_LF_Hill_Far_01 | 94 / <300 | 130 x 105 x 24 |
| SM_LF_Hill_Far_02 | 94 / <300 | 170 x 120 x 33 |
| SM_LF_Hill_Far_03 | 94 / <300 | 195 x 145 x 39 |
| SM_LF_Bush_01 | 140 / <300 | 1.426 x 1.010 x 1.25 |
| SM_LF_Bush_02 | 144 / <300 | 1.422 x 1.070 x 1.30 |
| SM_LF_Log | 136 / <200 | 2.64 x 0.510 x 0.724 |
| SM_LF_Fence_Section | 140 / <200 | 2.054 x 0.207 x 1.254 |
| SM_LF_TreeCluster_01 | 942 / <3000 | 14.565 x 10.827 x 7.406 |

Ground patches have broad faceted bumps and Moss/Reed grass patches. Each has 48 open boundary edges whose vertices are exactly Z=0; tile footprints and boundary vertex spacing match. These are intentionally single-sided ground surfaces, with no underside. The hills have closed bases, modest asymmetric shoulders and the shared palette's cooler desaturated Cloth green. Bushes are round overlapping solid crowns. The log has exposed end grain and a snapped branch. The fence has two leaning posts, two rails, a diagonal brace and small metal nail heads. The cluster is one mesh containing three pines and four round trees; a seventh centre tree fills the gap seen in the first review.

Every environment asset is one flat-shaded mesh, with base-centre origin (0,0,0), identity scale, no unapplied modifiers, no textures, no team paint and no UCX meshes. Shared materials only: M_LF_Moss, M_LF_Reed, M_LF_Cloth, M_LF_Wood, M_LF_WoodDark, M_LF_Rope and M_LF_Metal as appropriate. No palette changes or external assets. FBX uses 1 Blender unit = 1 metre, Apply Scale = FBX All, forward -X, up Z.

## Icon fixes

- ICON_LF_Paddle: horizontal, longer rounded oval blade, thin shaft and T grip. Uses the lighter existing Rope wood colour. 148 triangles in the optional icon-stage FBX.
- ICON_LF_FishGold: thin pale PaintCream rim arc plus three small four-point sparkle stars. Existing bonus-fish PaintYellow/Orange colours retained under the established gold-fish palette exception. 432 triangles in the optional icon-stage FBX. PaintCream supplies the palette's near-white highlight rather than adding a new white material.
- Both corrected UI PNGs are 256 x 256 RGBA with transparent backgrounds, matching orthographic lighting and unclipped margins. The other six icon PNGs were read for the contact sheet and left unchanged.
- UI/B10_ContactSheet.png: top row Fish / FishGold / Clock / Paddle; bottom row Red / Blue / Green / Yellow. Checkerboard appears only in the sheet.

## Files changed

- BlenderScripts/lf_b10.py: new original geometry and mesh/export checks, using lf_common's existing palette and export helper.
- BlenderScripts/SM_LF_GroundPatch_01.py, SM_LF_GroundPatch_02.py, SM_LF_Hill_Far_01.py, SM_LF_Hill_Far_02.py, SM_LF_Hill_Far_03.py, SM_LF_Bush_01.py, SM_LF_Bush_02.py, SM_LF_Log.py, SM_LF_Fence_Section.py, SM_LF_TreeCluster_01.py: one idempotent entrypoint per mesh, each clearing its own named collection.
- BlenderScripts/ICON_LF_Paddle.py and ICON_LF_FishGold.py: updated to use B10 geometry/rendering. B08/B09 scripts were read for context but never executed on this machine.
- BlenderScripts/B10_Validate.py and B10_Review.py: serial repeat-run/FBX audit, PNG checks, contact sheet, separate review scenes and rendering.
- Export/: ten new environment FBXs, two corrected icon-stage FBXs, B10_Review.blend, B10_runs.json, B10_mesh_runs.json, B10_mesh_verified.json, B10_verified.json, B10_UI_verified.json, B10_execution.log and this report.
- Export/_reports/*_B10.json: twelve per-asset mesh reports.
- Export/UI/: the two corrected icon PNGs and B10_ContactSheet.png.
- Export/_shots/: B10_ground_tiles.png, B10_distant_hills.png, B10_shore_props.png, B10_tree_cluster.png.

## Commands and results

Read AGENTS.md, Docs/CurrentTask.md, existing in-repo palette/scripts and output conventions. Used PowerShell Get-Content/Get-ChildItem, rg, git status and apply_patch to inspect/create source files. All geometry generation, FBX export/reimport, image rendering and contact-sheet composition ran inside the live Blender through serial MCP execute_blender_code calls. No C++ changes or Unreal operations were performed by this task; a C++ rebuild is therefore not applicable. Other concurrent Source/ changes belong to another task and were not staged here.

Final asset checks:

```python
v = runpy.run_path(r'D:/Projects/LakeFight/Art/BlenderScripts/B10_Validate.py')
v['run_assets'](v['b'].MESHES)  # each entrypoint twice
v['audit'](v['b'].MESHES)       # meshes available before rendering
v['run_assets'](v['b'].ICONS)   # each corrected icon twice, including PNGs
v['contact_sheet']()
# After adding the seventh centre tree, its entrypoint was rerun twice.
v['run_assets'](['SM_LF_TreeCluster_01'])
v['audit'](v['b'].MESHES)
v['audit'](v['b'].MESHES + v['b'].ICONS)
r = runpy.run_path(r'D:/Projects/LakeFight/Art/BlenderScripts/B10_Review.py')
r['screenshots']()
```

B10_runs.json records identical final geometry hashes, vertices/faces/material assignments, dimensions, origins and triangle counts across two runs of every entrypoint. Both revised icons additionally have identical PNG SHA256 values across their two renders. B10_verified.json records actual FBX reimport comparisons for all twelve outputs: geometry and per-face material assignments match at 0.1 mm precision; dimensions, triangle counts, zero origins, identity scales, one mesh and flat shading all pass. Binary FBX equality is not claimed because FBX embeds timestamps. B10_UI_verified.json records all eight UI inputs passing size, RGBA, transparency and unclipped-margin checks. Early development runs remain in the execution log; the final JSON reports supersede them.

All four final mesh screenshots and the updated UI contact sheet were inspected with Blender MCP look. B10_Review.blend preserves separate review scenes and original default scene; review placements do not change FBX origins.

Git handoff: seven commits, one for each numbered item, using git add, git diff --cached --check and git commit --only with explicit Art/ path lists. All seven staged whitespace checks passed after removing an extra blank line at EOF in B10_Validate.py. Git LFS remained enabled for FBX, PNG and blend artifacts. A sandboxed whole-Art diff initially failed in Git LFS's shell helper; the commit/check commands succeeded with elevated execution. No automatic approval rejection occurred.

## Human checks / stop boundary

During L01 layout, check the ground joins and silhouettes from the actual game camera, hill colour separation against the lake/sky, forest density when repeating the seven-tree cluster, and icon readability at real HUD scale. Visual approval remains with the director. Ground uses open single-sided surfaces; import collision policy is for the layout task. No Unreal import or PIE visual acceptance is claimed.

Docs logs are intentionally untouched under Art-only scope. Director-log line for later: "B10 asked: ten beyond-shore meshes and two UI fixes / done: original Blender 5.1 scripts, twice-run exports, FBX audits, screenshots and updated contact sheet / kept or changed: awaiting director review." No human design decisions were invented.

Follow-up: Blender MCP reports addon protocol 15 versus server expectation 16; the required execution and look calls worked. Updating the addon is outside B10. Stopped after assets, evidence and report.
