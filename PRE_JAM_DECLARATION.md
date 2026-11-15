# Pre-jam declaration — Epic AI MegaJam 2026

**Entrant:** Ketan Patel (solo)
**Entry:** Lake Fight: Director's Cut — Unreal Engine 5.8 (not UEFN)

## Prepared before the jam (before 12 Nov 2026, 05:00 UTC) — tooling only

| Item | Type | Notes |
| --- | --- | --- |
| `Tools/ue.ps1` | Script | Generic Unreal build / package / zip wrapper |
| `Tools/init-jam-repo.ps1` | Script | Copies templates and creates the git repo after the jam start |
| `CLAUDE.md`, `AGENTS.md` (first version), `.mcp.json`, `.codex/config.toml` | Agent / MCP configuration | Rules and endpoints for the coding agents |
| `Docs/*` templates | Document templates | Empty task, asset, decision and director's-log templates |
| `prompts/R-review.md`, `prompts/P-package-check.md` | Prompts | Generic review and packaging prompts |
| `prompts/old/*` | Prompts | Written for a different concept (Line Down); not used |
| `submission/*` | Checklists | Submission checklist and itch page template |

No game code, art, audio or level content was prepared before the jam.

## Made after the jam started

I chose Lake Fight at about 10:40 IST (05:10 UTC) on 12 Nov. Everything else was written after the start, including the Lake Fight version of `AGENTS.md`, the Blender MCP configuration, `Tools/import_fbx.py`, `Tools/setup-blender-mcp.ps1`, `Art/BlenderScripts/lf_common.py`, every task prompt, and all game code, art, audio processing and documentation.

## Prior concept

Lake Fight is a concept I explored before the jam as a separate Unity prototype: two boats on a lake in local split-screen, boat movement only. Combat, swimming, boarding and boat theft were deferred and never built. Nothing from that project — code, prefabs, models or textures — is used, and the agents were not allowed to open it. This entry was created from an empty Unreal template after the jam start; its git history begins after 12 Nov 2026 05:00 UTC. Every 3D model was made during the jam by committed Blender scripts.

## Third-party assets and AI tools

See `ASSETS.md`.
