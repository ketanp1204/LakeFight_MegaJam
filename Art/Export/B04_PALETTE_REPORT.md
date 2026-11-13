# Palette fix before B04

Changed only the four team RGB values in lf_common.py, plus refreshing shader/viewport colours when lf.mat() retrieves an existing material. This refresh is required for live Blender re-exports to receive the updated palette. Other palette values, rowboat/flag scripts and geometry are unchanged.

Values are linear shader RGB; sRGB 0-255 is the standard transfer conversion, rounded to the nearest integer. These are source values, not sampled pixels or a claim about Unreal exposure/lighting.

| Colour | Before linear | Before sRGB | After linear | After sRGB |
|---|---|---|---|---|
| PaintRed | (0.75, 0.18, 0.14) | (225, 118, 105) | (0.6, 0.05, 0.04) | (203, 63, 56) |
| PaintBlue | (0.16, 0.36, 0.7) | (111, 162, 218) | (0.035, 0.17, 0.65) | (53, 115, 211) |
| PaintGreen | (0.2, 0.55, 0.25) | (124, 196, 137) | (0.035, 0.38, 0.055) | (53, 166, 66) |
| PaintYellow | (0.9, 0.72, 0.15) | (243, 221, 108) | (0.9, 0.6, 0.025) | (243, 203, 44) |

The director supplied exact red. Blue/green/yellow were also desaturated in the original source sRGB values; their off-hue channels were lowered to make clearly distinct blue, green and yellow families. Cream/black/wood values were kept.

| Re-export | Tris before -> after | Geometry / bounds |
|---|---:|---|
| SM_LF_Rowboat | 1286 -> 1286 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_Rowboat_Blue | 1286 -> 1286 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_Rowboat_Green | 1286 -> 1286 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_Rowboat_Yellow | 1286 -> 1286 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_TeamFlag_Red | 216 -> 216 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_TeamFlag_Blue | 216 -> 216 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_TeamFlag_Green | 216 -> 216 | Unchanged; original B02.1 geometry hash matches |
| SM_LF_TeamFlag_Yellow | 216 -> 216 | Unchanged; original B02.1 geometry hash matches |

The existing SM_LF_Rowboat.py and SM_LF_TeamFlag.py each ran twice and each produces four variants. Their two reports matched exactly. Rowboat totals include 1,214 render + 72 UCX triangles; flags are 216 triangles per asset. Source scripts were not edited.

All eight re-exported FBXs were imported into Blender and their actual shader RGB values checked against the new palette. Bounds/counts match the initial FBX measurements. B04_Validate.py subsequently repeated all twenty FBX checks twice.

Evidence: B04_palette_before.json, B04_palette_fix.json, B04_palette_execution.log, B04_fbx_audit.json. Preview: [_shots/B04_TeamPalette_BoatsFlags.png](_shots/B04_TeamPalette_BoatsFlags.png).

Palette exports were completed and verified before B04 geometry generation started. Actual game-lighting colour acceptance remains a human check; no Unreal MCP calls or uasset edits were made.
