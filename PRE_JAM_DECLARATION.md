# Pre-jam declaration - Epic AI MegaJam 2026

Entrant: Ketan Patel
Engine: Unreal Engine 5.8   Entry type: Unreal Engine (not UEFN)

## Prepared BEFORE the jam (12 Nov 2026, before 05:00 UTC) - tooling only
| Item | Type | Notes |
| --- | --- | --- |
| tools/ue.ps1 | Script | Generic Unreal build/package/zip wrapper |
| tools/init-jam-repo.ps1 | Script | Copies templates, creates the git repo after jam start |
| templates/CLAUDE.md, AGENTS.md (first version), .mcp.json, .codex/config.toml | Agent/MCP config | Rules and endpoints for coding agents |
| templates/Docs/* | Document templates | Empty task, asset, decision and director's-log templates |
| prompts/R-review.md, P-package-check.md | Prompts | Generic review and packaging prompts |
| prompts/old/* | Prompts | Written for a different concept (Line Down); NOT used |
| submission/* | Checklists | Submission checklist, itch page template |

## Made AFTER the jam started (jam work, listed for transparency)
The concept switched to Lake Fight at ~10:40 IST (05:10 UTC). The following were written after the start:
AGENTS.md (Lake Fight version), Blender MCP config and settings, tools/import_fbx.py, tools/setup-blender-mcp.ps1,
Art/BlenderScripts/lf_common.py, prompts T01-T04 and B01-B02.

## Prior concept
"Lake Fight" is a concept I explored before the jam as a separate Unity prototype (boat movement only; combat,
swimming, boarding and boat theft were deferred and never built). Nothing from that project - code, prefabs,
models or textures - is used. This entry was created from an empty Unreal template after the jam start; its git
history begins after 12 Nov 2026 05:00 UTC. All models are made during the jam by committed Blender scripts.

## Third-party assets / AI tools
See `Docs/ASSETS.md` in the project.
