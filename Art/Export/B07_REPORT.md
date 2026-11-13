# B07 - paddle grip sockets

Completed before resuming paused B06. Only Art/ files changed; no git or Unreal calls.

| FBX child Empty | Location relative to paddle (cm) | Rotation | Scale |
|---|---|---|---|
| SOCKET_Grip_R | (3.5, 0, 0) | (0, 0, 0) | (1, 1, 1) |
| SOCKET_Grip_L | (58.5, 0, 0) | (0, 0, 0) | (1, 1, 1) |

Grip_R is centred on the T-grip; Grip_L is exactly 55cm along +X. Both local +X axes point toward the blade, +Z normal to the wide blade face. Both are parented to SM_LF_Paddle and exported as FBX Null helpers, with the exact requested names.

SM_LF_Paddle.py ran twice with identical reports. Actual exported FBX was re-imported into Blender: one mesh and two child empties; positions, unit scales and +X/+Z axes validated. The imported render vertices, faces, material assignments, material names, origin and 204 triangle count match the archived pre-B07 FBX exactly.

Files: modified Art/BlenderScripts/SM_LF_Paddle.py; new B07_Review.py; re-exported Art/Export/SM_LF_Paddle.fbx; _reports/SM_LF_Paddle.json; B07_before.json, B07_verified.json, B07_execution.log; _before_B07/ archive.

Screenshots: _shots/B07_Paddle_Sockets.png and B07_Paddle_Sockets_Top.png. Coloured axis rods and captions are screenshot staging only; they are NOT in the FBX. The actual empty arrows are also shown.

Epic's Blender Tools documents child SOCKET_ helpers and stripping that prefix on import: https://epicgames.github.io/BlenderTools/send2ue/asset-types/static-mesh.html#sockets . Accordingly, expected runtime socket names are Grip_R and Grip_L. Exact helpers were kept as requested; no duplicate mesh-qualified aliases were added. Unreal import was not executed; verify Import Sockets is enabled and both sockets appear after reimport in the director's chosen FBX importer.

Command: runpy.run_path('D:/Projects/LakeFight/Art/BlenderScripts/SM_LF_Paddle.py') twice, then FBX round-trip checks and B07_Review.py twice.

B07 complete. B06 subsequently resumed and completed; see B06_REPORT.md.
