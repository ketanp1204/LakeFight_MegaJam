# Lake Fight: Director's Cut — dev log

*Epic AI MegaJam 2026 · solo entry by Ketan Patel · Unreal Engine 5.8*

## The shelved idea, uncut

Lake Fight is a physical-comedy boat brawler on a small fishing lake. Four crews fight over the fish: you catch them, carry them home in your boat and bank them at your dock, while everyone else tries to take them off you with a paddle, a ram or by stealing your boat outright. You can fall overboard, swim back, climb into the nearest boat and row off with someone else's catch.

I read the theme "Director's Cut" as "the shelved idea, uncut." The old Lake Fight was a Unity prototype: two boats on a lake in local split-screen co-op. Combat and boarding were deferred, and paddle fights, overboard ragdolls, swimming, boarding, stealing boats and the four-crew fight over fish never got built. This jam was my chance to go back for exactly those parts.

I shelved the prototype because local split-screen co-op felt like a small audience, and making couch multiplayer feel right looked like a long job, so I moved on to other projects. The jam version flips that: it's single-player against three AI crews, built around the fights the old prototype never had. Split-screen isn't in this version; it's the first thing on my list for later.

The old project was off limits to the agents. My rules forbade opening it or copying its code, models or textures. Everything in this entry was made during the jam, starting from the declared jam boundary.

*[Image: Docs/Shots/Hero/Hero_02_pileup_overboard.png — three crews overboard around their boats]*

## Who did what

I'm Ketan, the director. I chose the design, reviewed everything that came back, played it and made the final calls. I like to watch my agents working and steer them a lot as they go.

- **Claude Code** handled Unreal C++ and Unreal MCP work: physics, the brawlers, AI, animation, audio, UI, the lake and packaging.
- **Codex** handled scripted Blender MCP assets, the audio processing pipeline, documentation, and — when I ran out of Claude — part of the round loop.
- **Claude in chat** was my producer: task prompts, reviews of every report and screenshot, scheduling, and the video and key art.

The loop was simple: put one bounded task in `CurrentTask.md`, give the agent an acceptance check and a stopping point, read its report, then keep, change or cut the result. Nearby problems became follow-ups. An agent didn't get to expand the game just because it found another thing to build. Feel was never accepted from a report: only from me playing it.

For art, the deliverable was the script as well as the mesh. Every Blender script ran twice, the geometry was compared, and the exported FBX was reimported and checked. For audio, a processing report recorded every source, slice and loudness target.

## Thursday, 12 November: get a boat onto water

**B01 / B01.1:** Codex made the first red and blue rowboats. I approved the direction with changes: more rise at the bow, a narrower stem, flared sides and a stronger painted stripe.

**T01:** Claude Code got buoyancy and boat physics running on our own summed-sine water, the same function driving both the material and the buoyancy.

**T01.1:** My first drive "felt like a car" with "not enough oomph", so handling gained rudder force at the stern, throttle spool and yaw momentum. A water-material setting made the lake disappear; I caught that in review.

**T01.2:** Separate bump and ram responses, collision hulls and a water mask inside the boat.

**B02:** The first prop batch: paddles, dock, fish, reeds, rocks, buoy, crate and shack.

*[Image: Art/Export/_shots/SM_LF_Rowboat_B011_Side.png — the scripted rowboat]*

## Friday, 13 November: turn boats into a fight

**T01.3 / T01.4:** Water and camera went through repeated review. I rejected moving facet polygons, a straight V-shaped wake and solid foam. Camera shake was "janky" because hitstop fought the camera lag; moving the shake onto real time fixed it, and rams now feel right.

**T02:** The brawler could stand, drive and swing a paddle. I rejected the first cannonball-style overboard launch, which became a lower ragdoll topple over the rail. Swimming, grabbing a gunwale, climbing and stealing replaced the temporary reset: a logged run went from overboard all the way to driving a stolen boat.

**T03:** One Blue rival learned to chase, ram, fight, recover a boat and flee using exactly the player's actions, with reaction delay and imperfect aim. Then three crews with personalities: the Bully rams, the Thief steals, the Hothead swings first. Their first AI-only run produced a steal and a string of overboards.

**T03.1:** Faster boats can tow someone hanging off the gunwale, and boats capsize, throw everyone out and right themselves.

**B03–B09:** Shoreline, plants, crew gear, larger hats, the title, team docks, HUD icons, and proper oars with oarlock sockets.

**A01:** A processed sound library from the Sonniss GDC bundle, with an audition page so every choice stayed reviewable.

*[Image: Docs/Shots/T03_spectate_1.png — the first four-crew AI test]*

## Saturday, 14 November: rowing, sound and the lake

**T03.1 / T03.2 — grip and rowing:** The paddle hold and rowing went through several setups (more on that below). Real oars, a seat measured from the mesh and foot IK on the floorboards made the rower sit in the boat instead of above it.

**A02 / A03:** Sounds wired into gameplay; music with Main, Menu and Final states, bar-length crossfades and ducking on rams. A four-boat pile-up exposed stacked bass tails, so those got capped.

**B10 / B11:** Ground, distant hills, bushes and trees beyond the shore; then clouds, ducks, a swan, a heron, floating junk, gulls and insects.

*[Image: Docs/Shots/T031_rowing_push_stroke.png — the forward-facing push stroke]*

### The lake, three times

The first layout came back as a swimming pool with nicer edges. The islands sat by the walls, so nobody's route home passed anything, and the left side had no island at all. The second pass solved that on paper: four arms, an island on every route home. But it came back as a perfect X. I only saw it from above and knew straight away that it would make the movement and the scenery dull, and look man-made. I asked for irregular lobes instead: four uneven arms, wide wooded headlands, one island on each route home, docks 68.5–72 m from the centre and about 57 m of open water in the middle. That's the lake in the game.

*[Image: DevLog_lake_three_passes.png — pool, X, final lobes]*

### The round, and running out of Claude

Halfway through Saturday my Claude usage hit its limit with the round loop not started. Instead of waiting, I handed the first two stages to Codex, boxed into new files with small marked hooks, and had Claude review the first stage read-only when the quota came back (12 findings, 2 must-fix). I was worried the code would stop looking like the rest of the game. It didn't: Codex handled it well enough that I couldn't tell the difference.

My own playtest changed the details. The fish spots were static rings I couldn't read, so they got bobbing pieces, ripples and leaping fish. Catching was fiddly at low speed, so the catch radius went from 2 to 4 m. And you couldn't tell who was carrying fish, so every boat got a live count above it. The result: a 4-minute round, a gold fish at 2:00 worth 5 each, a final-30-seconds music change, and an end screen with comedy stats.

*[Image: Docs/Shots/UI/N5_06_hud_gold.png — the HUD and the gold fish alert]*

## Saturday night: an AI employee on the night shift

I had to be away from the machine, so the last big chunk of work ran unattended on my laptop: one long checklist, a commit per item, and a scheduled restart every five hours when the usage window reset. It felt like having an AI employee working while I was busy with other things, which in a time-limited jam is a big deal.

By morning it had finished 17 items: AI crews that fish and bank, a bug pass (driverless boats, sinking swimmers, sliding while charging a swing, the seated paddle, low-speed turning), the main menu, credits, pause, how-to-play, graphics options, a stylized sky, ducks, gulls and shore spectators, a full UI pass and hero shots. Each was packaged and smoke-tested, with a safety build kept aside. Everything it did was marked "unreviewed" until I played it.

## Sunday, 15 November: making the fight worth having

Playing the overnight build, I noticed Yellow won most rounds because a fishing spot sat next to its dock, and nobody had a reason to fight while everyone was busy fishing. Swimmers were also still sinking out of sight.

The next run fixed the swimming (measured against the water every frame: heads now stay above the surface) and made the spots fair: a centre spot plus one that rotates between the docks. It also gave crews reasons to fight: loaded boats and the leader draw attention, a crew defends its spot, and the Thief dives in to steal a loaded boat. That doubled the overboards, but halved the scores.

One more pass brought the fish back and added **ram steal**: ram a loaded boat and a fish flies into yours. It feels unfair, and rewarding when you're the one doing it. Now a different crew wins each round, and the tension in the last minute is real: my best round ended with a bank nine seconds before the whistle, 24–22.

The last polish was mine: the whole UI down to 70% of its size, and the menu hover sound gone.

*[Image: Docs/Shots/Hero/Hero_07_brawl_on_the_spot.png — a brawl on a fish spot]*

## What the AI got wrong, and how we caught it

**The mirror lake.** The first water rendered as a mirror with black holes in it. I caught it in my first screenshot review; it was replaced with our own water and a toon lake material.

**Water inside the hull.** Even with a hull mask, review found submerged hull edges, lines on the boat bottom and a foam halo. The fixes used the hull stencil and the real outline.

**The paddle hold.** The paddle first floated in front of the brawler's hands. Grip sockets and two-hand IK fixed that; then I rejected a shoulder-pivot grip and a diagonal hold before settling on a level, chest-centred one. Playtests caught the right hand drifting across the chest mid-swing: the paddle pivot needed to turn with the torso.

**Rowing direction took three tries.** Facing the bow with a classic pull looked familiar but pushed water the wrong way. Facing the stern made physical sense, but pressing W moved the boat behind the rower's back, which felt backwards. We kept facing forward with a push stroke.

**The floating seat.** Measured from the skinned mesh, the rower sat 10 cm above the thwart. Lowering him brought it to 0.4 cm, and foot IK put the soles on the floor.

**The oar arm glitch.** In a playtest the left arm and oar glitched at the end of the stroke: the reach solve broke down and flung the hand at the gunwale. A soft clamp and staggered hands fixed it.

**Washed-out red.** The palette values in Unreal matched the source, but the source red itself was salmon-like. I supplied an exact red and the team colours were strengthened.

**The reeds.** The first reed beds didn't read as reeds; they were rebuilt as narrow blades and cattails, and later replaced on the lake with organic patches of small clumps.

**Balance by accident.** Yellow kept winning from its doorstep and the AI rarely fought. Only playing it showed that; the fix is described above.

**An agent crossed the no-git boundary.** Codex committed during an art task while the rule was "no git". I resolved that by allowing art-task commits only for explicitly listed `Art/` paths, one commit per item, and wrote the rule into `AGENTS.md`.

*[Image: Docs/Shots/T032_fix_rowing_strip.png — the rowing fix, frame by frame]*

## What I left on the cutting-room floor

Split-screen would be my first pick: the original Lake Fight started as a couch game, and that's still where it belongs. I'd also love a boat-to-boat jump, so I could knock a loaded crew out of their own boat and row off with it. Gamepad support is mapped, but I haven't tested it with a real pad yet.

The uncut version, for me, is everything that was missing from the old prototype: paddle fights, ram steal, swimming back and stealing boats, with decent animation and a loop that's actually fun to play.

## Calls I made

- I chose a low-poly physical comedy game and kept final feel acceptance with myself.
- I changed the boat silhouette and allowed extra collision triangles to fit the visible hull.
- I cut the cannonball overboard, moving water facets and the straight wake.
- I chose forward-facing push rowing over the stern-facing setup.
- I asked for larger hats, supplied the red and reserved team colours for team gear.
- I rejected the symmetric X lake for irregular lobes.
- I handed round-loop code to Codex when Claude's quota ran out, boxed into new files.
- I put moving wildlife behind a working round loop.
- I asked for fair fish spots after Yellow kept winning from its doorstep, and kept ram steal.
- I shrank the whole UI to 70% and cut the menu hover sound.

## By the numbers

- **116 commits** over four days (6 Thursday, 29 Friday, 63 Saturday, 18 Sunday).
- **23 unattended checklist items** across the overnight and Sunday runs, each packaged and smoke-tested.
- **More than 150 headless AI-only rounds** played to tune the balance.
- **86 scripted FBX exports** from Blender by Saturday, including colour variants and parts. Boats are 1,214 triangles plus 232 for collision; the flying gull is 140.
- **59 processed audio files**: 56 sound effects, ambience and voice clips, plus three music edits.
- On the dev laptop the packaged game runs at about 120 fps on High during a four-boat pile-up.

## Tools and licences

- **Unreal Engine 5.8**, C++. Manny is Epic engine content under the Unreal Engine EULA; animations include Mixamo clips retargeted to Manny, used under Adobe's Mixamo terms.
- **Blender**, driven by committed Python scripts, made every original mesh. FFmpeg, NumPy and SoundFile processed the audio.
- **Sound effects:** Sonniss GDC 2026 Game Audio Bundle (royalty-free).
- **Music (Uppbeat, credit required):**
  - Music: 'Boardwalk Jaunt' by Sky Toes on #Uppbeat https://uppbeat.io/c/sky-toes — License code: TBK6NUHLZIDMPKOK
  - Music: 'Funk Formula' by Nick Petrov on #Uppbeat https://uppbeat.io/c/nick-petrov — License code: ILNEZRJIJP1DSTBI
- **AI tools:** Claude Code and Claude (Anthropic), Codex (OpenAI). The full AI-content disclosure and my pre-jam declaration are on the game page.

## Sunday: submission

The submitted build is the Windows Shipping package from the final UI pass, smoke-tested through a full round from the main menu to the results screen, then downloaded back from itch and run from a fresh folder. The submission video was cut from one of my own rounds in the final balance build.

Kept: everything in the build. Cut: split-screen and the boat-to-boat jump, for now.
