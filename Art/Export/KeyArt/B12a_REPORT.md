# B12a - title lockups for key art

Rendered in Blender 5.1.0 through serial Blender MCP calls on 14 November 2026.
The director's B12a brief supplies this Art-only scope; CurrentTask still describes
Unreal work. Deadline: commit finished work before 16:35 IST (11:05 UTC).

## Deliverables

All files are in `Art/Export/KeyArt/`. Each transparent overlay contains both the
existing LAKE FIGHT logo and the separate DIRECTOR'S CUT ribbon, at their original
relative transforms. Canvas sizes are exact, not cropped to the title bounds.

| Size | Light outline | Dark outline | Hero layout guide |
| --- | --- | --- | --- |
| 630 x 500 | `LF_TitleLockup_630x500_Light.png` | `LF_TitleLockup_630x500_DarkOutline.png` | `LF_KeyArt_Layout_630x500.png` |
| 960 x 300 | `LF_TitleLockup_960x300_Light.png` | `LF_TitleLockup_960x300_DarkOutline.png` | `LF_KeyArt_Layout_960x300.png` |
| 1920 x 1080 | `LF_TitleLockup_1920x1080_Light.png` | `LF_TitleLockup_1920x1080_DarkOutline.png` | `LF_KeyArt_Layout_1920x1080.png` |

Light uses a cream silhouette stroke for dark/busy backgrounds. DarkOutline uses
a deep blue/charcoal stroke for light backgrounds. Both retain the original wood,
cream lettering and red ribbon. Outlines include the diagonal paddle silhouette.
Use whichever reads better over the final hero shot.

## Placement / use

Place the overlay at (0, 0) over a hero image with the same canvas dimensions.
The cover and 16:9 title sit above the action; the wide banner title sits left of
the action. The hero image/background can fill the entire canvas. These are title
overlays and guides; a gameplay hero shot is not included.

The three opaque guide PNGs show the actual light lockup and an orange-bordered
hero/action safe area. Keep faces and the boat brawl inside the box. They are
layout templates, not finished promotional images; do not publish the guides.

| Canvas | Hero box: x, y, width, height (pixels from top-left) |
| --- | --- |
| 630 x 500 | 24, 238, 582, 238 |
| 960 x 300 | 478, 18, 464, 264 |
| 1920 x 1080 | 96, 460, 1728, 566 |

## Source / reproducibility

New script: `Art/BlenderScripts/B12a_TitleLockups.py`. It imports the existing
`SM_LF_Logo_LakeFight.fbx` and `SM_LF_Logo_DirectorsCut.fbx` into its own Blender
scene, renders with an orthographic camera and transparent film, and produces
the outline variants with deterministic NumPy alpha dilation inside Blender.
The original lettering is scripted B05 geometry; no new font or external art
is used for the title. Template instructions use Blender's built-in font.

Command run through `execute_blender_code`:

```python
import runpy
b12 = runpy.run_path(r'D:/Projects/LakeFight/Art/BlenderScripts/B12a_TitleLockups.py')
report = b12['build']()
```

The script rebuilds only its own scenes and removes temporary base PNGs after
writing both variants. No new model/FBX is needed for this render-only task.
The original FBXs are read-only inputs; their hashes match before and after.

## Checks / results

- Six overlays: exact requested dimensions, PNG 8-bit RGBA, fully transparent
  background/hero area, opaque foreground, no clipped title or outline.
- Three guides: correct dimensions and fully opaque.
- Repeated full renders produced identical SHA256 hashes for all six overlays.
- `B12a_verified.json` records source/output hashes, alpha ranges, occupied
  title bounds and guide rectangles. PNG timestamp/text metadata is stripped.
- Inspected the light/dark lettering, ribbon readability and guide layout.
  The first guides had redundant title labels partly behind the logo; those
  labels were removed before delivery.
- One repeat command initially omitted its `runpy` import; rerun with an explicit
  import succeeded. No output loss or scope change.
- No Unreal calls, builds, gameplay edits, original asset edits or Docs writes.

Final approval of title size/contrast over the chosen hero shot remains a visual
check for the director; this task stops at the committed lockups/templates.
