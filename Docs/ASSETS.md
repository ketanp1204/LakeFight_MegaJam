# Assets, licences and AI use — Lake Fight: Director's Cut

## Third-party assets

| Asset | Source | Licence | Used for |
| --- | --- | --- | --- |
| Sound effects, ambience and voice clips (56 files) | Sonniss GDC 2026 Game Audio Bundle (https://gdc.sonniss.com) | Royalty-free bundle licence | All sound effects, ambience and voices, edited for the game |
| Boardwalk Jaunt — Sky Toes | Uppbeat (https://uppbeat.io/c/sky-toes) | Uppbeat licence, attribution required | Main and menu music |
| Funk Formula — Nick Petrov | Uppbeat (https://uppbeat.io/c/nick-petrov) | Uppbeat licence, attribution required | Final-30-seconds music |
| UE5 Mannequin (Manny) | Epic Games engine content | Unreal Engine EULA | Character body |
| Animation clips | Adobe Mixamo | Mixamo terms; used inside the game only, raw files not redistributed | Swimming, sitting, walking, dizzy and celebration clips, retargeted to Manny |

### Music attribution (shown in the game's credits and on the game page)

Music: 'Funk Formula' by Nick Petrov on #Uppbeat https://uppbeat.io/c/nick-petrov
License code: ILNEZRJIJP1DSTBI

Music: 'Boardwalk Jaunt' by Sky Toes on #Uppbeat https://uppbeat.io/c/sky-toes
License code: TBK6NUHLZIDMPKOK

## AI-generated and AI-assisted content

| What | Made by | How |
| --- | --- | --- |
| Gameplay code, physics, AI crews, procedural animation and IK, materials and effects, audio integration, UI, level layout, packaging | Claude Code (Anthropic) | Unreal C++ and editor scripting during the jam, task by task under my briefs; every feature accepted only after my own playtest |
| Fish and round systems (catch, carry, bank, timer, gold fish, HUD first version) | Codex (OpenAI) | Unreal C++ in new files while Claude's usage limit was reached; reviewed by Claude Code |
| All 3D models: boats, oars, paddles, fish, docks, shoreline, islands, vegetation, crew gear, wildlife, clouds, title lettering | Codex (OpenAI) | Python scripts executed in Blender through Blender MCP; every mesh is built by a committed script, run twice and re-imported to verify it |
| HUD icons and title lockups | Codex (OpenAI) | Orthographic Blender renders of the same scripted geometry |
| Audio processing | Codex (OpenAI) | Python + FFmpeg scripts that cut, loop and level the licensed recordings; no generated music or voices |
| Trailer edit and key art | Claude (Anthropic) | Cut and composited with FFmpeg and Python from my own gameplay recordings, in-engine screenshots and the Blender title lockups |
| Dev log, game page, reports and task briefs | Claude and Codex | Drafted from the project's records and my answers; edited and approved by me |

No text-to-image, text-to-music, text-to-speech or text-to-3D generators were used. Unreal MCP and Blender MCP are the connections the agents used to work in the editors; they are not content generators and are not needed to run the game.

## Human work

Design, direction, every review and feel decision, playtesting, the theme reading and the final calls: Ketan Patel. The dev log describes which decisions were mine and what I rejected.
