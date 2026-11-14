# B11 - ambient life, stylized sky, flying birds and insects

Built in the live Blender 5.1.0 on 14 November 2026. All changes are under Art/. Includes the director's flying-bird and insect addenda. The B11 brief supplies the scope because Docs/CurrentTask.md still describes earlier code work.

## Delivered meshes

| Asset / assembly | Triangles | Required budget | Size / placement |
| --- | ---: | --- | --- |
| SM_LF_Cloud_01 | 280 | <400 | 24 x 12.59 x 11.14 m |
| SM_LF_Cloud_02 | 336 | <400 | 36 x 19.92 x 17.55 m |
| SM_LF_Cloud_03 | 336 | <400 | 48 x 22.40 x 17.50 m |
| SM_LF_Cloud_04 | 392 | <400 | 58 x 31.39 x 27.44 m |
| SM_LF_Duck | 252 | <400 | 0.35 m long; waterline origin |
| SM_LF_Swan | 312 total | <400 | 0.80 m long; waterline origin; two mesh objects |
| SM_LF_Heron | 376 | <400 | 1.00 m tall; foot/base origin |
| SM_LF_Barrel | 276 | <300 | 0.96 m long, side-on; waterline origin |
| SM_LF_LifeRing | 288 | <300 | 0.962 m across including rope; waterline origin |
| SM_LF_Bottle_Message | 256 | <300 | 0.481 m long; waterline origin |
| SM_LF_Bird_Body + Wing_L + Wing_R | 84 + 28 + 28 = 140 total | <150 total | 0.90 m level wingspan |
| SM_LF_Bird_Glide | 104 | <150 | Single merged mesh, shallow 6 degree dihedral |
| SM_LF_Dragonfly + Dragonfly_Wings | 68 + 16 = 84 total | <100 each (also <100 assembled) | Body 0.12 m, wingspan 0.18 m, authored at x1.5 |
| SM_LF_Butterfly_Body + Wing_L + Wing_R | 36 + 12 + 12 = 60 total | <100 each (also <100 assembled) | Wingspan 0.1275 m, authored at x1.5 |

Nineteen FBX files, twenty mesh objects in total (the swan FBX contains two objects). Every asset has a matching idempotent entrypoint in BlenderScripts/. They clear only their own named collection, build original geometry, set pivots, assign existing palette materials and export to Export/. Nothing was imported from another project or an asset library.

Clouds use chunky overlapping flattened puffs with different proportions and skylines. Tops are PaintCream; downward-facing underside facets use the existing Metal palette's slight blue-grey. Bird silhouettes include a Moss-headed mallard, white swan with an S neck and orange/black bill, and a grey heron standing on one leg with its other leg tucked up. The barrel has alternating wooden staves, pale end grain, metal hoops and a bung. The life ring is Orange/Cream with an outer Rope loop. The corked green bottle shows a cream message panel with raised strokes; it is a solid palette interpretation of glass, with no transparent shader or texture. Its message panel rolls upward to stay visible above the waterline.

All roots export at (0,0,0) with identity scale and rotation. Floating bird/prop geometry crosses Z=0; the swan's root is on the waterline and its separate head/neck retains its local neck-base origin. Cloud origins are at their flattened base centres. The heron's lowest foot vertices are at Z=0. Export convention is metres, Apply Scale = FBX All, forward -X, up Z. Meshes are flat shaded with no unapplied modifiers or collision meshes.

Only the existing palette is used. No team paint appears except the director's explicit PaintBlue exception on the tiny dragonfly body; its thorax is Moss. Gull bodies/wings are PaintCream with WoodDark tips, an Orange bill and tiny Black eyes. Butterfly wings use broad PaintOrange/Cream fields. No palette values or shared materials were changed.

## Articulation and placement

Export/B11_ATTACHMENTS.json is the machine-readable handoff. Values are metres; multiply by 100 for Unreal centimetres. All neutral rotations are zero. +X is forward, -Y left, +Z up.

- Swan: keep SM_LF_Swan and SM_LF_Swan_HeadNeck separate. The head/neck is parented to the body at local (0.1210526, 0, 0.1105263) m, with vertices stored relative to that neck-base pivot. Body = 140 triangles, head/neck = 172. The FBX audit verifies both nodes, hierarchy and local pivot. Review-only pitch/yaw demonstrates independent movement; no animation clip is authored.
- Gull: body and both wings are separate FBXs. Attach Wing_L at (0, -0.043, 0.006) m and Wing_R at (0, +0.043, 0.006) m relative to the body. Every wing FBX has its shoulder centre at its own origin. Flap around local X: left negative / right positive raises the wings. Screenshot up angles are -42/+42 degrees; down is +32/-32 degrees. Level assembled span was measured and asserted at 0.900000 m. Glide is one merged 104-triangle mesh with a simpler body and no tiny eyes.
- Dragonfly: attach SM_LF_Dragonfly_Wings at (0.0255, 0, 0) m relative to SM_LF_Dragonfly. Its four thin wing planes share one mesh and central thorax pivot. The 8 cm nominal body is baked at x1.5 to 12 cm; keep component scale 1 for that authored size.
- Butterfly: attach Wing_L at (0.00675, -0.00225, 0) m and Wing_R at (0.00675, +0.00225, 0) m relative to Butterfly_Body. Each wing's origin is its hinge. Flap about local X with opposite signs as for the gull. Authored x1.5 is baked into the vertices.
- Insect planes use independent front/back vertices with micrometre separation so their two faces survive FBX duplicate-face cleanup and use the existing opaque palette materials. They intentionally have open boundaries; no manifold/volume claim is made for insect wing and leg planes. Other B11 solid components pass closed-edge checks.

## Screenshots

All under Export/_shots/, captured in Blender and visually inspected through MCP look:

- B11_clouds.png: top row Cloud_01 / Cloud_02; bottom row Cloud_03 / Cloud_04, at their actual relative sizes.
- B11_cloud_undersides.png: Cloud_01 through Cloud_04 left to right, viewed from below.
- B11_birds.png: Duck / Swan / Heron, left to right, at correct relative scales and Z=0 reference plane.
- B11_floating_junk.png: barrel, life ring, message bottle on the Z=0 reference plane.
- B11_swan_neck_pose.png: neutral swan on the left; head/neck-only pitch/yaw on the right.
- B11_flying_bird_poses.png: assembled gull up, level, down across the back; merged glide in front.
- B11_flying_bird_front.png: wings up / level / down across the top; merged glide below. This view shows shoulder rotation directly.
- B11_insects.png: dragonfly, butterfly with mostly open wings, butterfly with raised wings. Both insects are shown at the same world scale.

B11_Review.blend saves source assets and separate review scenes. Posing, review placements, lighting and the green reference plane do not change the exported meshes or origins. World sky blue is scene lighting/background, not a new material.

## Files changed

- BlenderScripts/lf_b11.py: original B11 geometry, solid/two-sided plane helpers, palette exceptions, pivot setup, mesh assertions and export. Reuses only the existing in-repo lf_b10.Geo primitive helper and lf_common palette/export functions; it does not rerun B10 assets or B08/B09 scripts.
- Nineteen new BlenderScripts/SM_LF_*.py entrypoints matching the nineteen FBX basenames in B11_verified.json.
- BlenderScripts/B11_Validate.py: repeat-run reports, actual FBX reimport geometry/material/hierarchy comparisons, assembly budget/span checks and attachment manifest.
- BlenderScripts/B11_Review.py: original review poses and eight renders; reuses the existing in-repo rendering helper without executing asset entrypoints.
- Export/: nineteen named FBXs, B11_Review.blend, B11_ATTACHMENTS.json, B11_runs.json, B11_verified.json, B11_execution.log and this report.
- Export/_reports/: nineteen matching *_B11.json mesh reports.
- Export/_shots/: the eight PNGs listed above.

## Commands and evidence

Read AGENTS.md, Docs/CurrentTask.md, palette/helpers and repository status with PowerShell Get-Content/Get-Date/git status. Created source via apply_patch and PowerShell Set-Content. All generation, export, reimport, posing and rendering ran serially in Blender via MCP execute_blender_code. B08/B09 scripts were not run. No Unreal imports, C++ changes or C++ rebuilds were performed by B11. Concurrent changes outside Art/ belong to another task and are excluded from these commits.

Execution used runpy in the live Blender:

```python
v = runpy.run_path(r'D:/Projects/LakeFight/Art/BlenderScripts/B11_Validate.py')
v['run_assets']()                 # initial ten assets, each twice
v['run_assets'](v['b'].FLYING)     # flying addendum, each twice
v['run_assets'](v['b'].INSECTS)    # insect addendum, each twice
# Corrected subsets were also regenerated twice before the final audit.
v['audit']()                      # final nineteen actual exported FBXs
r = runpy.run_path(r'D:/Projects/LakeFight/Art/BlenderScripts/B11_Review.py')
r['screenshots']()                # five original B11 views
r['flying_screenshots']()         # two flying-bird views
r['insect_screenshots']()         # one insect view
```

B11_runs.json records two matching final runs per entrypoint, including complete vertex/face/material hashes, counts, bounds, origins and hierarchy. B11_verified.json records successful reimport of all nineteen FBXs with polygon geometry and per-face palette names matching at 0.01 mm precision, triangle counts matching, local pivots/parenting preserved, flat shading and rotation/scale identity (within 1e-6 rad float tolerance). Assembly checks enforce the 140-triangle flying bird, measured 0.9 m span, 84-triangle dragonfly and 60-triangle butterfly. FBX byte identity is not claimed because timestamps are embedded. The execution log retains earlier development runs; final JSON records supersede them.

Visual QA corrected pinched bird neck cross-sections, tall column-like cloud lobes, a submerged message panel and cropped swan-pose framing. FBX audit caught duplicate thin-plane faces being discarded; independent front/back vertices fixed that before delivery. Blender's imported rotation included a 1.6e-7 rad rounding remainder, so the audit uses an explicit 1e-6 rad tolerance. Final checks pass.

Git handoff uses one commit per numbered B11 item, explicit Art/ path lists, staged whitespace checks and Git LFS for FBX/PNG/blend outputs.

## Human checks / stop boundary

Please judge sky-cloud scale and colour in L01, floating bird/prop waterline placement on the game's waves, flying-bird readability at 50-150 m, and insect visibility from the chase camera. Confirm wing hinges and the swan neck after Unreal import; preserve the separate objects and attachment offsets. These static assets and Blender poses do not implement flock circling, flutter timing, movement, buoyancy or the sky replacement in Unreal. Visual acceptance remains with the director.

Docs logs remain untouched under Art-only scope. Director-log line for later: "B11 asked: clouds, ambient birds, floating junk, flying gull parts/glide and oversized insects / done: original Blender 5.1 scripts, twice-run exports, FBX audits, attachment manifest and screenshots / kept or changed: awaiting director review." Human decision recorded here: PaintBlue is explicitly allowed on the tiny dragonfly body.

Follow-up: the Blender MCP addon reports protocol 15 against server expectation 16; required calls worked. Updating it is outside B11. Stopped after requested assets, addenda, report and screenshots.
