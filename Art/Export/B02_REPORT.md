# B02 lake props

Completed all 11 exports; no asset failures or skipped assets.

| Asset | Tris | Limit (exclusive) | Status |
| --- | ---: | ---: | --- |
| SM_LF_Paddle | 156 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Dock | 788 | 800 | Two runs match; FBX + PNG present |
| SM_LF_Crate | 288 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Fish_A | 168 | 200 | Two runs match; FBX + PNG present |
| SM_LF_Fish_B | 168 | 200 | Two runs match; FBX + PNG present |
| SM_LF_Reeds | 356 | 400 | Two runs match; FBX + PNG present |
| SM_LF_Rock_01 | 38 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Rock_02 | 60 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Rock_03 | 68 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Buoy | 236 | 300 | Two runs match; FBX + PNG present |
| SM_LF_Shack | 1188 | 1500 | Two runs match; FBX + PNG present |

Each of the ten asset scripts ran from its saved file via exec(compile(open(path).read(), filename, 'exec')).
Commands also captured Blender solid viewport PNGs and checked the exports on disk.
One fish script builds both colour variants; each rock has its own script.
Original geometry only. lf_common.py unchanged. Added lf_b02.py for shared explicit geometry, validation and screenshots.
Files written: ten asset scripts plus lf_b02.py in Art/BlenderScripts; eleven FBX exports; eleven PNGs in _shots; eleven JSON reports in _reports; B02_manifest.json and this report.

## Human review

- Approve silhouettes and palette in the screenshots.
- Shack walls are 4 x 3 m; roof/fascia overall is 4.5 x 3.447 m.
- Dock deck is Z=0; posts extend down 1.5 m and up 0.65 m.
- Unreal import, collision and gameplay checks remain with the director/Unreal agent.
