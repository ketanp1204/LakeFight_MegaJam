# T02 - Data-driven faults + diagnosis (Fri 13 Nov morning)

## Goal
Faults are data. The line can fail in 4 ways, each with its own symptoms, so the player must diagnose before fixing.

## In scope
- `ULDFaultDefinition` (PrimaryDataAsset): id, display name, symptoms (HMI code, beacon colour, sound cue,
  inspect text per component), fix requirement (which interactable + which action), wrong-fix consequence.
- `ULDFaultComponent` on line machines: can be faulted, exposes symptoms, validates fixes.
- `ALDLineDirector`: schedules faults from a list per shift; tracks downtime and units.
- Four faults:
  1. Conveyor jam (from T01, migrated to data).
  2. Tripped breaker: line dead, HMI "E-07 POWER". Fix: breaker in wall cabinet. Resetting while a jam
     is still present re-trips it.
  3. Dirty photo-eye: line runs but every box is rejected to the scrap chute. Fix: wipe the sensor.
  4. Low air: pusher arm moves slowly, boxes pile up. Fix: find hissing leak, close the valve.
- HMI panel: shows a code that is true but ambiguous (two faults can share it).

## Out of scope
Shifts/menus/art. No more than 4 fault types today.

## Acceptance (manual)
- [ ] Each fault is identifiable within 30 s by a first-time player using sound/light/inspect.
- [ ] A wrong fix has a visible consequence, not just nothing.
