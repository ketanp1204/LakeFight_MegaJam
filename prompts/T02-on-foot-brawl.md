# T02 - The cut features: on foot, paddle, overboard, swim, board, steal (Fri 13 Nov) - agent: Claude Code

These are the features the original Lake Fight deferred. They are the headline of the Director's Cut.

## Goal
The player is a character standing IN the boat. They can swing a paddle, knock people overboard, swim, climb into
any boat and drive it away.

## In scope
- `ALFBrawler` (Character): states InBoat-Driving / InBoat-Standing / Swimming / Climbing / Ragdoll-lite.
  While InBoat, the character is based on the boat (moves with it, sways with roll; no sliding off unless hit).
- Toggle drive/stand (one button). Driving uses T01 controls; standing frees the character to move inside the boat.
- Paddle swing (light, charged): sweep trace; hit = knockback impulse + short stun; strong hit or hit near the
  gunwale = falls overboard. Hitting a boat hull rocks the boat.
- Overboard: splash, Swimming state (slow, bobbing on `GetHeightAt`), can grab any boat's side within 1.5 m ->
  Climbing (0.8 s, interruptible by a paddle hit) -> InBoat.
- Stealing: if you climb into a boat with no one driving, you can drive it. If someone is driving, you must knock
  them out first.
- Jumping boat-to-boat when hulls are within 2.5 m.
- Placeholder anims are fine (mannequin + simple procedural lean). Comedy beats > fidelity.

## Out of scope
AI, fish/scoring, menus, final art/audio.

## Acceptance (manual)
- [ ] Knocking a dummy overboard is funny the first ten times.
- [ ] Swim -> climb -> drive away in someone else's boat takes under 10 s and feels like a heist.
- [ ] Standing in a rocking boat never launches the character into orbit.
