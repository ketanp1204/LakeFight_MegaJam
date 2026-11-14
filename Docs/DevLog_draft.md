# Lake Fight: Director's Cut — dev log draft

*Saturday draft. Counts are frozen at Git commit `b97d4bf`, before this writing task. Image slots point to existing files; captions still need my editorial review.*

## The shelved idea, uncut

Lake Fight is a physical-comedy boat brawler on a small fishing lake. You can fight someone with a paddle, fall overboard, swim back, climb into a boat and steal it. The larger plan is a four-crew fight over fish, with enough wobble and bad decisions to stop it becoming a tidy race around some buoys.

I'm reading the jam's theme 'Director's Cut' as “the shelved idea, uncut.” The old Lake Fight was a Unity prototype with two boats on a lake in split-screen co-op. Combat and boarding were deferred. Paddle fights, overboard ragdolls, swimming, boarding, stealing boats and the four-crew fight over fish never got built. This is my chance to return to those parts of the idea.

I shelved the project because I'd built the prototype as a local split-screen co-op. That felt like a small audience, and making couch multiplayer feel right looked like a long job, so I moved on to other projects. The jam version flips that: it's single-player against three AI crews from the start, and split-screen becomes an extra rather than the foundation. That's a big part of why the cut features finally fit. 

The old project is off limits to the agents. My rules forbid opening it or copying its code, models or textures. The jam project has its own work, starting from the declared jam boundary. 

![Image slot: the scripted rowboat, side view](../Art/Export/_shots/SM_LF_Rowboat_B011_Side.png)

## Who did what

I'm Ketan, the director. I choose the design, review what comes back, play it and make the final calls. An agent can tell me something builds or produce a clean log. I still have to decide whether steering, hitting someone or climbing aboard feels right. I like to watch my agents working and steer them a lot as they go to nudge the work in the direction that I want.

Claude Code handles Unreal C++ and Unreal MCP work. Codex handles scripted Blender MCP assets, the audio processing pipeline and documentation. Claude in chat is my producer: task prompts, reviews and scheduling. 

The intended loop is simple: put one bounded task in `CurrentTask.md`, give the agent an acceptance check and a stopping point, inspect its report, then keep, change or cut the result. Nearby problems become follow-ups. The agent doesn't get to expand the game just because it found another thing to build.

In practice, some art reports say that `CurrentTask.md` still described another agent's work, so my explicit art brief supplied their scope. The written process and the actual handoff were not always perfectly aligned.

For art, the useful deliverable is the script as well as the mesh. We run it twice, compare the generated geometry, then reimport the FBX and check what survived export. A pretty screenshot alone cannot tell me whether a shoulder pivot moved or collision filled the cockpit.

For audio, the processing report records sources, selected slices, loudness and repeatability. Those checks make review easier. They don't listen on my behalf.

![Image slot: crew clothing and accessories on fitting proxies](../Art/Export/_shots/B04_DressUp_Gallery.png)

## Thursday, 12 November: get a boat onto water

**B01 / B01.1:** Codex made the first red and blue rowboats. I approved the direction with changes: more rise at the bow, a narrower stem, flared sides and a stronger painted stripe.

**T01:** Claude Code got buoyancy and boat physics running. The first report had stable boats but cube visuals because the available MCP route could not execute the mesh-import Python.

**T01.1:** Handling gained rudder force, throttle spool and yaw momentum. A water-material setting made the lake disappear; I caught that during review and it was restored.

**T01.2:** We added separate bump and ram responses, collision hulls and a water mask inside the boat. The report explicitly left driving and ram feel for my test.

**B02:** The prop batch supplied paddles, dock, fish, reeds, rocks, buoy, crate and shack. Git records the batch on Thursday; later corrections belong to Friday's art reports.

## Friday, 13 November: turn boats into a fight

**T01.3 / T01.4:** Water and camera work went through repeated review. I rejected moving facet polygons, the straight V-shaped wake and solid foam. Exposure and palette checks followed.

**T02, steps 1–2.1:** The brawler could stand, drive and swing a paddle. I rejected the cannonball-style overboard launch, which became a lower ragdoll topple over the rail.

**T02, step 3:** Swimming, grabbing a gunwale, climbing and stealing replaced the temporary reset. A logged player sequence went from overboard all the way back to driving a stolen boat.

![Image slot: swimming after leaving the boat](Shots/T025_swim.png)

**T03, stage 1:** One Blue rival learned to chase, ram, fight, recover a boat and flee, using the same player actions. It also needed reaction delay and imperfect aim.

**T02, step 5 / T03, stage 2:** Clothing, animation hooks and paddle grip arrived alongside three rival personalities. The recorded AI-only run produced a steal and repeated overboards; whether those personalities read without debug labels remained my question.

![Image slot: the four-crew spectator test](Shots/T03_spectate_1.png)

**T03.1:** Faster boats could tow someone hanging from a gunwale. Capsizing threw occupants out and temporarily prevented driving. The automated run demonstrated righting and reboarding, but did not demonstrate the hang-and-tow case.

**B03–B07:** Art work covered shoreline pieces, plants, crew gear, larger hats, the title and team dock. Collision and paddle sockets were separate jobs; I brought B07 forward while B06 waited.

**B08 / B09:** Round markers and HUD icons arrived, followed by proper oars and oarlock sockets. These were assets for integration, not proof that the round loop or rowing animation was finished.

**A01:** The processed sound library was committed. Its source mapping and audition page kept the sound choices available for review rather than burying them in an import folder.

## Saturday, 14 November, so far

The log crosses midnight, and some entries were written out of order. I'm grouping the work by its recorded day rather than pretending every row is a precise stopwatch.

**T03.1 grip and rowing:** The late-night pass replaced the sword-slash base with procedural movement and worked through several holding and rowing setups. My “looks great” applied to rowing; hit-rate and walking feel were still unmeasured.

**A03 / A02 / A02.1:** Music loops were processed, sounds were wired into gameplay, and music gained persistent playback, crossfades and ram/capsize ducking. A four-boat pile-up exposed stacked bass tails, so the implementation limited them and added burst ducking.

**T03.2 / Playtest 007:** Real oars, seating and foot placement improved. Then my playtest found an arm-and-oar glitch at the end of the stroke. That got another correction and another pending human check.

![Image slot: rowing correction strip from Playtest 007](Shots/T032_fix_rowing_strip.png)

**B10:** Ten environment meshes extended the ground and distant hills beyond the shore. The paddle icon also became horizontal with an oval blade, and the bonus fish gained a pale highlight and sparkles.

**B11:** Clouds, birds, floating junk and insects were delivered and kept. Clouds and one static heron are planned for L01. I deferred moving wildlife until T04 works, and enlarged the duck and bottle in Unreal for chase-camera readability.

![Image slot: ambient birds at their authored relative scales](../Art/Export/_shots/B11_birds.png)

## What the AI got wrong, and how we caught it

**The mirror lake.** The first T01 water rendered as a mirror with black holes in it. I caught that in my first screenshot review. We replaced it with custom summed-sine water and a toon lake material.

Later reviews rejected moving facets, a straight wake and heavy foam. T01.3 records fragmented foam, depth-colour corrections and foam following the actual hull outline. Those are documented changes, with some look approval still pending.

**Boat handling.** My T01 playtest notes were “feels like a car” and “not enough oomph”. That led to rudder force at the stern, throttle spool and yaw momentum in T01.1. The implementation report still left feel acceptance to me.

**Water inside the hull.** T01.2 introduced the hull water mask and checked it in screenshots at zero and fifteen degrees of roll. Later review still found submerged hull edges, bottom lines and an unwanted foam halo. The fixes used the hull stencil and real outline. The lake also became translucent because of a material limitation, which the report disclosed.

![Image slot: water treatment from the chase view](Shots/T013_chase.png)

**Camera-shake jank.** I called it “janky”. Hitstop was fighting camera lag. The T01.3 fix moved the shake onto real time, along with camera and FOV timing, using one decaying vertical shake and a pair cooldown. The report could verify a clean run, but could not drive a ram through MCP.

TODO(director): Add my camera-shake acceptance verdict after testing the real-time fix in a ram.

**The paddle hold.** On Friday the paddle floated in front of the brawler's hands instead of being held. The fix added `Grip_R` and `Grip_L` sockets on the paddle and two-hand IK. I also rejected the first shoulder-pivot grip. It moved towards the waist, then later iterations settled on a level chest-centred hold with hand alignment and finger curl. Playtests caught the right hand drifting across the chest during a swing; the paddle pivot needed to turn with the torso twist.

![Image slot: the earlier diagonal hold for comparison](Shots/T031_grip_v1_diagonal_front.png)

**Rowing direction took three tries.** Facing the bow with a classic pull looked familiar but pushed water the wrong way. Facing the stern made physical sense, but pressing W moved the boat behind the rower's back. I cut that because it felt backwards. We kept facing forward with a push stroke, reversing to a pull for reverse throttle.

![Image slot: the forward-facing push stroke](Shots/T031_rowing_push_stroke.png)

**The floating seat.** T03.2 measured the character's backside from skinned vertices: it sat 10.1–10.9 cm above the thwart. Lowering the driver 10.4 cm brought that gap to 0.4 cm; foot IK brought soles within 0.2 cm of the floor. Those measurements support the fix, while the log still asks me to check the pose in play.

**The oar arm glitch.** In Playtest 007 I reported the left arm and oar glitching. The reach solve broke down near the stroke's end, throwing the hand towards the gunwale before snapping back. A continuous soft clamp, fore-aft hand stagger and raised hand goals reduced the measured grip jump. It still needed my re-check.

**Washed-out PaintRed had two parts.** The Unreal exposure check found imported palette values matched their source. The source red itself was salmon-like. I supplied an exact red, and the palette pass strengthened the team colours, re-exporting boats and flags without changing their geometry. Checking values and checking the final look were separate steps.

![Image slot: the revised team palette on boats and flags](../Art/Export/_shots/B04_TeamPalette_BoatsFlags.png)

**The reeds needed another pass.** The first reed beds didn't read as reeds in review. B03 v2 rebuilt them as narrow blades and cattails, without rows. A geometry test caught root gaps; the agent changed blade construction and added coverage. It reported near-total concealment from one fixed broadside test, while leaving other angles to human review.

**An agent crossed the no-git boundary.** Codex committed during an art task while the rule was “no git”. I resolved that by allowing art-task commits only for explicitly listed `Art/` paths, one commit per item. That rule is now in [AGENTS.md](../AGENTS.md), alongside the task boundaries. Finishing the asset did not excuse ignoring the instruction.

## Calls I made

- I chose a low-poly physical comedy game and kept final feel acceptance with myself.
- I changed the boat silhouette and later allowed extra collision triangles to fit the visible hull.
- I cut the cannonball overboard, moving water facets and straight wake.
- I chose forward-facing push rowing over the stern-facing setup.
- I asked for larger hats, supplied the red colour and reserved team colours, with explicit exceptions.
- I kept B11, but put moving wildlife behind a working round loop. A finished bird mesh didn't earn permission to expand the schedule.

## By the numbers

These are a snapshot, not submission totals. At `b97d4bf`, Git has **59 commits**: 6 dated Thursday, 29 Friday and 24 Saturday. This excludes the dev-log commit and anything added afterwards.

There are **86 tracked top-level FBX exports** under `Art/Export`: 8 icon-stage exports and 78 `SM_LF` files, including the preserved reed-bed v1. That count includes colour variants and separate parts; it is not 86 unique designs or 86 placed objects.

The task-family count is **17**: B01–B11, T01–T03 and A01–A03. Revisions, stages and numbered items stay inside their families. Active L01 and planned T04 are excluded. Separately, the Director's Log contains **25 dated entries**, including experiments and playtest follow-ups.

The general art rule is under 3,000 triangles per boat and under 1,000 per prop, with task-specific budgets recorded separately. B06 boats have 1,214 render triangles plus 232 collision triangles. B11's assembled gull is 140, its distant glide 104, its dragonfly 84 and its butterfly 60.

![Image slot: wings up, level, down and the merged glide](../Art/Export/_shots/B11_flying_bird_front.png)

The audio report has **59 output WAVs**, **34 source mappings**, **40 mono / 19 stereo** outputs and **8 loops**. Targets are -16 LUFS for one-shots, -18 for voices, -20 for UI and -28 for ambience; music targets are -22 main, -24 menu and -21 final. The true-peak ceiling is -1 dBTP. These describe processing, not my approval of the mix.

## Tools and licences

The game uses Unreal Engine, with Blender scripts producing the original meshes. The audio pipeline uses FFmpeg, NumPy and SoundFile. Manny is Epic engine content under the Unreal Engine EULA; animations include Mixamo clips retargeted to Manny.

`ASSETS.md` records the Sonniss GDC 2026 bundle as royalty-free, with licensing confirmed by me. Its 56 processed WAVs plus the audio report's three music outputs account for the 59-file total.

The music is **Boardwalk Jaunt — Sky Toes**, used for main/menu, and **Funk Formula — Nick Petrov**, used for final music, from Uppbeat. Credit is required. The recorded codes are `TBK6NUHLZIDMPKOK` and `ILNEZRJIJP1DSTBI`, respectively. The final credits need to retain the attribution recorded in [ASSETS.md](ASSETS.md).

My [pre-jam declaration](../PRE_JAM_DECLARATION.md) is committed at the repository root and belongs with the process record.

TODO(verify): Complete the empty AI-content/tool tables in ASSETS.md and confirm the submission wording for Mixamo and Manny.

## TODO(director): Saturday afternoon — L01 lake layout

Add the finished layout and camera/readability decisions, including what happened to the clouds and static heron planned for L01.

## TODO(director): T04 round loop

Record the actual fishing, banking, timer and win/lose loop, what got cut, and the playtest result.

## TODO(director): Sunday — polish

Add final feel and audio decisions, remaining bugs, and whether deferred wildlife made it in.

## TODO(director): Sunday — submission

Add the packaged-build check, final credits, submission links and the last keep/change/cut decisions.
