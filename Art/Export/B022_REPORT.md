# B02.2 verification — 13 Nov 2026

The delivered FBXs were measured before regeneration. They ALREADY contained the fixes at session start. No new paddle/reed-clump geometry change is claimed for this session. Historical pre-fix numbers below come from the existing B022_before.json; B02's original paddle is separately documented in B02_manifest.json.

| Asset | Historical pre-fix tris | Session-start FBX tris | Verified tris | Historical size XYZ (m) | Verified size XYZ (m) |
|---|---:|---:|---:|---|---|
| SM_LF_Paddle | 156 | 204 | 204 | (1.600000, 0.377000, 0.065000) | (1.600000, 0.377000, 0.065000) |
| SM_LF_Reeds | 356 | 396 | 396 | (0.902369, 0.872499, 1.200000) | (0.877876, 0.847619, 1.200000) |
| SM_LF_ReedBed | 2988 | 2996 | 2996 | (4.000000, 3.000000, 1.800000) | (4.001063, 3.001639, 1.800000) |

## Bounds (metres)

- **SM_LF_Paddle** historical min (0.000000, -0.188500, -0.032500); max (1.600000, 0.188500, 0.032500). Verified min (0.000000, -0.188500, -0.032500); max (1.600000, 0.188500, 0.032500).
- **SM_LF_Reeds** historical min (-0.451184, -0.436250, 0.000000); max (0.451184, 0.436250, 1.200000). Verified min (-0.438938, -0.423809, 0.000000); max (0.438938, 0.423809, 1.200000).
- **SM_LF_ReedBed** historical min (-2.000000, -1.500000, 0.000000); max (2.000000, 1.500000, 1.800000). Verified min (-2.000544, -1.500814, 0.000000); max (2.000518, 1.500825, 1.800000).

Paddle: 8-sided shaft diameter 0.040 m; blade width 0.377 m versus B02 0.290 m (=1.3x); blade root/tip thickness 0.02692/0.02308 m. Overall 0.065 m Z bounds include the grip, so they do not describe blade thickness.

Both reed assets: paired surfaces measured 0.010 m apart within floating-point tolerance, closed walls and caps; no two-sided material required. Reeds have 10 leaf shells; preserved bed v1 has 180. Two identical regeneration runs per asset are recorded in B022_verified.json.

Paddle screenshots reconstruct the original B02 paddle from its exact script values, assert 156 tris, and place it at the same scale next to the delivered paddle. The historical original FBX is unavailable; this reference is a reconstruction.

- [_shots/SM_LF_Paddle_B02_Comparison_Face.png](_shots/SM_LF_Paddle_B02_Comparison_Face.png)
- [_shots/SM_LF_Paddle_B02_Comparison_Side.png](_shots/SM_LF_Paddle_B02_Comparison_Side.png)

The fixed B02.2 bed was preserved before B03 replaced the canonical bed: SM_LF_ReedBed_v1.py / SM_LF_ReedBed_v1.fbx. Geometry is unchanged apart from the _v1 object/collection/export names. Both archive script runs matched.

Evidence: B022_session_before.json, B022_verified.json; initial FBXs and scripts under _before_B022/. The canonical bed is now B03 v2; B02.2 bed values remain in the v1 export and this report.
