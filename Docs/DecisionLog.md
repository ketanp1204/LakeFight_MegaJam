# Decision log (human decisions only)

| When (IST) | Decision | Why |
| --- | --- | --- |
| 2026-11-13 11:50 | T01.3 lake: no visible toon facets (MI_LF_Lake FacetContrast 0, param kept) | Moving facet triangles read as "underwater jagged polygons" |
| 2026-11-13 11:50 | T01.3 lake: straight V-wake off (WakeOpacity 0); a real trailing wake is a later task | Rigid lines behind the stern read as a bug |
| 2026-11-13 21:30 | Standing paddle hold: nearly horizontal, centred on the chest (not the diagonal rowing-like hold) | The player isn't rowing while standing |
| 2026-11-13 21:30 | Sitting1 (director re-sourced) is the seated idle clip; hands must rest on, not inside, the thighs | The old Sitting clip was wrong; hands clipped into the thighs |
| 2026-11-13 21:40 | Paddle swing: arms follow the paddle arc (hit timing/reach unchanged) + procedural spine twist (+35 -> -45 deg, 20/30/50 %), forward pitch on the strike, lean into charged swings; no sword clip | The sword slash fought the IK and moved the arms wrong |
| 2026-11-13 22:30 | Charged punch: left hand lets go, right holds the charge pose, fires FASTER than a light punch; light punch keeps both hands on | Feel |
| 2026-11-13 22:45 | Hands at equal height, right overhand / left underhand palm grip; hold held out from the body, centred on the chest | Looked stiff / contracted |
| 2026-11-13 23:10 | Brawler size 0.75 -> 0.85 (try 0.9 if still small) | Player read small next to the boat |
| 2026-11-13 23:10 | Rowing uses two oars on the boat at the oarlocks (Codex adds SM_LF_Oar + Oarlock sockets); paddle stowed while driving; oars shipped when nobody drives | Single paddle can't match two-oar rowing |
| 2026-11-14 00:30 | Procedural rowing (torso + hand IK), both oars together; Wheelbarrow_Walk as the walk (made in-place in code) | No usable rowing clip; MF walk looked feminine |
| 2026-11-14 01:20 | Rower faces the bow (stern-facing tried and reverted) | W must move the boat where the player faces |
| 2026-11-14 01:20 | Boat camera never auto-recentres | Disliked the automatic reset |
