# B02.1 approved changes

All 17 changed/new exports completed; no assets skipped. Every affected asset script passed two consecutive file executions with matching complete reports and geometry/material hashes.

| Asset | Tris | Status |
| --- | ---: | --- |
| SM_LF_Rowboat | 1214 + 72 UCX = 1286 | Two runs match; FBX + PNG verified |
| SM_LF_Rowboat_Blue | 1214 + 72 UCX = 1286 | Two runs match; FBX + PNG verified |
| SM_LF_Rowboat_Green | 1214 + 72 UCX = 1286 | Two runs match; FBX + PNG verified |
| SM_LF_Rowboat_Yellow | 1214 + 72 UCX = 1286 | Two runs match; FBX + PNG verified |
| SM_LF_Shack | 1188 | Two runs match; FBX + PNG verified |
| SM_LF_Buoy | 236 | Two runs match; FBX + PNG verified |
| SM_LF_Fish_A | 180 | Two runs match; FBX + PNG verified |
| SM_LF_Fish_B | 180 | Two runs match; FBX + PNG verified |
| SM_LF_Fish_Gold | 180 | Two runs match; FBX + PNG verified |
| SM_LF_Paddle | 156 | Two runs match; FBX + PNG verified |
| SM_LF_Reeds | 356 | Two runs match; FBX + PNG verified |
| SM_LF_ReedBed | 2988 | Two runs match; FBX + PNG verified |
| SM_LF_Rock_03 | 68 | Two runs match; FBX + PNG verified |
| SM_LF_TeamFlag_Red | 216 | Two runs match; FBX + PNG verified |
| SM_LF_TeamFlag_Blue | 216 | Two runs match; FBX + PNG verified |
| SM_LF_TeamFlag_Green | 216 | Two runs match; FBX + PNG verified |
| SM_LF_TeamFlag_Yellow | 216 | Two runs match; FBX + PNG verified |

## Palette diff

Modified Art/BlenderScripts/lf_common.py; added only these RGB entries:

| Material suffix | RGB |
| --- | --- |
| PaintGreen | (0.2, 0.55, 0.25) |
| PaintYellow | (0.9, 0.72, 0.15) |
| Black | (0.03, 0.03, 0.03) |
| PaintOrange | (0.95, 0.45, 0.1) |
| Moss | (0.33, 0.42, 0.22) |

Existing palette values are unchanged. Added TEAM_COLOURS to document the reservation; shader lookup now finds the Principled node by type.
Red, Blue, Green, Yellow are reserved for boats, stripes and team flags; Fish_Gold is the explicit Yellow exception. Final audit passed across all SM_LF collections.
Shack roof is Moss; buoy is Orange/Cream; Fish_B uses Metal/Cream and Orange fins. Eyes are eight-sided Black domes with thin Cream rims.

## Geometry changes

- Rowboats: four variants; unchanged approved silhouette and six empty-material UCX cuboids per variant.
- Paddle: shaft diameter 4.0-4.2 cm, blade 0.377 m wide (1.3x), thickness tapered 2.8 to 2.2 cm, length 1.6 m, grip origin.
- Reeds: original clump leaves have 1 cm solid thickness. ReedBed has 30 reeds with 180 broad solid leaves, envelope 4 x 3 m, stem tops 1.2-1.8 m; 2,988 triangles.
- Rock_03: planar slab top, no dished cap.
- TeamFlag: four variants, two objects per FBX (pole and <asset>_Flag); 2.58 m pole, waved solid team field with Cream hoist.

## Files and commands

Modified: lf_common.py, lf_b02.py, SM_LF_Rowboat.py, SM_LF_Shack.py, SM_LF_Buoy.py, SM_LF_Fish.py, SM_LF_Paddle.py, SM_LF_Reeds.py, SM_LF_Rock_03.py.
Added asset scripts: SM_LF_ReedBed.py and SM_LF_TeamFlag.py. All script paths are under Art/BlenderScripts.
Wrote 17 FBX exports, 17 asset PNGs, two extra reed-bed side/boat-preview PNGs, 17 per-asset JSON reports, B021_manifest.json and this report under Art/Export.
Executed each saved asset file through Blender MCP using exec(compile(open(path).read(), filename, 'exec')), twice consecutively. Captured solid-shaded material-colour viewport images, ran hull/window/convexity checks where applicable, and checked exported files on disk.
Only Art was written. No git, Unreal MCP or external asset tools were used.

## Human review

- Approve fish eyes, widened paddle and reed-bed silhouette.
- Reed bed: 30 reeds, 4 x 3 x 1.8 m envelope, 637/671 ray samples blocked in a 3 m wide, Z=0.10..0.55 m window. This is a geometry probe; gameplay concealment remains a human check.
- HidingBoat screenshot is a Blender preview with a red boat behind the bed, waterline 0.25 m above bed base. Boat transforms were restored.
- Unreal import, simple collision and gameplay remain with the director/Unreal agent.

Screenshots: Art/Export/_shots/<asset>.png; extra previews SM_LF_ReedBed_Side.png and SM_LF_ReedBed_HidingBoat.png.
