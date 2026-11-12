# T03 - AI rival captains (Fri 13 Nov afternoon) - agent: Claude Code

## Goal
Three AI rivals, each in their own boat, play the same game as the player: fish, ram, board, brawl, steal, flee.

## In scope
- `ALFCaptainAI` (AIController) driving an `ALFBrawler`, using the same inputs as the player (no cheating physics).
- Simple utility/state choice re-evaluated every 0.5 s: GoFish, ReturnToDock, Chase (target with most fish),
  Ram, Board, Fight, RecoverBoat (when swimming: nearest empty boat, else own boat), Flee.
- Personalities via a DataAsset: Bully (rams a lot), Thief (boards and steals), Fisher (avoids fights until robbed).
- Boat steering: steer toward a point with lookahead; avoid shore with 3 line traces; no navmesh on water.
- Debug: `lf.ai.debug 1` shows each AI's state above its head.

## Acceptance (manual)
- [ ] Watching 2 minutes of AI-vs-AI produces at least one steal and one man-overboard without the player.
- [ ] I can tell each personality apart without debug text.
