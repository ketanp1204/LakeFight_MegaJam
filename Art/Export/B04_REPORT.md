# B04 - brawler dress-up

Completed 2026-11-13T07:21:05.476121+00:00. Target: **UE5 Manny at component scale 0.75, nominal height 135cm**. The director's scale correction was applied to every final mesh and baked offset. Palette fix completed first: [B04_PALETTE_REPORT.md](B04_PALETTE_REPORT.md).

## Asset / triangles / status

| Asset | Before -> after tris | Final size XYZ (cm) | Attach to | Status |
|---|---:|---|---|---|
| SM_LF_Hat_Bucket_Red | 0 -> 304 | (30.75, 26.25, 11.66) | head | PASS export; human fit pending |
| SM_LF_Hat_Bucket_Blue | 0 -> 304 | (30.75, 26.25, 11.66) | head | PASS export; human fit pending |
| SM_LF_Hat_Bucket_Green | 0 -> 304 | (30.75, 26.25, 11.66) | head | PASS export; human fit pending |
| SM_LF_Hat_Bucket_Yellow | 0 -> 304 | (30.75, 26.25, 11.66) | head | PASS export; human fit pending |
| SM_LF_LifeVest_Red | 0 -> 440 | (39.38, 41.70, 30.93) | spine_03 | PASS export; human fit pending |
| SM_LF_LifeVest_Blue | 0 -> 440 | (39.38, 41.70, 30.93) | spine_03 | PASS export; human fit pending |
| SM_LF_LifeVest_Green | 0 -> 440 | (39.38, 41.70, 30.93) | spine_03 | PASS export; human fit pending |
| SM_LF_LifeVest_Yellow | 0 -> 440 | (39.38, 41.70, 30.93) | spine_03 | PASS export; human fit pending |
| SM_LF_Hat_Captain | 0 -> 344 | (29.55, 19.80, 13.19) | head | PASS export; human fit pending |
| SM_LF_Beard | 0 -> 80 | (10.59, 17.87, 17.32) | head | PASS export; human fit pending |
| SM_LF_Moustache | 0 -> 112 | (3.71, 24.34, 7.93) | head | PASS export; human fit pending |
| SM_LF_FishingRod | 0 -> 324 | (27.30, 11.10, 159.07) | hand_r | PASS export; human fit pending |

All twelve assets were absent at task start. Four bucket hats and four vests use Red/Blue/Green/Yellow; captain hat uses Cream/Black only; beard/moustache use WoodDark only; rod uses wood/metal/rope/black. Every prop is strictly under 600 triangles.

## Scale change proof

Initial 180cm-fit geometry was measured before the director changed the target. All twelve final min bounds, max bounds and dimensions equal 0.75 times those initial values within 0.001mm; counts are unchanged. B04_scale_before.json and B04_scale_proof.json retain the numbers.

| Family | Initial size XYZ (cm) | Final size XYZ (cm) |
|---|---|---|
| SM_LF_Hat_Bucket_Red | (41.00, 35.00, 15.54) | (30.75, 26.25, 11.66) |
| SM_LF_LifeVest_Red | (52.50, 55.60, 41.24) | (39.38, 41.70, 30.93) |
| SM_LF_Hat_Captain | (39.40, 26.40, 17.58) | (29.55, 19.80, 13.19) |
| SM_LF_Beard | (14.12, 23.82, 23.09) | (10.59, 17.87, 17.32) |
| SM_LF_Moustache | (4.95, 32.45, 10.58) | (3.71, 24.34, 7.93) |
| SM_LF_FishingRod | (36.40, 14.80, 212.09) | (27.30, 11.10, 159.07) |

The synthetic fitting proxy measures 1.350000024m from soles to head crown. It represents Manny's intended 0.75 scale and rough proportions; it is an original scripted block mannequin, not an extracted Unreal skeletal mesh. No Manny rig or animation was imported or edited.

## Attachment assumptions

Meshes use an aligned socket frame: +X forward, +Y right, +Z up. Their origins are at assumed bone pivots, with dressing offsets baked into vertices. Raw Manny bone-local axes have NOT been measured; align sockets/correct relative rotation in Unreal before judging fit.

**These FBXs are already authored at the smaller 135cm target size.** If the parent SkeletalMesh component is scaled 0.75 and ordinary scale inheritance is used, apply relative prop scale 1.333333 to cancel that extra shrink, or keep prop absolute world scale at 1. Do not multiply these meshes by another 0.75.

Assumed pivot positions in the fitting proxy, centimetres from its soles: head (0,0,119.25); spine_03 (0,0,101.25); hand_r (3.75,35.25,70.50). These approximate pivot positions are not verified Manny reference transforms.

| Family | Baked offset from aligned pivot |
|---|---|
| SM_LF_Hat_Bucket_Red | Brim opening centre +10.125cm Z from head pivot; outer crown +18.375cm Z; no separate UE translation assumed. |
| SM_LF_LifeVest_Red | Puff centre approximately +11.625cm X, -3.75cm Z from spine_03; front opens along centre Y=0. Proxy spine_03 at 101.25cm above soles. |
| SM_LF_Hat_Captain | Band lower edge +11.25cm Z and crown top +22.65cm Z from assumed head pivot; visor +X. |
| SM_LF_Beard | Front surface X=7.125..14.475cm, lower tip Z=-8.625cm, cheek tips Z=+8.25cm from assumed head pivot. |
| SM_LF_Moustache | Upper-lip centre approximately +11.25cm X,+5.55cm Z from assumed head pivot, curl span approximately 24.3cm. |
| SM_LF_FishingRod | Origin at middle of grip; butt Z=-12.75cm, rod tip Z=+146.25cm. Align socket local +Z along rod; review holds it 25deg forward. |

Machine-readable per-asset notes: [B04_ATTACHMENTS.json](B04_ATTACHMENTS.json). Fishing rod preview is tilted 25 degrees about aligned local Y, tip forward. Hat/beard/moustache/vest previews otherwise use identity rotation. The vest is a rigid spine attachment with neck/arm openings; it is not skinned and needs animation clearance review.

## Validation and screenshots

- Each of the six B04 asset scripts ran twice consecutively at the final scale. Full reports including geometry hashes, material RGB and attachment metadata matched exactly across each pair. All four variants of each team family share geometry; only team material changes.
- B04_Validate.py ran twice and round-tripped all 20 FBXs: twelve props plus eight boat/flag re-exports. Exact counts and material names, bounds within 0.01mm, current palette shader RGB, flat shading, no image textures, zero degenerate faces and zero nonmanifold edges after welding only the audit copy.
- All vertices are original scripted geometry. No textures, external libraries, other-project meshes or generated third-party assets.
- Every B04 asset has _shots/<asset>_Proxy.png showing the complete 135cm block mannequin and ruler, plus _shots/<asset>_Detail.png. All final screenshots were regenerated after the scale correction.
- Combined checks: _shots/B04_CrewLeader_Proxy.png and B04_CrewLeader_Head.png; overview B04_DressUp_Gallery.png. Palette lineup B04_TeamPalette_BoatsFlags.png.
- B04_Review.py ran twice with identical numerical fitting evidence. B04_Review.blend contains final masters at origin and a separate gallery of dressed proxies. The proxy and staging geometry are not exported as game meshes.

Visual review caught a cream vest strap disappearing into the front pillows. Changed that strap from an ellipse to a chamfered rectangle so it stays continuous and visible; final triangle count remains 440. Final close-up inspected after correction.

## Files changed / saved

- Modified Art/BlenderScripts/lf_common.py: four team RGB values and live-material refresh only. Rowboat/flag source scripts unchanged.
- New original asset scripts: SM_LF_Hat_Bucket.py, SM_LF_LifeVest.py, SM_LF_Hat_Captain.py, SM_LF_Beard.py, SM_LF_Moustache.py, SM_LF_FishingRod.py.
- New support/proof scripts: lf_b04.py, B04_Review.py, B04_Validate.py. Existing lf_b02.py/lf_b03.py were reused without edits in this task.
- Art/Export: twelve named B04 FBXs, eight palette-updated boat/flag FBXs, per-prop JSON under _reports/, 24 per-prop fitting screenshots plus combined/gallery/palette images under _shots/, B04_Review.blend.
- Evidence: B04_before.json; B04_palette_before.json, B04_palette_fix.json, B04_palette_execution.log, B04_PALETTE_REPORT.md; B04_runs.json, B04_execution.log, B04_scale_before.json, B04_scale_proof.json, B04_review_proof.json, B04_fbx_audit.json, B04_checks.json, B04_ATTACHMENTS.json, B04_manifest.json, B04_REPORT.md.

## Commands / results

All Blender operations ran serially through execute_blender_code:

    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/SM_LF_Rowboat.py')  # twice, all four boats
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/SM_LF_TeamFlag.py')  # twice, all four flags
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/<B04 asset>.py')  # twice per final source
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/B04_Review.py')  # twice
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/B04_Validate.py')  # twice

Files were inspected with Get-Content/Get-ChildItem/rg and edited with apply_patch under Art/. No git, Unreal MCP, C++, Unreal build or uasset writes. FBX scale is Apply Scale FBX All; forward -X, up Z, metre source units.

## Human check / stop boundary

Pending: actual UE5 Manny socket orientation and pivot placement, prop scale inheritance under component scale 0.75, head/hat clearance and rigid vest clipping in idle/run/punch poses, rod hand grip, palette appearance under the actual game lighting. Proxy screenshots establish approximate scale, not certified skeletal fit or accepted art direction.

Stopped at palette fix + B04; no gameplay, animation, attachment Blueprint or Unreal import work started. Docs files remain untouched under the Art-only rule.

Director's log draft: 2026-11-13 - Asked palette fix first then B04; four hues updated, four boats/four flags re-exported without geometry changes; twelve dress-up props generated for 0.75-scale Manny, two-run/FBX proof and 135cm proxy shots saved; kept/changed/cut pending director review. Director changed nominal target from 180cm to 135cm during task.

Follow-ups: human Unreal import/socket/pose/lighting review; move the log draft into Docs when permitted.
