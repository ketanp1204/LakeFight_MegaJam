# B01 - Rowboat model (Thu 12 Nov, parallel with T01) - agent: Codex (owns the Blender connection)

## Goal
`Art/BlenderScripts/SM_LF_Rowboat.py` builds a chunky, readable low-poly wooden rowboat and exports
`Art/Export/SM_LF_Rowboat.fbx`.

## Spec
- Uses `lf_common.py` (fresh_collection, mat, export_fbx). Idempotent: running twice gives the same result.
- Size ~3.2 m long, ~1.4 m wide, hull ~0.6 m deep. Origin at waterline centre (waterline ~0.25 m above keel).
- Parts: hull (slightly exaggerated sheer/upturned bow), 2 thwart seats, inner floor, gunwale trim, 2 oarlocks,
  small bow ring. Separate object `Stripe` along the hull in PaintRed (so a second variant can recolour it).
- Materials: Wood, WoodDark (trim/seats), PaintRed (stripe), Metal (oarlocks). Flat shaded.
- Under 3k triangles. Hull must be closed (no holes) so collision generation works.
- Also export `SM_LF_Rowboat_Blue` from the same script with PaintBlue stripe (rival boats).

## Acceptance
- [ ] Script runs clean twice in a row; FBX files exist; printed tri count < 3000.
- [ ] Viewport screenshot (3/4 view) attached to the report.
- [ ] Human approves silhouette: reads as "rowboat" at 30 m distance.

## Stop boundary
Stop after both FBX files exist and the screenshot is in the report.
