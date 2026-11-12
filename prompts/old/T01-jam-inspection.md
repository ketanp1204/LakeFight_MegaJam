# T01 - Jam Inspection P0 (Thu 12 Nov, 10:30-13:00 IST)

Copy into `Docs/CurrentTask.md` after the repo is initialised. Prepared pre-jam as a prompt; declared.

## Goal
In a greybox bay, a conveyor carries boxes. A box jams; the line stops with an alarm. The player walks over,
inspects the jam, pulls the box free and presses Restart on the panel. The line runs again.

## In scope
- Map `/Game/LineDown/Maps/L_Bay01`: floor, walls, one straight conveyor (~12 m), a control panel, a beacon light.
- `ALDConveyor`: moves boxes along a spline or fixed direction at a tunable speed; states Running / Stopped.
  Kinematic box movement while running (no physics fights); boxes become physics bodies when jammed.
- `ALDBoxSpawner`: spawns a box every N seconds at the start, destroys at the end, counts `UnitsProduced`.
- Jam trigger: after a tunable time, one box wedges at a marked point and the conveyor stops. Beacon on, alarm loop.
- Player (First Person template character): look-at highlight on interactables, `E` to interact.
  Inspect = short camera nudge + on-screen line "Box wedged against side guide".
  Grab = physics handle; drag the box off the belt.
- Panel `Restart` button: if the jam point is clear, line runs and alarm stops; if not, short buzzer, nothing else.
- HUD: units produced, downtime seconds (counts while stopped).

## Out of scope
Other faults, tools, menus, art, final audio, shifts/quota, save data.

## Acceptance (automated)
- [ ] Editor target compiles.
- [ ] PIE starts on `L_Bay01` with no errors in the Output Log.

## Acceptance (manual - human)
- [ ] Jam happens within ~20 s of start; I know something is wrong without reading text (sound + light).
- [ ] Pulling the box free feels physical, not like clicking a button.
- [ ] Restart while still jammed gives a clear "no"; restart when clear resumes the line.
- [ ] Downtime counter makes me want to hurry.

## Stop boundary
Stop when the above passes or at 12:45 IST, whichever comes first. Commit, report, write one DirectorsLog line.
