# B02 - Lake prop batch (unattended while away, Thu 13:00 onwards) - agent: Codex (Blender only)

Bounded, unattended-safe: Blender scripts and FBX exports only. No Unreal, no C++, no git push.

## Assets (one script each in Art/BlenderScripts/, export to Art/Export/)
1. `SM_LF_Paddle` - 1.6 m wooden paddle, chunky blade. Origin at grip. < 300 tris.
2. `SM_LF_Dock` - modular dock section 4 x 2 m, planks + 4 posts. Origin at top-surface centre. < 800 tris.
3. `SM_LF_Crate` - fish crate 0.6 x 0.4 x 0.3 m, open top, slats. < 300 tris.
4. `SM_LF_Fish` - stylized fish 0.4 m, 2 colour variants (`_A`, `_B`). < 200 tris each.
5. `SM_LF_Reeds` - clump of 8-12 reeds, 1.2 m. < 400 tris.
6. `SM_LF_Rock_01..03` - shoreline rocks, 1-3 m, faceted. < 300 tris each.
7. `SM_LF_Buoy` - round marker buoy with flag, PaintRed/PaintCream. < 300 tris.
8. `SM_LF_Shack` - small fishing shack 4 x 3 m for the shore, door + window holes. < 1.5k tris.

## Rules
- Use `lf_common.py`; flat shaded; palette materials only; idempotent scripts.
- After each asset: print tri count, take one viewport screenshot, save it to `Art/Export/_shots/<asset>.png`.
- If an asset fails twice, skip it and note why. Do not modify `lf_common.py` without noting it.

## Stop boundary
Stop after the list (or after 3 hours). Report: table of asset / tris / status, plus the screenshots folder.
