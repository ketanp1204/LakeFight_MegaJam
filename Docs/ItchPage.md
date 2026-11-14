# Lake Fight: Director's Cut

**Fall overboard. Swim back. Leave in somebody else's boat.**

## About

I'm Ketan, and Lake Fight: Director's Cut is my small fishing lake turned into a boat brawl. Paddle someone, fall overboard, swim back, climb aboard and try leaving in their boat. The plan is a four-crew fight over fish, with plenty of wobble along the way.

I'm using Director's Cut to return to a shelved Unity idea. This jam project is built separately; the old project's code and assets are off limits to the agents.

I direct the design, playtest and make the final calls. Claude Code handles Unreal gameplay, Codex makes scripted Blender assets and processes audio, and Claude in chat helps me produce and schedule the work. Each task has a stopping point and a report I can check.

The dev log keeps the awkward parts: rowing that felt backwards, a character sitting above the seat, an oar solve that flung a hand towards the gunwale, and art that needed another pass. Passing a build or an export check doesn't settle whether I want it in the game.

## Controls

Gamepad labels below use Xbox / PlayStation names.

| Action | Keyboard + mouse | Gamepad |
| --- | --- | --- |
| Row forward / reverse while driving | W / S (or Up / Down arrows) | Right trigger / left trigger (RT / LT; R2 / L2) |
| Steer while driving | A / D (or Left / Right arrows) | Left stick left / right |
| Move while standing; swim | WASD or arrow keys | Left stick |
| Look / aim the paddle | Mouse | Right stick |
| Drive at an empty seat; stand up when driving | E | Y / Triangle (top face button) |
| Grab a nearby boat while swimming | E | Y / Triangle |
| Let go while hanging from a moving boat | E | Y / Triangle |
| Paddle attack while standing | Tap left mouse button; hold and release for a charged swing | Tap RT / R2; hold and release for a charged swing |
| Results: choose Restart / Quit | Click a button, or Left / Right arrows / Tab | D-pad left / right |
| Results: confirm selection | Enter or Space | A / Cross (bottom face button) |

Swim close to a boat and interact to grab its side. A fast boat can tow you until it slows enough to climb aboard. Once aboard, move to an empty seat and interact to drive it.

Fishing and banking happen automatically: slow down near a fish spot to catch, then enter your own team's bank zone to deposit your boat's catch. Fish stay with the boat when its driver changes.

## Credits

- **Ketan Patel** — design, direction, playtesting and final creative decisions.
- **Claude Code** — Unreal gameplay and editor implementation, plus code review.
- **Codex** — original scripted Blender art and icons, audio processing, documentation, and the assigned T04 fish/round implementation and fixes.
- **Claude in chat** — production support, task briefs, reviews and scheduling.
- **Epic Games** — Unreal Engine and UE5 Mannequin (Manny), used under the Unreal Engine EULA.
- **Adobe Mixamo** — animation clips retargeted to Manny.
- **Sonniss GDC 2026 Game Audio Bundle** — licensed sound effects, ambience and voice recordings; edited for the game. Per-file sources are recorded in `Audio/AUDIO_REPORT.md`.
- **Sky Toes / Uppbeat** — Boardwalk Jaunt, edited for main/menu music.
- **Nick Petrov / Uppbeat** — Funk Formula, edited for final music.

Music: 'Funk Formula' by Nick Petrov on #Uppbeat https://uppbeat.io/c/nick-petrov
License code: ILNEZRJIJP1DSTBI

Music: 'Boardwalk Jaunt' by Sky Toes on #Uppbeat https://uppbeat.io/c/sky-toes
License code: TBK6NUHLZIDMPKOK

## Made with

- Unreal Engine 5.8 — C++, Blueprints, Chaos physics, Enhanced Input, Niagara and UMG.
- Blender and Python — original low-poly geometry, materials, FBX exports and rendered UI icons.
- FFmpeg, NumPy and SoundFile — editing, looping and loudness processing of licensed audio.
- Claude Code, Codex and Claude in chat — the AI-assisted development workflow described above.
- Blender MCP and Unreal MCP — development automation connections.
- Git — source history and task-by-task commits.

<!-- D02 drafting notes (not page copy): Title and tagline 2, plus the four About paragraphs,
come from Docs/DevLog_short.md. Its Saturday-status paragraph is omitted from public copy.
Controls checked against Source/LakeFight/Input/LFInput.cpp, Brawler/LFBrawler.cpp,
Boat/LFBoat.cpp and Round/LFRoundWidget.cpp; Content/Input/IMC_Default.uasset,
Content/Input/IMC_MouseLook.uasset, Content/LakeFight/Input/IMC_LF_Boat.uasset and
Config/DefaultInput.ini. The mapping assets were read without opening the editor.
Jump is intentionally absent: the template action exists but LFBrawler does not bind it.
Gamepad mappings are documented, but gamepad playtesting remains unverified.
Credits/tool disclosure use Docs/ASSETS.md, Audio/AUDIO_REPORT.md, Art/Export/B02-B11
reports, Docs/CurrentTask.md and PRE_JAM_DECLARATION.md. Uppbeat lines are copied exactly
from Docs/ASSETS.md. This is a page draft, not a packaged-build or final-submission claim. -->
