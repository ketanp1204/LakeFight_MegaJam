# Director's log

One line per delegated task. Kept, changed or cut - and why. This becomes the "Director's Cut" section of the itch page.

| When (IST) | Task | Agent | Outcome | Kept / Cut / Changed - why |
| --- | --- | --- | --- | --- |
| 2026-11-12 11:28 | B01: asked for scripted red/blue rowboats | Codex | Built both FBX variants, 1,206 tris each; file executed twice with identical 17-object lists/counts/bounds and no duplicates; solid bow 3/4 screenshot saved; no known off-spec items (hull 0.58 m deep, overall 0.644 m including oarlocks). | Pending human silhouette approval; nothing marked kept/cut yet. |
| 2026-11-12 11:37 | B01.1: director approved B01 with changes | Codex | Revised sheer (+0.20 m bow/+0.08 m stern), narrow stem, 11-degree flare, double-height stripe and painted bow cap; joined each variant to one render mesh plus six material-free convex UCX boxes; 1,214 render + 72 UCX = 1,286 tris each; final file run twice with matching geometry/material hashes and no duplicates; both FBX and bow/side screenshots saved. | B01 approved with changes by director; B01.1 silhouette and in-engine collision/standing check pending human review. |
| 2026-11-12 11:37 | T01: boat on water (buoyancy, boat physics, L_Lake01) | Claude Code | Compiles; PIE 60 s with no warnings/errors; both boats settle level by 3.3 s (pitch/roll < 1 deg, within 7 cm of the waterline); rowboat FBX not imported (MCP has no Python exec) - placeholder cube visual. | Pending human feel test in PIE. |
| 2026-11-12 12:05 | T01.1: boat handling (rudder force, spool, yaw momentum), hull fit + PM, faceted lake, default map, pycache cleanup | Claude Code | Builds; PIE clean; settle level. Set lake Two Sided OFF per spec -> water vanished (grid faces down), director caught it; restored Two Sided ON. PM_LF_Hull created by director (MCP cannot create physical materials). | Pending human feel test. |
