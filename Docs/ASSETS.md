# Assets and tools used (goes into the submission)

## Third-party assets (licensed)

| Asset | Source | Licence | Used for |
| --- | --- | --- | --- |
| Sound effects, ambience and voice clips (56 WAVs, processed in A01; per-file sources in `Audio/AUDIO_REPORT.md`) | [Sonniss GDC 2026 Game Audio Bundle](https://gdc.sonniss.com) | Royalty-free, licensed as confirmed by the director | Sound effects, ambience and voices (`/Game/LakeFight/Audio`) |
| Mixamo animation clips | Adobe Mixamo | Adobe Mixamo terms; clips used inside the game only, raw files not redistributed | Animations retargeted to UE5 Manny |
| Epic UE5 Mannequin (Manny) | Epic Games engine content | Unreal Engine EULA | Character mannequin; retargeted Mixamo animations |
| Funk Formula by Nick Petrov | [Uppbeat](https://uppbeat.io/c/nick-petrov) | Director-supplied Uppbeat Basic License; attribution required | Final music |
| Boardwalk Jaunt by Sky Toes | [Uppbeat](https://uppbeat.io/c/sky-toes) | Director-supplied Uppbeat Basic License; attribution required | Main/menu music |

### Music attribution

Exact attribution text supplied by the director:

```text
Music: 'Funk Formula' by Nick Petrov on #Uppbeat https://uppbeat.io/c/nick-petrov
License code: ILNEZRJIJP1DSTBI
```

```text
Music: 'Boardwalk Jaunt' by Sky Toes on #Uppbeat https://uppbeat.io/c/sky-toes
License code: TBK6NUHLZIDMPKOK
```

## AI-generated content

| What | Tool | Notes |
| --- | --- | --- |
| Original low-poly meshes, palette materials, collision hulls and attachment/socket data | Codex writing Python scripts, executed in Blender through Blender MCP | Boats, paddles/oars, fish, docks, shoreline, vegetation, crew accessories, title lettering and ambient props/life. Geometry is constructed by committed scripts in `Art/BlenderScripts/`; FBX exports and repeat-run/round-trip evidence are recorded in the B02-B11 reports under `Art/Export/`. The title uses original block-cell lettering, not a font asset (`Art/Export/B05_REPORT.md`). Delivery does not imply every asset is placed in the game. |
| HUD icon PNGs | Codex writing Blender Python scripts | B08's eight transparent icons are orthographic renders of original scripted geometry, not text-to-image output; paddle/gold-fish revisions are recorded in `Art/Export/B10_REPORT.md`. See `Art/Export/B08_REPORT.md` for generation and validation. |
| Gameplay code, procedural animation, effects/material setup, UI and level assembly | Claude Code; Codex for the explicitly assigned T04 stages 1-2.1 | Unreal C++ and editor scripting made during the jam, under the director's task briefs. Work and checks are recorded in `Docs/CurrentTask.md` and `Docs/DirectorsLog.md`; the engine template, Manny and retargeted Mixamo clips are distinct from the original agent-written work. |
| Documentation, reports and submission copy | Codex; Claude in chat for production prompts and reviews | Drafts and task reports based on repository evidence and the director's decisions. See `Docs/DevLog_short.md`, `Docs/DevLog_draft.md` and the per-task reports. Human design, playtesting and final calls are described in the dev log. |

The recorded audio work is AI-assisted **processing of licensed recordings**, not generated music or synthesized voices: A01 selects/edits Sonniss recordings; A03 edits Uppbeat music into loops. Sources and processing are mapped in `Audio/AUDIO_REPORT.md`; the third-party credits above remain applicable.

## AI tools used during development

| Tool | Used for |
| --- | --- |
| Claude Code (Anthropic) | Unreal C++ gameplay, physics, AI opponents, animation/IK, materials/effects, audio integration and level/editor work; source reviews. Task evidence: `Docs/CurrentTask.md` and `Docs/DirectorsLog.md`. |
| Codex (OpenAI) | Original scripted Blender assets, export/geometry checks and icon renders; licensed audio processing scripts and reports; documentation; explicitly assigned Unreal T04 stages 1-2.1. Evidence: B02-B11 reports in `Art/Export/`, `Audio/AUDIO_REPORT.md` and `Docs/CurrentTask.md`. |
| Claude in chat (Anthropic) | Producer support: task briefs, reviews and scheduling, as described in `Docs/DevLog_short.md` and `Docs/DevLog_draft.md`. |

Blender MCP and Unreal MCP are development connections used by the agents, not content generators or runtime requirements. Blender/Python generate the original art; FFmpeg, NumPy and SoundFile process the licensed audio. The reports document those operations without identifying a separate image, music or voice generation service. See also `PRE_JAM_DECLARATION.md` at the repository root for the tooling and prior-concept declaration.
