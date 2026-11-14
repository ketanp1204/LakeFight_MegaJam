"""Read-only A03 beat analysis; writes diagnostic JSON only under Tools/audio/."""
import json
from pathlib import Path
import numpy as np


def onset_envelopes(audio, rate):
    mono = audio.mean(axis=1)
    hop, size = 240, 2048
    window = np.hanning(size)
    freq = np.fft.rfftfreq(size, 1 / rate)
    bands = [(40, 180), (180, 2000), (2000, 12000)]
    previous = np.zeros(size // 2 + 1)
    rows = []
    for start in range(0, len(mono) - size, hop):
        spectrum = np.log1p(100 * np.abs(np.fft.rfft(mono[start:start + size] * window)))
        flux = np.maximum(spectrum - previous, 0)
        rows.append([flux[(freq >= lo) & (freq < hi)].mean() for lo, hi in bands])
        previous = spectrum
    values = np.array(rows)
    values /= np.maximum(np.std(values, axis=0), 1e-12)
    return (np.arange(len(values)) * hop + size / 2) / rate, values


def detect_grid(audio, rate, settings):
    times, bands = onset_envelopes(audio, rate)
    lo, hi = settings['analysis_seconds']
    envelope = bands[:, 0] + .35 * bands[:, 1] + .15 * bands[:, 2]
    mask = (times >= lo) & (times < hi)
    # Autocorrelation supplies tempo candidates; long-span comb fitting refines
    # period/phase without assuming a BPM from the filename or external source.
    env = envelope[mask] - envelope[mask].mean()
    nfft = 1 << (2 * len(env) - 1).bit_length()
    spectrum = np.fft.rfft(env, nfft)
    corr = np.fft.irfft(spectrum * spectrum.conj(), nfft)[:len(env)]
    corr /= np.arange(len(env), 0, -1)
    lags = np.arange(int(60 / settings['bpm_max'] / .005), int(60 / settings['bpm_min'] / .005) + 1)
    peaks = lags[(corr[lags] >= corr[lags - 1]) & (corr[lags] >= corr[lags + 1])]
    best = sorted(peaks, key=lambda lag: -corr[lag])[:5]
    candidates = []
    for lag in best:
        for bpm in np.arange(60 / (lag * .005) - 1, 60 / (lag * .005) + 1, .01):
            beat = 60 / bpm
            for phase in np.arange(0, beat, .005):
                grid = np.arange(phase, hi, beat)
                grid = grid[grid >= lo]
                score = np.interp(grid, times, envelope).mean()
                candidates.append((float(score), float(bpm), float(phase)))
    score, bpm, phase = max(candidates)
    # Sub-millisecond phase and fine tempo refinement around the winner.
    fine = []
    for tempo in np.arange(bpm - .02, bpm + .0201, .0005):
        for offset in np.arange(phase - .006, phase + .0061, .0005):
            grid = np.arange(offset, hi, 60 / tempo)
            grid = grid[grid >= lo]
            fine.append((float(np.interp(grid, times, envelope).mean()), float(tempo), float(offset)))
    score, bpm, phase = max(fine)
    result = dict(bpm=round(bpm, 6), beat_phase_seconds=round(phase, 6),
                  analysis_seconds=[lo, hi], onset_hop_seconds=.005,
                  method='three-band positive log spectral flux; autocorrelation candidates; long-span beat comb refinement',
                  comb_score=score,
                  autocorrelation_candidates_bpm=[60 / (lag * .005) for lag in best])
    return result, times, bands


if __name__ == '__main__':
    import argparse
    from process_audio import Processor, HERE, ROOT, save_json, sha256
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default='funk-formula-nick-petrov-main-version-50121-02-27.mp3')
    parser.add_argument('--output', default='music_analysis.json')
    args = parser.parse_args()
    processor = Processor(HERE / 'audio_manifest.json')
    source = ROOT / 'Audio/Source/Music' / args.source
    audio, info, _ = processor.decode(source, 2)
    result, times, bands = detect_grid(audio, processor.rate, dict(analysis_seconds=[20, 120], bpm_min=80, bpm_max=150))
    result.update(source_sha256=sha256(source), source_info=info,
                  first_activity_seconds=float(np.flatnonzero(np.max(np.abs(audio), axis=1) > .01)[0] / processor.rate))
    # Strong onset positions and band strengths expose beat/bar grouping for review.
    env = bands[:, 0] + .35 * bands[:, 1] + .15 * bands[:, 2]
    peaks = np.flatnonzero((env[1:-1] > env[:-2]) & (env[1:-1] >= env[2:])) + 1
    result['strong_onsets'] = [[round(float(times[i]), 4), *np.round(bands[i], 3).tolist()]
                               for i in peaks if env[i] > 3 and times[i] < 24]
    bar = 240 / round(result['bpm'], 1)
    phase = result['beat_phase_seconds']
    result['bar_energy_dbfs'] = [[round(phase + b * bar, 6),
                                20 * float(np.log10(max(np.sqrt(np.mean(audio[round((phase + b * bar) * processor.rate):round((phase + (b + 1) * bar) * processor.rate)].astype(float) ** 2)), 1e-12)))]
                               for b in range(int((len(audio) / processor.rate - phase) / bar))]
    save_json(HERE / args.output, result)
    print(json.dumps({k:v for k,v in result.items() if k != 'strong_onsets'}, indent=2))
