# A01 / A03 - audio processing

Backend: **FFmpeg**, with NumPy for deterministic sample operations and SoundFile for float intermediates.

- ffmpeg version 9.0-full_build-www.gyan.dev Copyright (c) 2000-2026 the FFmpeg developers
- Python 3.14.4; NumPy 2.5.4; SoundFile 0.14.0
- Dependencies isolated under Tools/audio/.venv; pinned in Tools/audio/requirements.lock.txt.
- Sonniss GDC2026 sources licensed/royalty-free as confirmed by the director. Current render source hashes verified unchanged before/after processing; prior A01 source evidence retained separately when rendering MUSIC only.
- No git or Unreal commands; deliverables under Audio/ and processing tooling under Tools/audio/.

## Outputs

| Output | Source file | Duration (s) | LUFS-I | True peak (dBTP) | Slices kept |
|---|---|---:|---:|---:|---|
| **AMBIENCE** | | | | | |
| [AMB_LF_Lake_Loop.wav](Processed/AMB_LF_Lake_Loop.wav) | WATRLap_Summer Tennessee Lake Dock Water Ripples Wake Wave Gentle 05 Distant_ESM_CPS.wav | 100.976 | -28.04 | -1.10 | single |
| [AMB_LF_Shore_Loop.wav](Processed/AMB_LF_Shore_Loop.wav) | WATRLap_Small Waves sloshing onto Rock Slabs at huge Glacial Lake _JSE_HoN_Quad.wav | 89.955 | -28.00 | -2.77 | single |
| [AMB_LF_Meadow_Loop.wav](Processed/AMB_LF_Meadow_Loop.wav) | AMBSwmp_Meadow Pipits calling many Insects humming Wind blowing through Grass_JSE_HoN_Stereo.wav | 84.496 | -28.00 | -10.50 | single |
| [AMB_LF_Birds_Loop.wav](Processed/AMB_LF_Birds_Loop.wav) | AMBSubn_Ambience, Forest Crickets, Birds, Connecticut 02_344 Audio_East Coast America.wav | 50.302 | -28.00 | -12.49 | single |
| **SPLASH** | | | | | |
| [SFX_LF_Overboard_01.wav](Processed/SFX_LF_Overboard_01.wav) | WATRImpt_Impact Water Deep Submerge Bubble Drown Ship Hit 05_ESM_EMWI.wav | 1.770 | -16.03 | -1.20 | single |
| [SFX_LF_Surface_01.wav](Processed/SFX_LF_Surface_01.wav) | Cartoon Bubbles Short.wav | 0.500 | -16.04 | -1.23 | single |
| **PADDLE** | | | | | |
| [SFX_LF_PaddleHit_01.wav](Processed/SFX_LF_PaddleHit_01.wav) | FGHTImpt_Combat Punch Impact Light Hit Delay Crunchy Vintage Quick Smack 05_ESM_AG.wav | 0.775 | -16.04 | -1.20 | single |
| [SFX_LF_PaddleWood_01.wav](Processed/SFX_LF_PaddleWood_01.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.215 | -16.04 | -1.16 | 6/16 |
| [SFX_LF_PaddleWood_02.wav](Processed/SFX_LF_PaddleWood_02.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.295 | -16.20 | -1.19 | 6/16 |
| [SFX_LF_PaddleWood_03.wav](Processed/SFX_LF_PaddleWood_03.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.330 | -16.16 | -1.18 | 6/16 |
| [SFX_LF_PaddleWood_04.wav](Processed/SFX_LF_PaddleWood_04.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.255 | -16.12 | -1.18 | 6/16 |
| [SFX_LF_PaddleWood_05.wav](Processed/SFX_LF_PaddleWood_05.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.220 | -16.17 | -1.20 | 6/16 |
| [SFX_LF_PaddleWood_06.wav](Processed/SFX_LF_PaddleWood_06.wav) | WEAPBlnt_Spear And Stick Impact, Wooden MKH 2_344 Audio_Medieval Weapons Vol 2.wav | 0.375 | -16.17 | -1.19 | 6/16 |
| [SFX_LF_PaddleCharged_01.wav](Processed/SFX_LF_PaddleCharged_01.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.735 | -16.03 | -1.18 | 6/22 |
| [SFX_LF_PaddleCharged_02.wav](Processed/SFX_LF_PaddleCharged_02.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.135 | -16.01 | -1.44 | 6/22 |
| [SFX_LF_PaddleCharged_03.wav](Processed/SFX_LF_PaddleCharged_03.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.655 | -16.14 | -1.18 | 6/22 |
| [SFX_LF_PaddleCharged_04.wav](Processed/SFX_LF_PaddleCharged_04.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.640 | -16.12 | -1.19 | 6/22 |
| [SFX_LF_PaddleCharged_05.wav](Processed/SFX_LF_PaddleCharged_05.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.855 | -15.89 | -1.16 | 6/22 |
| [SFX_LF_PaddleCharged_06.wav](Processed/SFX_LF_PaddleCharged_06.wav) | SWSH_SWING IMPACTS Quick Heavy Weapon Swing To Thud Impact Var 01_DDUMAIS_MWP2.wav | 0.670 | -16.15 | -1.19 | 6/22 |
| [SFX_LF_Stun_01.wav](Processed/SFX_LF_Stun_01.wav) | Cartoon Confused Dizzy.wav | 0.795 | -16.01 | -6.65 | single |
| [SFX_LF_HitCreak_01.wav](Processed/SFX_LF_HitCreak_01.wav) | Cartoon Airy Impact Creaks.wav | 1.620 | -16.13 | -1.20 | single |
| **RAM** | | | | | |
| [SFX_LF_RamWood_01.wav](Processed/SFX_LF_RamWood_01.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 0.790 | -15.95 | -1.20 | 6/32 |
| [SFX_LF_RamWood_02.wav](Processed/SFX_LF_RamWood_02.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 0.723 | -16.14 | -1.20 | 6/32 |
| [SFX_LF_RamWood_03.wav](Processed/SFX_LF_RamWood_03.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 0.883 | -15.95 | -1.20 | 6/32 |
| [SFX_LF_RamWood_04.wav](Processed/SFX_LF_RamWood_04.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 0.151 | -16.17 | -1.19 | 6/32 |
| [SFX_LF_RamWood_05.wav](Processed/SFX_LF_RamWood_05.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 1.198 | -16.14 | -1.19 | 6/32 |
| [SFX_LF_RamWood_06.wav](Processed/SFX_LF_RamWood_06.wav) | WOODImpt_Wooden Hit, Dark, Heavy Hit, Vampire's Prison_344 Audio_Haunting Ambiences Vol 3.wav | 0.126 | -16.16 | -1.19 | 6/32 |
| [SFX_LF_RamBass_01.wav](Processed/SFX_LF_RamBass_01.wav) | DSGNBass_Bass Drop & Downer Fast 16_344 Audio_Bass Drops & Downers.wav | 1.985 | -16.01 | -4.86 | single |
| [SFX_LF_BoatCreak_01.wav](Processed/SFX_LF_BoatCreak_01.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.105 | -15.97 | -1.20 | 6/7 |
| [SFX_LF_BoatCreak_02.wav](Processed/SFX_LF_BoatCreak_02.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.165 | -15.88 | -1.20 | 6/7 |
| [SFX_LF_BoatCreak_03.wav](Processed/SFX_LF_BoatCreak_03.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.225 | -15.82 | -1.20 | 6/7 |
| [SFX_LF_BoatCreak_04.wav](Processed/SFX_LF_BoatCreak_04.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.205 | -15.98 | -1.20 | 6/7 |
| [SFX_LF_BoatCreak_05.wav](Processed/SFX_LF_BoatCreak_05.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.100 | -16.05 | -1.20 | 6/7 |
| [SFX_LF_BoatCreak_06.wav](Processed/SFX_LF_BoatCreak_06.wav) | FOLYProp_Sitting Down, Creaky Seat, Squeaks 3_344 Audio_CInema Experience.wav | 0.140 | -16.02 | -1.20 | 6/7 |
| **VOICE** | | | | | |
| [VO_LF_Attack_01.wav](Processed/VO_LF_Attack_01.wav) | VOXMale_Warrior Opponent Attack, 'Ai yah' 2_344 Audio_Anime Fight Voices.wav | 0.805 | -18.02 | -2.38 | single |
| [VO_LF_Yell_01.wav](Processed/VO_LF_Yell_01.wav) | VOXMale_Male Adventurer, Active Yell 19_344 Audio_Anime Fight Voices.wav | 0.335 | -18.00 | -3.59 | single |
| [VO_LF_Pant_01.wav](Processed/VO_LF_Pant_01.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.455 | -18.05 | -5.49 | 6/12 |
| [VO_LF_Pant_02.wav](Processed/VO_LF_Pant_02.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.400 | -18.05 | -5.18 | 6/12 |
| [VO_LF_Pant_03.wav](Processed/VO_LF_Pant_03.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.395 | -18.01 | -5.22 | 6/12 |
| [VO_LF_Pant_04.wav](Processed/VO_LF_Pant_04.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.440 | -18.05 | -5.83 | 6/12 |
| [VO_LF_Pant_05.wav](Processed/VO_LF_Pant_05.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.515 | -18.01 | -3.37 | 6/12 |
| [VO_LF_Pant_06.wav](Processed/VO_LF_Pant_06.wav) | HMNBrth_Panting Male 02 04_SNDBTS_VH.wav | 0.390 | -18.01 | -2.99 | 6/12 |
| [VO_LF_Whistle_01.wav](Processed/VO_LF_Whistle_01.wav) | WHSTHmn_Whistle Male 06 09_SNDBTS_VH.wav | 2.325 | -18.01 | -11.58 | single |
| [VO_LF_Taunt_01.wav](Processed/VO_LF_Taunt_01.wav) | VOXLaff_Police Officer Laugh Vocal Male 1.wav | 1.585 | -18.00 | -5.65 | single |
| **UI / SCORE** | | | | | |
| [UI_LF_FishBanked.wav](Processed/UI_LF_FishBanked.wav) | Ting Coins.wav | 0.575 | -20.05 | -3.62 | single |
| [UI_LF_Accept.wav](Processed/UI_LF_Accept.wav) | Accept Boing Crunch.wav | 0.795 | -20.01 | -8.75 | single |
| [UI_LF_Deny.wav](Processed/UI_LF_Deny.wav) | Deny Muted.wav | 0.460 | -20.05 | -15.88 | single |
| [UI_LF_Win.wav](Processed/UI_LF_Win.wav) | Cofetti Whoosh Pluck Spill.wav | 1.805 | -19.84 | -1.20 | single |
| [UI_LF_RoundStart.wav](Processed/UI_LF_RoundStart.wav) | Game Entry Happy Short.wav | 0.355 | -20.02 | -10.29 | single |
| [UI_LF_FishCaught.wav](Processed/UI_LF_FishCaught.wav) | Foley Coin Flip Single Fast.wav | 0.515 | -20.02 | -1.30 | single |
| [UI_LF_GoldFish.wav](Processed/UI_LF_GoldFish.wav) | DSGNStngr_Power Up Bright Positive Successful Light Saturation Crash Shimmer 05_ESM_AG.wav | 1.205 | -19.99 | -5.21 | single |
| [UI_LF_GoldFishSparkle.wav](Processed/UI_LF_GoldFishSparkle.wav) | MAGMisc_Magic Christmas Bells 2_344 Audio_Christmas.wav | 16.145 | -20.01 | -2.58 | single |
| [UI_LF_Final30_Loop.wav](Processed/UI_LF_Final30_Loop.wav) | CLOCKTick_You're Running Late _344 Audio_Antique Clocks.wav | 58.000 | -20.07 | -1.12 | single |
| [UI_LF_MenuSelect.wav](Processed/UI_LF_MenuSelect.wav) | UIMisc_Kalimba 3 Up_CB Sounddesign_APPlicable Sounds.wav | 0.360 | -20.01 | -3.79 | single |
| [UI_LF_MenuMove.wav](Processed/UI_LF_MenuMove.wav) | Interface Plucks Happy.wav | 0.650 | -20.00 | -10.49 | single |
| [UI_LF_EndCheer.wav](Processed/UI_LF_EndCheer.wav) | CRWDCheer_Small Club, 50 People, Crowd, Spanish, Cheering and Applause-Surround_KSL_KS015.wav | 4.000 | -20.00 | -5.16 | single |
| **MUSIC** | | | | | |
| [MUS_LF_Main_Loop.wav](Processed/MUS_LF_Main_Loop.wav) | boardwalk-jaunt-sky-toes-main-version-35005-02-28.mp3 | 17.962 | -22.01 | -11.91 | single |
| [MUS_LF_Menu.wav](Processed/MUS_LF_Menu.wav) | boardwalk-jaunt-sky-toes-main-version-35005-02-28.mp3 | 8.981 | -24.00 | -11.61 | single |
| [MUS_LF_Final_Loop.wav](Processed/MUS_LF_Final_Loop.wav) | funk-formula-nick-petrov-main-version-50121-02-27.mp3 | 17.455 | -21.00 | -9.69 | single |

## Processing choices

- All WAVs are PCM16 / 48kHz. Splash/paddle/ram/voice are mono; ambience/UI are stereo. Quad shore recordings use equal front/rear stereo blends; 5.1 crowd uses L/R+centre/rears with LFE omitted. Mono sparkle input is duplicated to stereo.
- Targets: one-shots -16 LUFS, voices -18, UI -20, ambience -28. True-peak ceiling -1 dBTP; loudnorm uses -1.1 dBTP working headroom. If a short/high-crest event remains below target, gain is calibrated through a 4x-oversampled, 5ms-lookahead/50ms-release limiter at -1.2dBFS. Every gain trial starts from the original prepared float signal, so peak reduction is not stacked. The final PCM16 is remeasured; integrated tolerance is +/-0.20 LU.
- Silence split starts at -40dBFS, 120ms gap, 5ms blocks, 10ms pre-roll/40ms post-roll; slices shorter than 80ms after any pitch-rate change are dropped. Selection begins with the strongest event then greedily maximizes distance in standardized log spectral-centroid/peak space, keeping at most six; numbering follows source chronology.
- Non-loop clips are trimmed and faded 5ms in / 20ms out. EndCheer selects the most energetic continuous four-second window, with 50ms in / 700ms out for a natural ending.
- RamWood +3 semitones uses natural resampling (rate factor 2^(3/12)); decay duration becomes 0.8409 of its source slice. No time stretch or added sustain.
- A01 loops equal-power crossfade two seconds of tail into head and remove that overlap from length. Music uses the bar-aligned assembly documented below. The continuous result is rotated to a quiet stereo-mid zero crossing, then a 64-sample guard anchors both channels to digital zero. RIFF smpl metadata uses loop start 0 and inclusive end N-1. Exact points are in _processing.json; audition players loop those complete WAVs.
- A sub-400ms one-shot has insufficient duration for ordinary 400ms integrated gating. Only the metering copy is repeated for whole cycles to at least 1.2s; the delivered file keeps its original trimmed duration. This avoids normalizing a tiny transient against invented silence. Rows use that documented short-event integrated convention; ordinary files use native-duration metering.
- PCM16 TPDF dither is seeded by manifest seed plus output name. Source hashes, tool versions, processing settings and selection rules are pinned; no timestamp metadata is written into WAVs.

Short-event metering convention applied to: SFX_LF_PaddleWood_01.wav, SFX_LF_PaddleWood_02.wav, SFX_LF_PaddleWood_03.wav, SFX_LF_PaddleWood_04.wav, SFX_LF_PaddleWood_05.wav, SFX_LF_PaddleWood_06.wav, SFX_LF_PaddleCharged_02.wav, SFX_LF_RamWood_04.wav, SFX_LF_RamWood_06.wav, SFX_LF_BoatCreak_01.wav, SFX_LF_BoatCreak_02.wav, SFX_LF_BoatCreak_03.wav, SFX_LF_BoatCreak_04.wav, SFX_LF_BoatCreak_05.wav, SFX_LF_BoatCreak_06.wav, VO_LF_Yell_01.wav, VO_LF_Pant_03.wav, VO_LF_Pant_06.wav, UI_LF_RoundStart.wav, UI_LF_MenuSelect.wav.

Police laugh loudness check: prepared source -14.34 LUFS -> delivered -18.00 LUFS, true peak -5.65 dBTP. The notably louder source was attenuated to the voice target.

## A03 - music

Sources: Boardwalk Jaunt by Sky Toes (Main/Menu) and Funk Formula by Nick Petrov (Final), Uppbeat Basic Licenses (director supplied); credit required, exact credits recorded in Docs/ASSETS.md. Source MP3s are read-only and must not be redistributed. No source links or copies are embedded in the audition page.

| Output | Source seconds used | BPM | Source / playback bars (4/4) | Duration (s) | LUFS-I | dBTP |
|---|---|---:|---:|---:|---:|---:|
| MUS_LF_Main_Loop.wav | 89.859146-110.066208 | 106.893 | 9 / 8 | 17.961833 | -22.01 | -11.91 |
| MUS_LF_Menu.wav | 2.295229-13.521375 | 106.893 | 5 / 4 | 8.980917 | -24.00 | -11.61 |
| MUS_LF_Final_Loop.wav | 98.727271-118.363625 | 110.000 | 9 / 8 | 17.454542 | -21.00 | -9.69 |

boardwalk-jaunt-sky-toes-main-version-35005-02-28.mp3: detected 106.893357 BPM over 20-120s using three-band positive log spectral flux; autocorrelation candidates; long-span beat comb refinement; grid 106.893357 BPM, 4/4, first downbeat 0.050000s. Flux phase 0.045500s agrees within 4.50ms. Bar grouping is inferred from the opening and repeating arrangement; director audition remains required.

funk-formula-nick-petrov-main-version-50121-02-27.mp3: detected 109.999243 BPM over 20-120s using three-band positive log spectral flux; autocorrelation candidates; long-span beat comb refinement; grid 110.000000 BPM, 4/4, first downbeat 0.545455s. Flux phase 0.539500s agrees within 5.96ms. Bar grouping is inferred from the opening and repeating arrangement; director audition remains required.

Music candidates begin/end on bar lines. One extra source bar supplies the one-bar equal-power overlap: N+1 source bars become exactly N playback bars (rounded once to the nearest sample). Boardwalk is scanned across the track for its strongest/quietest sustained passages by mean RMS power; within 0.15dB of the best energy, tail/head spectral distance and waveform correlation select the cleaner match. Per-bar energy floors reject dips. Funk Formula candidates stay in 70-120s and reject a bar more than 3dB below the median, excluding the brief approximately 87-89s dip. No intro, breakdown or outro enters Final.

Final loop rotation is restricted to a stereo-mid zero crossing within 2ms of the bar line, preserving duration. The A01 64-sample guards anchor both channels to exact zero; this is a stereo-mid crossing plus digital-zero endpoints, not a claim of simultaneous natural L/R crossings. Crossfade, rotation, grid, per-bar source energy and all candidate scores are in _processing.json.

Targets: main -22 LUFS-I, menu -24 LUFS-I, final -21 LUFS-I; ceiling -1 dBTP. Human check: listen through several repetitions of each MUSIC player for rhythmic/harmonic closure and crossfade swell; approve menu calmness, steady fight energy and final climax. Boardwalk is a live-feeling performance: measured local tempo 106.889-106.908 BPM supports the average grid, but phrase closure still needs an ear. Numeric evidence does not establish audible seamlessness or in-game mix acceptance.

A03 files: Tools/audio/process_audio.py, analyze_music.py, audio_manifest.json; Audio/Processed/MUS_LF_Main_Loop.wav, MUS_LF_Menu.wav, MUS_LF_Final_Loop.wav, _listen.html, _processing.json, _reproducibility.json, _validation.json; Audio/AUDIO_REPORT.md. Local environment/cache and diagnostic evidence stay under Tools/audio/.

Director log draft (write restriction): A03 asked, then updated to two sources / three music loops and manifest pipeline delivered / awaiting director audition. No Docs files edited.

## Verification

- Outputs: 59; source mappings: 34; source inventory hashes protected: 2.
- 40 mono / 19 stereo; 8 loops. Largest target error 0.20 LU; highest true peak -1.10 dBTP.
- Every rendered WAV was remeasured for LUFS and true peak after PCM16 quantization. Format, channel count, sample rate, loop endpoints and RIFF metadata are checked.
- Two independent processing passes completed for the selected render group (MUSIC); selected WAV hashes match byte-for-byte. Earlier A01 evidence is retained for untouched outputs.
- Evidence: Processed/_processing.json and Processed/_reproducibility.json. Selected passes decode and process sources again; there is no rendered-audio cache.

## Run / audition

    Tools/audio/.venv/Scripts/python.exe -B Tools/audio/process_audio.py --group MUSIC --verify-repro

Manifest: Tools/audio/audio_manifest.json. Open [Processed/_listen.html](Processed/_listen.html) in a browser to audition the grouped outputs; no server is required.

## Files / human check / stop

Tooling: Tools/audio/process_audio.py, audio_manifest.json, requirements.lock.txt, isolated .venv and scratch .work/.tmp. Deliverables: Audio/Processed/*.wav, _listen.html, _processing.json, _reproducibility.json and this report.
Audition impact/voice choices, loop seams and relative levels in the provided page. Numeric normalization and reproducibility do not establish final artistic acceptance or in-game mix. No import, attenuation, concurrency, SoundCue, game code or other audio work was started.
The unused Laughing Female source remains untouched; it was not substituted for the requested Police Officer laugh. The two initially absent requested sources were added by the director before processing.
Director log draft (kept here under the write restriction): A01 asked / manifest pipeline, processed audio and audition page delivered / artistic acceptance pending director audition.
