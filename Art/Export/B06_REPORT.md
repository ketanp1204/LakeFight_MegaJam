# B06 - rowboat UCX wall fit

Completed 2026-11-13T12:54:37.880166+00:00. B07 grip sockets were completed first when requested; B06 then resumed. Blender only, writes under Art/, no git or Unreal MCP.

## Result

All four rowboat FBXs re-exported. Render geometry, UV layers, material names/assignments/values, flat shading and origin match the actual archived pre-B06 FBXs exactly. The render construction source block is also identical. Floor _00 and stern _05 are unchanged, including their geometry and origin.

| Variant | Render tris before -> after | UCX tris before -> after | Total after | UCX pieces | Largest piece | Max outline gap |
|---|---:|---:|---:|---:|---:|---:|
| SM_LF_Rowboat | 1214 -> 1214 | 72 -> 232 | 1446 | 12 | 14 vertices | 0.878355 cm |
| SM_LF_Rowboat_Blue | 1214 -> 1214 | 72 -> 232 | 1446 | 12 | 14 vertices | 0.878355 cm |
| SM_LF_Rowboat_Green | 1214 -> 1214 | 72 -> 232 | 1446 | 12 | 14 vertices | 0.878355 cm |
| SM_LF_Rowboat_Yellow | 1214 -> 1214 | 72 -> 232 | 1446 | 12 | 14 vertices | 0.878355 cm |

The director explicitly approved extra UCX triangles while preserving render triangles, then specified <=12 convex pieces, about <=24 vertices each, and <=2cm as the outline fit target. Final result is 12 pieces, maximum 14 vertices, and 0.878355cm maximum outline difference for every variant.

## Fit measurement / outer-face Y

Metric: top-down outside envelope of the visible wood hull and gunwale, compared with the union of all UCX projections. Metal oarlocks and the bow ring are excluded from the hull outline. Both port and starboard and the entire stem-to-transom length are covered.

The profiles are piecewise linear. Validation evaluates all vertex X breakpoints and pairwise envelope-switch intersections, not merely three favourable samples. Maximum outward difference is 0.878355cm at the retained original bow inner-face corner near X=0.709911m; maximum inward difference is under numerical roundoff. This is an outline metric, not a claim that every low-poly surface facet coincides at every height.

Table is absolute Y in metres (port has negative Y; starboard positive). Three-quarter forward means X=0.75*1.58=1.185m from the origin toward the bow; near-stem is included in addition to the requested stations.

| Side | Station | X (m) | Visible hull outer abs(Y) | Before UCX outer abs(Y) | After UCX outer abs(Y) | After minus hull (cm) |
|---|---|---:|---:|---:|---:|---:|
| port | amidships | 0.000 | 0.668402 | 0.621559 | 0.668402 | 0.000002 |
| port | three-quarter forward | 1.185 | 0.324832 | 0.351905 | 0.324832 | 0.000002 |
| port | bow | 1.450 | 0.129530 | 0.184071 | 0.129530 | 0.000002 |
| port | near stem | 1.580 | 0.016487 | 0.101738 | 0.016487 | 0.000004 |
| starboard | amidships | 0.000 | 0.668402 | 0.621559 | 0.668402 | 0.000002 |
| starboard | three-quarter forward | 1.185 | 0.324832 | 0.351905 | 0.324832 | 0.000002 |
| starboard | bow | 1.450 | 0.129530 | 0.184071 | 0.129530 | 0.000002 |
| starboard | near stem | 1.580 | 0.016487 | 0.101738 | 0.016487 | 0.000004 |

These Y values are the full projected outside edge, including the gunwale-height portion. The originally reported ~0.578m is the old mid-height side face, rather than its ~0.622m highest projected outside edge. The new pieces follow the 11-degree flare; keeping these measurement bases separate avoids comparing a low wall section to a higher gunwale. The old bow was not uniformly inset: its upper/forward corners also overhung the outline, so it was reshaped rather than simply translated outward.

## Inner-face proof

Existing inner planes were partitioned without shifting them. For each old wall, all four original inner corners remain in the union of its replacement segments; the sum of coplanar inner triangle areas matches the original quad. No replacement vertex crosses inward through that plane beyond sub-micrometre float roundoff.

| Original | Replacements | Inner abs(Y) at Z=.085m / sample X | Before=after | Corner displacement (m) | Face area before -> after (m2) |
|---|---|---|---|---:|---|
| _01 | _01, _06 | 0.491705 at X=0.000 | yes | 0.0000000000 | 0.949399915 -> 0.949399977 |
| _02 | _02, _07 | 0.491705 at X=0.000 | yes | 0.0000000000 | 0.949400060 -> 0.949400100 |
| _03 | _03, _08 | 0.183809 at X=1.185 | yes | 0.0000000000 | 0.576691512 -> 0.576691466 |
| _04 | _04, _09 | 0.183809 at X=1.185 | yes | 0.0000000000 | 0.576691451 -> 0.576691480 |

New aft cheeks _10/_11 cover the old side-to-stern gap, entirely behind X=-1.28m. The unchanged floor box begins at X=-1.15m: the added cheeks are at least 13cm behind that standing-floor footprint. Their forward inner edge meets the original side inner plane. No original inner face was moved or standing-floor area reduced.

## Piece numbering / convexity

| Suffix | Role | Replaces old | Vertices | Tris |
|---|---|---|---:|---:|
| _00 | unchanged floor | _00 | 8 | 12 |
| _01 | port side aft | _01 | 14 | 24 |
| _02 | starboard side aft | _02 | 14 | 24 |
| _03 | port bow rear | _03 | 12 | 20 |
| _04 | starboard bow rear | _04 | 12 | 20 |
| _05 | unchanged stern | _05 | 8 | 12 |
| _06 | port side forward | _01 | 12 | 20 |
| _07 | starboard side forward | _02 | 12 | 20 |
| _08 | port bow/stem | _03 | 12 | 20 |
| _09 | starboard bow/stem | _04 | 12 | 20 |
| _10 | new port aft cheek | extension behind old _01 | 12 | 20 |
| _11 | new starboard aft cheek | extension behind old _02 | 12 | 20 |

Every variant uses exact sequential object names UCX_<variant>_00 through _11. All pieces are closed, material-free and convex; no concave bent wall is sent for Unreal to convex-hull across the cockpit.

Convexity test: every vertex lies behind/on every outward face plane within 0.00002m numerical tolerance. Largest measured half-space violation across all 48 exported UCX pieces: 0.0000001589m. All signed volumes are positive; no boundary/nonmanifold edges or degenerate faces.

## Validation / commands

- SM_LF_Rowboat.py ran twice consecutively, rebuilding/exporting all four variants each time. Full geometry and collision metadata reports match exactly.
- B06_Validate.py ran twice against the actual delivered FBXs and archived before FBXs; the results match. Render data, UVs, material assignments/shader values and origins are identical; floor/stern data identical; inner-plane/corner/area checks pass.
- B06_Review.py ran twice. Overlay curves follow actual UCX edges; UCX mesh materials remain empty. Curve materials and captions are screenshot staging only, not exported shapes.
- An early numeric check caught a redundant collinear outer midpoint giving a degenerate cap normal; removed the redundant point and validated native meshes and exported FBXs. Final snapshots have only the passing geometry.

    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/SM_LF_Rowboat.py')  # twice
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/B06_Validate.py')  # twice
    runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/B06_Review.py')  # twice

Files read with Get-Content/Get-ChildItem and edited with apply_patch. No git, Unreal MCP, C++ changes, builds, level edits or engine imports. FBX metre source units, FBX All scale, forward -X/up Z are unchanged.

## Files changed / saved

- Modified Art/BlenderScripts/SM_LF_Rowboat.py: collision segmentation integration, 12-piece/24-vertex guards and collision metadata. All render-construction code unchanged.
- New helpers: lf_b06.py, B06_Validate.py, B06_Review.py.
- Re-exported Art/Export/SM_LF_Rowboat.fbx, SM_LF_Rowboat_Blue.fbx, SM_LF_Rowboat_Green.fbx, SM_LF_Rowboat_Yellow.fbx.
- Evidence: _before_B06/ original script/four original FBXs; B06_before.json; B06_runs.json; B06_execution.log; B06_fbx_audit.json; B06_checks.json; B06_manifest.json; B06_review_proof.json; this report.
- B06_Review.blend; _shots/B06_UCX_Top.png, B06_UCX_Front.png, B06_UCX_<Red|Blue|Green|Yellow>_<Top|Front>.png and B06_UCX_Before_<Top|Front>.png. Twelve screenshots in total.
- B07 was completed separately while B06 was paused: SM_LF_Paddle.py/FBX now include exactly the two requested socket empties; see B07_REPORT.md. No further paddle changes were made while resuming B06.

## Human check / stop

Reimport the four FBXs in Unreal with UCX collision enabled and one convex hull per UCX piece; confirm 12 shapes, then touch/ram two boats in PIE. Review contact while rolling and at the narrowing bow, character standing room and physics cost. Blender outline/convexity evidence does not establish in-engine collision cooking or gameplay feel.

B06 complete after B07 priority work. Stopped at the requested collision/evidence boundary. No render fixes, boat handling, character logic, sockets for other meshes or T03 work started. Docs files untouched under the Art-only restriction.

Director's log draft: B06 asked / four UCX wall sets refitted with 12 convex pieces, fixed inner faces/floor/stern and unchanged rendering; maximum outline error 0.878cm / human import and contact acceptance pending. Director allowed extra UCX tris and specified the 12-piece/24-vertex/2cm limits; B07 was completed first on request.
