#!/usr/bin/env python3
"""A01/A03 deterministic, manifest-driven audio processing. Sources are read-only.

Run with Tools/audio/.venv/Scripts/python.exe Tools/audio/process_audio.py
     --verify-repro

FFmpeg is the selected audio backend; NumPy handles segmentation, spectral
selection, fades and loop assembly. Final WAVs are written as deterministic
PCM16 RIFF, with a smpl loop chunk for loops. Source files are never written.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import wave
from html.parser import HTMLParser

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
try:
    import numpy as np
    import soundfile as sf
except ModuleNotFoundError:
    local_python = HERE / ".venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if local_python.exists() and Path(sys.executable).resolve() != local_python.resolve():
        os.execv(str(local_python), [str(local_python), "-B", str(Path(__file__).resolve()), *sys.argv[1:]])
    raise SystemExit("Install Tools/audio/requirements.lock.txt into Tools/audio/.venv first.")
GROUPS = ["AMBIENCE", "SPLASH", "PADDLE", "RAM", "VOICE", "UI / SCORE", "MUSIC"]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def save_json(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def inside(path: Path, parent: Path) -> Path:
    path = path.resolve()
    if not path.is_relative_to(parent.resolve()):
        raise ValueError(f"Path outside {parent}: {path}")
    return path


def db(value: float) -> float:
    return 20 * math.log10(max(float(value), 1e-12))


class Processor:
    def __init__(self, manifest: Path):
        self.manifest_path = manifest.resolve()
        self.config = json.loads(manifest.read_text(encoding="utf-8"))
        self.rate = self.config["format"]["sample_rate"]
        self.peak_ceiling = self.config["normalization"]["target_true_peak_dbtp"]
        self.working_peak = self.config["normalization"]["working_true_peak_dbtp"]
        self.source_root = inside(ROOT / self.config["source_root"], ROOT / "Audio" / "Source")
        self.out = inside(ROOT / self.config["output_root"], ROOT / "Audio")
        self.work = inside(ROOT / self.config["work_root"], HERE)
        self.out.mkdir(parents=True, exist_ok=True)
        self.work.mkdir(parents=True, exist_ok=True)
        self.ffmpeg = shutil.which("ffmpeg")
        self.ffprobe = shutil.which("ffprobe")
        if not self.ffmpeg or not self.ffprobe:
            raise RuntimeError("This manifest selects FFmpeg. Install the recorded FFmpeg version and retry.")
        self.environment = {
            "backend": "ffmpeg",
            "ffmpeg": self.run([self.ffmpeg, "-version"]).stdout.decode().splitlines()[0],
            "numpy": np.__version__, "soundfile": sf.__version__,
            "python": ".".join(map(str, sys.version_info[:3])),
        }
        self.music_grids = {}
        expected = self.config["environment"]
        for key in ["ffmpeg", "numpy", "soundfile"]:
            if self.environment[key] != expected[key]:
                raise RuntimeError(f"Version mismatch for reproducible render: {key}: {self.environment[key]} != {expected[key]}")

    @staticmethod
    def run(args, input_bytes=None):
        result = subprocess.run(args, input=input_bytes, capture_output=True)
        if result.returncode:
            raise RuntimeError(result.stderr.decode("utf-8", errors="replace"))
        return result

    def command(self):
        return [self.ffmpeg, "-hide_banner", "-nostdin", "-threads", "1", "-filter_threads", "1"]

    def probe(self, path):
        data = self.run([self.ffprobe, "-v", "error", "-select_streams", "a:0",
                         "-show_entries", "stream=sample_rate,channels,channel_layout,codec_name,bits_per_sample:format=duration",
                         "-of", "json", str(path)])
        return json.loads(data.stdout)

    def verify_sources(self, group=None):
        inventory = {}
        roots = [(self.source_root, self.config["source_inventory"], "")]
        if "music" in self.config:
            roots.append((inside(ROOT / self.config["music"]["source_root"], ROOT / "Audio/Source"),
                          self.config["music"]["source_inventory"], "Music/"))
        for root, files, prefix in roots:
            for name, expected in sorted(files.items()):
                if group and not any(e['group'] == group and e['source'] == name for e in self.config['events']):
                    continue
                path = inside(root / name, root)
                actual = sha256(path)
                if actual != expected["sha256"]:
                    raise RuntimeError(f"Read-only source changed: {prefix}{name}")
                inventory[prefix + name] = {"sha256": actual, "bytes": path.stat().st_size}
        return inventory

    def process_music(self, entry):
        from analyze_music import detect_grid
        settings = self.config['music']['sources'][entry['source']]
        root = inside(ROOT / self.config['music']['source_root'], ROOT / 'Audio/Source')
        audio, info, pan = self.decode(inside(root / entry['source'], root), 2)
        if entry['source'] not in self.music_grids:
            detected, _, _ = detect_grid(audio, self.rate, settings['beat_analysis'])
            bpm = settings['grid_bpm']
            assert abs(detected['bpm'] - bpm) < .02, 'Measured tempo no longer matches approved grid'
            phase_error = (detected['beat_phase_seconds'] - settings['downbeat_seconds'] + 30 / bpm) % (60 / bpm) - 30 / bpm
            assert abs(phase_error) < .012, 'Measured beat phase no longer matches grid'
            self.music_grids[entry['source']] = dict(detected, grid_bpm=bpm, downbeat_seconds=settings['downbeat_seconds'],
                                   beats_per_bar=settings['beats_per_bar'], phase_error_seconds=phase_error)
        bar = settings['beats_per_bar'] * 60 / settings['grid_bpm']
        origin = settings['downbeat_seconds']
        bars = entry['bars']
        count = round(bar * self.rate)
        length = round(bar * bars * self.rate)
        lo, hi = entry['source_window_seconds']
        candidates = []
        for index in range(math.ceil((lo - origin) / bar), math.floor((hi - origin) / bar - bars - 1) + 1):
            begin = round((origin + index * bar) * self.rate)
            # One extra source bar is consumed by the overlap; playback stays N bars.
            segment = audio[begin:begin + length + count]
            energy = [db(np.sqrt(np.mean(segment[round(i * bar * self.rate):round((i + 1) * bar * self.rate)].astype(float) ** 2)))
                      for i in range(bars + 1)]
            drop = float(np.median(energy) - min(energy))
            head, tail = segment[:count].astype(float), segment[-count:].astype(float)
            correlation = float(np.sum(head * tail) / max(np.sqrt(np.sum(head ** 2) * np.sum(tail ** 2)), 1e-20))
            # Compare spectral envelopes over the bar; match arrangement/timbre as
            # well as waveform. A hard energy floor rejects a hidden breakdown.
            spectra = []
            for part in (head, tail):
                frames = part[:len(part) // 1024 * 1024].mean(axis=1).reshape(-1, 1024)
                spectra.append(np.log1p(np.abs(np.fft.rfft(frames * np.hanning(1024))).mean(axis=0)))
            distance = float(np.sqrt(np.mean((spectra[0] - spectra[1]) ** 2)))
            candidates.append(dict(start_bar_index=index, start_frame=begin, energy_dbfs=energy,
                                   mean_energy_dbfs=db(np.sqrt(np.mean(segment.astype(float) ** 2))),
                                   energy_drop_db=drop, correlation=correlation, spectral_distance=distance,
                                   accepted=drop <= entry['maximum_bar_drop_db'], score=distance + .1 * (1 - correlation)))
        valid = [c for c in candidates if c['accepted']]
        if not valid:
            raise ValueError(f"No steady whole-bar passage for {entry['id']}")
        if entry.get('selection_mode') in ('energetic', 'calm'):
            energies = [c['mean_energy_dbfs'] for c in valid]
            best = max(energies) if entry['selection_mode'] == 'energetic' else min(energies)
            valid = [c for c in valid if abs(c['mean_energy_dbfs'] - best) <= entry['energy_tie_db']]
        chosen = min(valid, key=lambda c: (c['score'], c['start_bar_index']))
        begin = chosen['start_frame']
        end = begin + length + count
        segment = audio[begin:end]
        theta = np.linspace(0, math.pi / 2, count)
        blend = segment[-count:] * np.cos(theta)[:, None] + segment[:count] * np.sin(theta)[:, None]
        result = np.concatenate([segment[count:-count], blend]).astype(np.float32)
        assert len(result) == length
        # Circular rotation preserves exact whole-bar duration. Restrict to 2ms
        # around the bar line instead of A01's unrestricted two-second search.
        mid = result.mean(axis=1)
        choices = np.flatnonzero(mid * np.roll(mid, 1) <= 0)
        choices = choices[np.minimum(choices, len(result) - choices) <= round(.002 * self.rate)]
        if not len(choices):
            raise ValueError('No mid zero crossing within 2ms of the music bar line')
        score = np.sum(result[choices] ** 2 + result[(choices - 1) % len(result)] ** 2, axis=1)
        rotation = int(choices[np.argmin(score)])
        result = np.roll(result, -rotation, axis=0)
        metrics = self.normalize(result, entry['output'], entry['target_lufs'], True)
        loop = dict(crossfade_seconds=count / self.rate, crossfade_bars=1,
                    crossfade_curve='equal-power cosine/sine tail into head', bars=bars, source_bars=bars + 1,
                    rotation_samples=rotation if rotation < length / 2 else rotation - length,
                    zero_crossing_channel='stereo mid; minimum joint L/R energy within 2ms of bar line',
                    boundary_guard_samples=self.config['loops']['boundary_guard_samples'],
                    start_sample=0, end_sample_inclusive=length - 1, wav_smpl_chunk=True, endpoint_pcm_frames_zero=True)
        output = dict(output=entry['output'] + '.wav', source=entry['source'], event=entry['id'], group='MUSIC',
                      channels=2, sample_rate=self.rate, bits_per_sample=16, duration_seconds=length / self.rate,
                      source_start_seconds=begin / self.rate, source_end_seconds=end / self.rate,
                      target_lufs=entry['target_lufs'], bpm=settings['grid_bpm'], beat_grid=self.music_grids[entry['source']],
                      loop=loop, downmix=pan, slices_kept=None, candidates=None, **metrics)
        event = dict(id=entry['id'], source=entry['source'], source_info=info,
                     selection_reason=entry['selection_reason'], candidates=candidates, selected=chosen,
                     selected_source_ranges_seconds=[[begin / self.rate, end / self.rate]])
        return [output], event

    def decode(self, path, channels):
        info = self.probe(path)
        original = info["streams"][0]["channels"]
        if original == channels:
            pan = None
        elif original == 1 and channels == 2:
            pan = "pan=stereo|c0=c0|c1=c0"
        elif original == 2 and channels == 1:
            pan = "pan=mono|c0=0.5*c0+0.5*c1"
        elif original == 4 and channels == 2:
            pan = "pan=stereo|c0=0.5*c0+0.5*c2|c1=0.5*c1+0.5*c3"
        elif original == 6 and channels == 2:
            # Preserve FL/FR and centre/rears; omit LFE for the UI crowd bed.
            pan = "pan=stereo|c0=0.414*c0+0.293*c2+0.293*c4|c1=0.414*c1+0.293*c2+0.293*c5"
        else:
            raise ValueError(f"Unspecified channel conversion {original}->{channels}")
        args = self.command() + ["-loglevel", "error", "-i", str(path), "-map", "0:a:0"]
        if pan:
            args += ["-af", pan]
        args += ["-ac", str(channels), "-ar", str(self.rate), "-c:a", "pcm_f32le", "-f", "f32le", "-"]
        raw = self.run(args).stdout
        audio = np.frombuffer(raw, dtype="<f4").reshape(-1, channels).copy()
        if not len(audio) or not np.isfinite(audio).all():
            raise ValueError(f"Empty/nonfinite source {path}")
        return audio, info, pan or "unchanged"

    def detection(self, audio, entry):
        settings = dict(self.config["slicing"], **entry.get("slicing", {}))
        block = round(self.rate * settings["analysis_block_ms"] / 1000)
        amplitude = np.max(np.abs(audio), axis=1)
        padding = (-len(amplitude)) % block
        peaks = np.max(np.pad(amplitude, (0, padding)).reshape(-1, block), axis=1)
        threshold = settings["threshold_dbfs"]
        # Quiet library recordings retain events instead of returning no audio.
        if db(amplitude.max()) < threshold + 6:
            threshold = db(amplitude.max()) - 30
        active = np.flatnonzero(peaks >= 10 ** (threshold / 20))
        if not len(active):
            raise ValueError(f"No active signal in {entry['id']}")
        gap_blocks = math.ceil(settings["min_gap_ms"] / settings["analysis_block_ms"])
        breaks = np.flatnonzero(np.diff(active) > gap_blocks)
        begins = np.r_[active[0], active[breaks + 1]]
        ends = np.r_[active[breaks], active[-1]] + 1
        pre = round(self.rate * settings["pre_roll_ms"] / 1000)
        post = round(self.rate * settings["post_roll_ms"] / 1000)
        regions = [(max(0, int(a * block - pre)), min(len(audio), int(b * block + post)))
                   for a, b in zip(begins, ends)]
        return regions, threshold, settings

    def features(self, audio):
        mono = np.mean(audio, axis=1)
        nfft = 2048
        if len(mono) < nfft:
            mono = np.pad(mono, (0, nfft - len(mono)))
        starts = np.linspace(0, len(mono) - nfft, min(10, max(1, len(mono) // nfft)), dtype=int)
        power = np.zeros(nfft // 2 + 1)
        for start in starts:
            power += np.abs(np.fft.rfft(mono[start:start + nfft] * np.hanning(nfft))) ** 2
        centroid = float(np.dot(power, np.fft.rfftfreq(nfft, 1 / self.rate)) / max(float(power.sum()), 1e-20))
        return {"centroid_hz": centroid, "peak_dbfs": db(np.max(np.abs(audio))),
                "rms_dbfs": db(np.sqrt(np.mean(audio.astype(np.float64) ** 2)))}

    @staticmethod
    def distinct(candidates, maximum):
        if len(candidates) <= maximum:
            return list(range(len(candidates)))
        features = np.array([[math.log2(max(c["centroid_hz"], 1)), c["peak_dbfs"]] for c in candidates])
        features = (features - features.mean(axis=0)) / np.maximum(features.std(axis=0), 1e-9)
        first = max(range(len(candidates)), key=lambda i: (candidates[i]["peak_dbfs"], -i))
        kept = [first]
        while len(kept) < maximum:
            distances = np.min(np.sum((features[:, None] - features[kept][None, :]) ** 2, axis=2), axis=1)
            distances[kept] = -1
            kept.append(int(np.argmax(distances)))
        return sorted(kept)  # Deterministic output numbering follows source time.

    def fade(self, audio, fade_in_ms, fade_out_ms):
        audio = audio.copy()
        ni = min(len(audio), round(self.rate * fade_in_ms / 1000))
        no = min(len(audio), round(self.rate * fade_out_ms / 1000))
        if ni:
            audio[:ni] *= np.linspace(0, 1, ni)[:, None]
        if no:
            audio[-no:] *= np.linspace(1, 0, no)[:, None]
        return audio

    def loop(self, audio, seconds):
        count = min(round(seconds * self.rate), len(audio) // 3)
        if count < 2:
            raise ValueError("Loop source too short")
        theta = np.linspace(0, math.pi / 2, count)
        blend = audio[-count:] * np.cos(theta)[:, None] + audio[:count] * np.sin(theta)[:, None]
        result = np.concatenate([audio[count:-count], blend]).astype(np.float32)
        # Rotate an already continuous loop onto a quiet shared stereo crossing.
        mid = np.mean(result, axis=1)
        choices = np.flatnonzero(mid[:-1] * mid[1:] <= 0) + 1
        choices = choices[(choices >= 128) & (choices < min(len(result) - 128, self.rate * 2))]
        if len(choices):
            score = np.sum(result[choices] ** 2 + result[choices - 1] ** 2, axis=1)
            start = int(choices[np.argmin(score)])
        else:
            start = int(np.argmin(np.max(np.abs(result[:self.rate * 2]), axis=1)))
        result = np.concatenate([result[start:], result[:start]])
        return result, {"crossfade_seconds": count / self.rate,
                        "crossfade_curve": "equal-power cosine/sine tail into head",
                        "rotation_samples": start, "zero_crossing_channel": "stereo mid, minimum joint L/R energy",
                        "boundary_guard_samples": self.config["loops"]["boundary_guard_samples"]}

    def pitch(self, audio, semitones):
        if not semitones:
            return audio
        ratio = 2 ** (semitones / 12)
        # Natural resampling: pitch and decay speed rise together; no time stretch.
        args = self.command() + ["-loglevel", "error", "-f", "f32le", "-ar", str(self.rate),
                "-ac", str(audio.shape[1]), "-i", "-", "-af",
                f"asetrate={round(self.rate * ratio)},aresample={self.rate}",
                "-c:a", "pcm_f32le", "-f", "f32le", "-"]
        return np.frombuffer(self.run(args, audio.astype("<f4").tobytes()).stdout, dtype="<f4").reshape(-1, audio.shape[1]).copy()

    def measure(self, path, target, minimum_seconds=.4):
        info = sf.info(path)
        duration = info.frames / info.samplerate
        short = self.config["normalization"]["short_event_meter"]
        if duration < short["below_seconds"]:
            repetitions = max(2, math.ceil(short["minimum_analysis_seconds"] / duration))
            prefix = f"aloop=loop={repetitions - 1}:size={info.frames}"
            analysis_mode = f"repeated {repetitions} cycles for short-event integrated measurement"
        else:
            prefix = f"apad=whole_dur={minimum_seconds}"
            analysis_mode = "native duration"
        filt = f"{prefix},loudnorm=I={target}:TP={self.working_peak}:LRA=11:print_format=json"
        result = self.run(self.command() + ["-i", str(path), "-map", "0:a:0", "-af", filt, "-f", "null", "-"])
        matches = re.findall(r'\{\s*"input_i".*?\}', result.stderr.decode(errors="replace"), re.S)
        if not matches:
            raise RuntimeError("No loudnorm measurement")
        data = json.loads(matches[-1])
        if not math.isfinite(float(data["input_i"])):
            if minimum_seconds < .8:
                return self.measure(path, target, .8)
            raise ValueError(f"Cannot measure integrated LUFS: {path}")
        data["analysis_minimum_seconds"] = minimum_seconds
        data["analysis_prefix"] = prefix
        data["analysis_mode"] = analysis_mode
        data["short_event_meter_extended"] = duration < short["below_seconds"]
        return data

    def quantize(self, audio, name, loop):
        audio = audio.astype(np.float64).copy()
        if loop:
            n = min(self.config["loops"]["boundary_guard_samples"], len(audio) // 4)
            ramp = np.sin(np.linspace(0, math.pi / 2, n)) ** 2
            audio[:n] *= ramp[:, None]
            audio[-n:] *= ramp[::-1, None]
        seed = int.from_bytes(hashlib.sha256((str(self.config["dither_seed"]) + name).encode()).digest()[:8], "little")
        rng = np.random.Generator(np.random.PCG64(seed))
        dither = rng.random(audio.shape) - rng.random(audio.shape)
        pcm = np.clip(np.rint(audio * 32768 + dither), -32768, 32767).astype("<i2")
        pcm[0] = 0
        pcm[-1] = 0
        return pcm

    def write_wav(self, path, pcm, loop):
        channels = pcm.shape[1]
        fmt = struct.pack("<HHIIHH", 1, channels, self.rate, self.rate * channels * 2, channels * 2, 16)
        data = pcm.tobytes()
        chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
        if loop:
            # RIFF smpl: one forward loop, inclusive sample indices.
            smpl = struct.pack("<9I", 0, 0, round(1e9 / self.rate), 60, 0, 0, 0, 1, 0)
            smpl += struct.pack("<6I", 0, 0, 0, len(pcm) - 1, 0, 0)
            chunks += b"smpl" + struct.pack("<I", len(smpl)) + smpl
        path.write_bytes(b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks)

    def normalize(self, audio, name, target, loop):
        prepared = self.work / (name + "_prepared.wav")
        trial = self.work / (name + "_trial.wav")
        sf.write(prepared, audio, self.rate, subtype="FLOAT")
        initial = self.measure(prepared, target)
        requested = target
        gain_db = target - float(initial["input_i"])
        gain_low = gain_high = None
        attempts = []
        output = self.out / (name + ".wav")
        try:
            limiter = self.config["normalization"]["residual_limiter"]
            for iteration in range(self.config["normalization"]["maximum_passes"]):
                if iteration == 0:
                    values = initial
                    norm = (f"{values['analysis_prefix']},"
                            f"loudnorm=I={requested:.5f}:TP={self.working_peak}:LRA=11:"
                            f"measured_I={values['input_i']}:measured_TP={values['input_tp']}:"
                            f"measured_LRA={values['input_lra']}:measured_thresh={values['input_thresh']}:"
                            f"offset={values['target_offset']}:linear=true:print_format=json,"
                            f"aresample={self.rate},atrim=end_sample={len(audio)}")
                    args = self.command() + ["-i", str(prepared), "-map", "0:a:0", "-af", norm]
                    result = self.run(args + ["-ar", str(self.rate), "-ac", str(audio.shape[1]), "-c:a", "pcm_f32le", "-f", "f32le", "-"])
                else:
                    # Re-render the ORIGINAL prepared signal for each gain trial.
                    # This avoids stacking multiple passes of peak reduction.
                    norm = (f"aresample={self.rate * limiter['oversample_factor']},volume={gain_db:.8f}dB,"
                            f"alimiter=limit={10 ** (limiter['ceiling_dbtp'] / 20):.16f}:attack={limiter['attack_ms']}:release={limiter['release_ms']}:level=false:latency=true,"
                            f"aresample={self.rate},atrim=end_sample={len(audio)}")
                    args = self.command() + ["-f", "f32le", "-ar", str(self.rate), "-ac", str(audio.shape[1]), "-i", "-", "-af", norm,
                            "-ar", str(self.rate), "-ac", str(audio.shape[1]), "-c:a", "pcm_f32le", "-f", "f32le", "-"]
                    result = self.run(args, audio.astype("<f4").tobytes())
                rendered = np.frombuffer(result.stdout, dtype="<f4").reshape(-1, audio.shape[1]).copy()
                if len(rendered) != len(audio):
                    raise ValueError(f"Unexpected render length {name}: {len(rendered)} != {len(audio)}")
                pcm = self.quantize(rendered, name, loop)
                self.write_wav(trial, pcm, loop)
                measured = self.measure(trial, target)
                actual, peak = float(measured["input_i"]), float(measured["input_tp"])
                attempts.append({"method": "loudnorm" if iteration == 0 else "single-pass oversampled limiter",
                                 "gain_db": None if iteration == 0 else gain_db,
                                 "measured_lufs": actual, "true_peak_dbtp": peak})
                if abs(actual - target) <= self.config["normalization"]["tolerance_lu"] and peak <= self.peak_ceiling:
                    trial.replace(output)
                    return {"source_prepared_lufs": float(initial["input_i"]),
                            "source_prepared_true_peak_dbtp": float(initial["input_tp"]),
                            "lufs": actual, "true_peak_dbtp": peak,
                            "sample_peak_dbfs": db(np.max(np.abs(pcm.astype(np.float64))) / 32768),
                            "analysis_minimum_seconds": measured["analysis_minimum_seconds"],
                            "short_event_meter_extended": measured["short_event_meter_extended"],
                            "analysis_mode": measured["analysis_mode"],
                            "normalization_attempts": attempts, "frames": len(pcm), "sha256": sha256(output)}
                if iteration:
                    if actual < target:
                        gain_low = gain_db
                        gain_db = ((gain_low + gain_high) / 2 if gain_high is not None
                                   else gain_db + max(.5, (target - actual) * 1.8))
                    else:
                        gain_high = gain_db
                        gain_db = ((gain_low + gain_high) / 2 if gain_low is not None
                                   else gain_db - max(.5, (actual - target) * 1.8))
            raise ValueError(f"Normalization did not meet target for {name}: {attempts}")
        finally:
            prepared.unlink(missing_ok=True)
            trial.unlink(missing_ok=True)

    def process_entry(self, entry):
        if entry['group'] == 'MUSIC':
            return self.process_music(entry)
        source = inside(self.source_root / entry["source"], self.source_root)
        channels = entry["channels"]
        audio, source_info, pan = self.decode(source, channels)
        regions, threshold, settings = self.detection(audio, entry)
        candidates = []
        rejected = 0
        if entry.get("slice"):
            for begin, end in regions:
                pitch_rate = 2 ** (entry.get("pitch_semitones", 0) / 12)
                if (end - begin) / self.rate / pitch_rate < settings["minimum_slice_ms"] / 1000:
                    rejected += 1
                    continue
                candidates.append(dict(start_frame=begin, end_frame=end, **self.features(audio[begin:end])))
            if not candidates:
                raise ValueError(f"No slices survived for {entry['id']}")
            kept = self.distinct(candidates, settings["maximum_per_event"])
            selected = [candidates[i] for i in kept]
        else:
            begin, end = (0, len(audio)) if entry.get("loop") else (regions[0][0], regions[-1][1])
            if entry.get("max_duration_seconds"):
                length = round(entry["max_duration_seconds"] * self.rate)
                if end - begin > length:
                    step = self.rate // 20
                    # Loudest four-second cheer window, then gentle entrance/exit.
                    power = np.mean(audio.astype(np.float64) ** 2, axis=1)
                    sums = np.r_[0, np.cumsum(power)]
                    starts = np.arange(begin, end - length + 1, step)
                    begin = int(starts[np.argmax(sums[starts + length] - sums[starts])])
                    end = begin + length
            selected = [dict(start_frame=begin, end_frame=end, **self.features(audio[begin:end]))]
            candidates = selected
        outputs = []
        for number, candidate in enumerate(selected, 1):
            name = (entry["output_prefix"] + f"_{number:02d}") if entry.get("slice") else entry["output"]
            if not re.fullmatch(r"[A-Za-z0-9_]+", name):
                raise ValueError("Unsafe output name")
            segment = audio[candidate["start_frame"]:candidate["end_frame"]].copy()
            segment = self.pitch(segment, entry.get("pitch_semitones", 0))
            loop_info = None
            if entry.get("loop"):
                segment, loop_info = self.loop(segment, self.config["loops"]["crossfade_seconds"])
            else:
                segment = self.fade(segment, entry.get("fade_in_ms", 5), entry.get("fade_out_ms", 20))
            metrics = self.normalize(segment, name, entry["target_lufs"], bool(loop_info))
            result = dict(output=name + ".wav", source=entry["source"], event=entry["id"], group=entry["group"],
                          channels=channels, sample_rate=self.rate, bits_per_sample=16,
                          duration_seconds=metrics["frames"] / self.rate, target_lufs=entry["target_lufs"],
                          source_start_seconds=candidate["start_frame"] / self.rate,
                          source_end_seconds=candidate["end_frame"] / self.rate,
                          selection_features=candidate, silence_threshold_dbfs=threshold,
                          slices_kept=len(selected) if entry.get("slice") else None,
                          candidates=len(candidates) if entry.get("slice") else None,
                          pitch_semitones=entry.get("pitch_semitones", 0), loop=loop_info,
                          downmix=pan, **metrics)
            if loop_info:
                result["loop"].update(start_sample=0, end_sample_inclusive=metrics["frames"] - 1,
                                      wav_smpl_chunk=True, endpoint_pcm_frames_zero=True)
            outputs.append(result)
        if entry.get("slice"):
            for number in range(len(selected) + 1, settings["maximum_per_event"] + 1):
                (self.out / (entry["output_prefix"] + f"_{number:02d}.wav")).unlink(missing_ok=True)
        return outputs, {"id": entry["id"], "source": entry["source"],
                         "source_info": source_info, "candidate_count": len(candidates),
                         "short_candidates_dropped": rejected, "kept_count": len(selected),
                         "selected_source_ranges_seconds": [[c["start_frame"] / self.rate, c["end_frame"] / self.rate] for c in selected]}

    def render(self, group=None):
        inventory_before = self.verify_sources(group)
        self.music_grids = {}
        outputs, events = [], []
        entries = [e for e in self.config['events'] if group is None or e['group'] == group]
        for index, entry in enumerate(entries, 1):
            print(f"[{index:02d}/{len(entries)}] {entry['id']}", flush=True)
            generated, event = self.process_entry(entry)
            outputs.extend(generated)
            events.append(event)
            print("  " + ", ".join(f"{x['output']} {x['lufs']:.1f} LUFS/{x['true_peak_dbtp']:.1f} dBTP" for x in generated), flush=True)
        inventory_after = self.verify_sources(group)
        if inventory_before != inventory_after:
            raise RuntimeError("Source inventory changed during processing")
        return {"manifest_sha256": sha256(self.manifest_path), "environment": self.environment,
                "render_group": group,
                "source_inventory_unchanged": True, "source_inventory": inventory_after,
                "events": events, "outputs": outputs}

    def audition_page(self, report):
        groups = []
        for group in GROUPS:
            cards = []
            for out in report["outputs"]:
                if out["group"] != group:
                    continue
                loop = " loop" if out["loop"] else ""
                label = html.escape(out["output"])
                source = html.escape(out["source"])
                cards.append(f'<article><h3>{label}</h3><audio controls preload="none"{loop} src="{label}"></audio>'
                             f'<p>{out["duration_seconds"]:.3f}s · {out["channels"]}ch · {out["lufs"]:.2f} LUFS · '
                             f'{out["true_peak_dbtp"]:.2f} dBTP{" · LOOP" if loop else ""}</p><details><summary>Source</summary><p>{source}</p></details></article>')
            groups.append(f'<section id="{group.split()[0].lower()}"><h2>{html.escape(group)}</h2><div class="grid">{"".join(cards)}</div></section>')
        page = '<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
        page += '<title>Lake Fight — Audio audition</title><style>body{margin:0;background:#132027;color:#edf1df;font:16px system-ui}main{max-width:1300px;margin:auto;padding:28px}h1{margin-bottom:6px}h2{border-top:1px solid #53636b;padding-top:24px}h3{font-size:15px;overflow-wrap:anywhere;margin-top:0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:14px}article{background:#20333d;border:1px solid #435962;border-radius:10px;padding:16px}audio{width:100%}p{color:#c5d2cf;font-size:13px}a{color:#e9cf89}nav{display:flex;flex-wrap:wrap;gap:16px;margin:24px 0}button{background:#d9c491;border:0;border-radius:6px;padding:9px 14px;cursor:pointer}details p{overflow-wrap:anywhere}</style><main>'
        page += f'<h1>LAKE FIGHT — audio audition</h1><p>{len(report["outputs"])} processed WAVs · 48 kHz / 16-bit · loops repeat automatically · one player at a time</p>'
        page += '<button onclick="document.querySelectorAll(\'audio\').forEach(a=>a.pause())">Stop all</button><nav>'
        page += "".join(f'<a href="#{g.split()[0].lower()}">{html.escape(g)}</a>' for g in GROUPS) + '</nav>'
        page += "".join(groups) + '<script>document.querySelectorAll("audio").forEach(p=>p.addEventListener("play",()=>document.querySelectorAll("audio").forEach(q=>{if(q!==p)q.pause()})))</script></main></html>'
        (self.out / "_listen.html").write_text(page, encoding="utf-8")

    def write_report(self, report, reproducibility):
        lines = ["# A01 / A03 - audio processing", "", "Backend: **FFmpeg**, with NumPy for deterministic sample operations and SoundFile for float intermediates.",
                 "", f"- {self.environment['ffmpeg']}", f"- Python {self.environment['python']}; NumPy {self.environment['numpy']}; SoundFile {self.environment['soundfile']}",
                 "- Dependencies isolated under Tools/audio/.venv; pinned in Tools/audio/requirements.lock.txt.",
                 "- Sonniss GDC2026 sources licensed/royalty-free as confirmed by the director. Current render source hashes verified unchanged before/after processing; prior A01 source evidence retained separately when rendering MUSIC only.",
                 "- No git or Unreal commands; deliverables under Audio/ and processing tooling under Tools/audio/.",
                 "", "## Outputs", "", "| Output | Source file | Duration (s) | LUFS-I | True peak (dBTP) | Slices kept |", "|---|---|---:|---:|---:|---|"]
        for group in GROUPS:
            lines.append(f"| **{group}** | | | | | |")
            for out in report["outputs"]:
                if out["group"] != group:
                    continue
                slices = f"{out['slices_kept']}/{out['candidates']}" if out["slices_kept"] else "single"
                lines.append(f"| [{out['output']}](Processed/{out['output']}) | {out['source']} | {out['duration_seconds']:.3f} | {out['lufs']:.2f} | {out['true_peak_dbtp']:.2f} | {slices} |")
        lines += ["", "## Processing choices", "",
                  "- All WAVs are PCM16 / 48kHz. Splash/paddle/ram/voice are mono; ambience/UI are stereo. Quad shore recordings use equal front/rear stereo blends; 5.1 crowd uses L/R+centre/rears with LFE omitted. Mono sparkle input is duplicated to stereo.",
                  "- Targets: one-shots -16 LUFS, voices -18, UI -20, ambience -28. True-peak ceiling -1 dBTP; loudnorm uses -1.1 dBTP working headroom. If a short/high-crest event remains below target, gain is calibrated through a 4x-oversampled, 5ms-lookahead/50ms-release limiter at -1.2dBFS. Every gain trial starts from the original prepared float signal, so peak reduction is not stacked. The final PCM16 is remeasured; integrated tolerance is +/-0.20 LU.",
                  "- Silence split starts at -40dBFS, 120ms gap, 5ms blocks, 10ms pre-roll/40ms post-roll; slices shorter than 80ms after any pitch-rate change are dropped. Selection begins with the strongest event then greedily maximizes distance in standardized log spectral-centroid/peak space, keeping at most six; numbering follows source chronology.",
                  "- Non-loop clips are trimmed and faded 5ms in / 20ms out. EndCheer selects the most energetic continuous four-second window, with 50ms in / 700ms out for a natural ending.",
                  "- RamWood +3 semitones uses natural resampling (rate factor 2^(3/12)); decay duration becomes 0.8409 of its source slice. No time stretch or added sustain.",
                  "- A01 loops equal-power crossfade two seconds of tail into head and remove that overlap from length. Music uses the bar-aligned assembly documented below. The continuous result is rotated to a quiet stereo-mid zero crossing, then a 64-sample guard anchors both channels to digital zero. RIFF smpl metadata uses loop start 0 and inclusive end N-1. Exact points are in _processing.json; audition players loop those complete WAVs.",
                  "- A sub-400ms one-shot has insufficient duration for ordinary 400ms integrated gating. Only the metering copy is repeated for whole cycles to at least 1.2s; the delivered file keeps its original trimmed duration. This avoids normalizing a tiny transient against invented silence. Rows use that documented short-event integrated convention; ordinary files use native-duration metering.",
                  "- PCM16 TPDF dither is seeded by manifest seed plus output name. Source hashes, tool versions, processing settings and selection rules are pinned; no timestamp metadata is written into WAVs."]
        short = [o["output"] for o in report["outputs"] if o["short_event_meter_extended"]]
        if short:
            lines += ["", "Short-event metering convention applied to: " + ", ".join(short) + "."]
        taunt = next(o for o in report["outputs"] if o["output"] == "VO_LF_Taunt_01.wav")
        lines += ["", f"Police laugh loudness check: prepared source {taunt['source_prepared_lufs']:.2f} LUFS -> delivered {taunt['lufs']:.2f} LUFS, true peak {taunt['true_peak_dbtp']:.2f} dBTP. The notably louder source was attenuated to the voice target."]
        music = [o for o in report['outputs'] if o['group'] == 'MUSIC']
        if music:
            lines += ['', '## A03 - music', '',
                      "Sources: Boardwalk Jaunt by Sky Toes (Main/Menu) and Funk Formula by Nick Petrov (Final), Uppbeat Basic Licenses (director supplied); credit required, exact credits recorded in Docs/ASSETS.md. Source MP3s are read-only and must not be redistributed. No source links or copies are embedded in the audition page.", '',
                      '| Output | Source seconds used | BPM | Source / playback bars (4/4) | Duration (s) | LUFS-I | dBTP |',
                      '|---|---|---:|---:|---:|---:|---:|']
            for out in music:
                lines.append(f"| {out['output']} | {out['source_start_seconds']:.6f}-{out['source_end_seconds']:.6f} | {out['bpm']:.3f} | {out['loop']['source_bars']} / {out['loop']['bars']} | {out['duration_seconds']:.6f} | {out['lufs']:.2f} | {out['true_peak_dbtp']:.2f} |")
            for source in dict.fromkeys(o['source'] for o in music):
                grid = next(o['beat_grid'] for o in music if o['source'] == source)
                lines += ['', f"{source}: detected {grid['bpm']:.6f} BPM over {grid['analysis_seconds'][0]}-{grid['analysis_seconds'][1]}s using {grid['method']}; grid {grid['grid_bpm']:.6f} BPM, {grid['beats_per_bar']}/4, first downbeat {grid['downbeat_seconds']:.6f}s. Flux phase {grid['beat_phase_seconds']:.6f}s agrees within {abs(grid['phase_error_seconds']) * 1000:.2f}ms. Bar grouping is inferred from the opening and repeating arrangement; director audition remains required."]
            lines += ['',
                      'Music candidates begin/end on bar lines. One extra source bar supplies the one-bar equal-power overlap: N+1 source bars become exactly N playback bars (rounded once to the nearest sample). Boardwalk is scanned across the track for its strongest/quietest sustained passages by mean RMS power; within 0.15dB of the best energy, tail/head spectral distance and waveform correlation select the cleaner match. Per-bar energy floors reject dips. Funk Formula candidates stay in 70-120s and reject a bar more than 3dB below the median, excluding the brief approximately 87-89s dip. No intro, breakdown or outro enters Final.', '',
                      'Final loop rotation is restricted to a stereo-mid zero crossing within 2ms of the bar line, preserving duration. The A01 64-sample guards anchor both channels to exact zero; this is a stereo-mid crossing plus digital-zero endpoints, not a claim of simultaneous natural L/R crossings. Crossfade, rotation, grid, per-bar source energy and all candidate scores are in _processing.json.', '',
                      'Targets: main -22 LUFS-I, menu -24 LUFS-I, final -21 LUFS-I; ceiling -1 dBTP. Human check: listen through several repetitions of each MUSIC player for rhythmic/harmonic closure and crossfade swell; approve menu calmness, steady fight energy and final climax. Boardwalk is a live-feeling performance: measured local tempo 106.889-106.908 BPM supports the average grid, but phrase closure still needs an ear. Numeric evidence does not establish audible seamlessness or in-game mix acceptance.', '',
                      'A03 files: Tools/audio/process_audio.py, analyze_music.py, audio_manifest.json; Audio/Processed/MUS_LF_Main_Loop.wav, MUS_LF_Menu.wav, MUS_LF_Final_Loop.wav, _listen.html, _processing.json, _reproducibility.json, _validation.json; Audio/AUDIO_REPORT.md. Local environment/cache and diagnostic evidence stay under Tools/audio/.', '',
                      'Director log draft (write restriction): A03 asked, then updated to two sources / three music loops and manifest pipeline delivered / awaiting director audition. No Docs files edited.']
        lines += ["", "## Verification", "", f"- Outputs: {len(report['outputs'])}; source mappings: {len(report['events'])}; source inventory hashes protected: {len(report['source_inventory'])}.",
                  f"- {sum(o['channels'] == 1 for o in report['outputs'])} mono / {sum(o['channels'] == 2 for o in report['outputs'])} stereo; {sum(bool(o['loop']) for o in report['outputs'])} loops. Largest target error {max(abs(o['lufs'] - o['target_lufs']) for o in report['outputs']):.2f} LU; highest true peak {max(o['true_peak_dbtp'] for o in report['outputs']):.2f} dBTP.",
                  "- Every rendered WAV was remeasured for LUFS and true peak after PCM16 quantization. Format, channel count, sample rate, loop endpoints and RIFF metadata are checked.",
                  "- " + (f"Two independent processing passes completed for the selected render group ({report.get('render_group') or 'ALL'}); selected WAV hashes match byte-for-byte. Earlier A01 evidence is retained for untouched outputs." if reproducibility else "Single render completed; use --verify-repro to verify two processing passes."),
                  "- Evidence: Processed/_processing.json and Processed/_reproducibility.json. Selected passes decode and process sources again; there is no rendered-audio cache.",
                  "", "## Run / audition", "", "    Tools/audio/.venv/Scripts/python.exe -B Tools/audio/process_audio.py --group MUSIC --verify-repro", "",
                  "Manifest: Tools/audio/audio_manifest.json. Open [Processed/_listen.html](Processed/_listen.html) in a browser to audition the grouped outputs; no server is required.",
                  "", "## Files / human check / stop", "",
                  "Tooling: Tools/audio/process_audio.py, audio_manifest.json, requirements.lock.txt, isolated .venv and scratch .work/.tmp. Deliverables: Audio/Processed/*.wav, _listen.html, _processing.json, _reproducibility.json and this report.",
                  "Audition impact/voice choices, loop seams and relative levels in the provided page. Numeric normalization and reproducibility do not establish final artistic acceptance or in-game mix. No import, attenuation, concurrency, SoundCue, game code or other audio work was started.",
                  "The unused Laughing Female source remains untouched; it was not substituted for the requested Police Officer laugh. The two initially absent requested sources were added by the director before processing.",
                  "Director log draft (kept here under the write restriction): A01 asked / manifest pipeline, processed audio and audition page delivered / artistic acceptance pending director audition."]
        (ROOT / "Audio" / "AUDIO_REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def validate_files(self, report):
        loops, clipped = [], 0
        for out in report["outputs"]:
            path = self.out / out["output"]
            with wave.open(str(path), "rb") as wav:
                assert wav.getnchannels() == out["channels"]
                assert wav.getframerate() == 48000 and wav.getsampwidth() == 2
                assert wav.getnframes() == out["frames"]
                raw = wav.readframes(wav.getnframes())
            pcm = np.frombuffer(raw, dtype="<i2").reshape(-1, out["channels"])
            clipped += int(np.count_nonzero((pcm == -32768) | (pcm == 32767)))
            assert (pcm[0] == 0).all() and (pcm[-1] == 0).all()
            if out["loop"]:
                riff = path.read_bytes()
                assert riff[:4] == b'RIFF' and riff[8:12] == b'WAVE'
                assert struct.unpack_from('<I', riff, 4)[0] + 8 == len(riff)
                chunks, offset = {}, 12
                while offset + 8 <= len(riff):
                    key, size = struct.unpack_from('<4sI', riff, offset)
                    chunks[key] = riff[offset + 8:offset + 8 + size]
                    offset += 8 + size + (size % 2)
                smpl = struct.unpack('<15I', chunks[b'smpl'])
                assert smpl[7] == 1 and smpl[10] == 0 and smpl[11] == 0
                assert smpl[12] == len(pcm) - 1 and smpl[14] == 0
                loops.append(dict(output=out['output'], duration_seconds=out['duration_seconds'],
                                  start=smpl[11], end_inclusive=smpl[12]))
                if out['group'] == 'MUSIC':
                    expected = out['loop']['bars'] * out['beat_grid']['beats_per_bar'] * 60 / out['bpm'] * self.rate
                    assert abs(len(pcm) - expected) <= .500001
                    assert abs(out['loop']['rotation_samples']) <= .002 * self.rate
            assert sha256(path) == out["sha256"]
            assert abs(out["lufs"] - out["target_lufs"]) <= self.config["normalization"]["tolerance_lu"]
            assert out["true_peak_dbtp"] <= self.peak_ceiling
        assert clipped == 0
        return dict(outputs=len(report['outputs']), mono=sum(o['channels'] == 1 for o in report['outputs']),
                    stereo=sum(o['channels'] == 2 for o in report['outputs']), loop_count=len(loops), loops=loops,
                    maximum_loudness_error_lu=max(abs(o['lufs'] - o['target_lufs']) for o in report['outputs']),
                    highest_true_peak_dbtp=max(o['true_peak_dbtp'] for o in report['outputs']),
                    source_files_unchanged=report['source_inventory_unchanged'],
                    source_hashes_checked=len(report['source_inventory']),
                    format='PCM16 / 48000 Hz', clipped_samples=clipped,
                    manifest_sha256=sha256(self.manifest_path), processing_script_sha256=sha256(Path(__file__)),
                    analysis_script_sha256=sha256(HERE / 'analyze_music.py'))

    def validate_page(self, report):
        class Players(HTMLParser):
            def __init__(self):
                super().__init__()
                self.players = {}
            def handle_starttag(self, tag, attrs):
                if tag == 'audio':
                    values = dict(attrs)
                    self.players[values['src']] = values
        parser = Players()
        page = (self.out / '_listen.html').read_text(encoding='utf-8')
        parser.feed(page)
        assert len(parser.players) == len(report['outputs'])
        for out in report['outputs']:
            attrs = parser.players[out['output']]
            assert ('loop' in attrs) == bool(out['loop'])
            assert 'controls' in attrs and (self.out / out['output']).is_file()
        assert '<h2>MUSIC</h2>' in page
        assert '.mp3"' not in page


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=HERE / "audio_manifest.json")
    parser.add_argument("--verify-repro", action="store_true", help="Run the full render twice and compare every WAV hash")
    parser.add_argument('--group', choices=GROUPS, help='Render only this group, preserving verified existing outputs')
    args = parser.parse_args()
    processor = Processor(args.manifest)
    previous = json.loads((processor.out / '_processing.json').read_text(encoding='utf-8')) if args.group else None
    previous_repro = json.loads((processor.out / '_reproducibility.json').read_text(encoding='utf-8')) if args.group else None
    if previous:
        processor.validate_files(previous)
    report = processor.render(args.group)
    processor.validate_files(report)
    first = {o["output"]: o["sha256"] for o in report["outputs"]}
    repro = {"full_processing_passes": 1, "verified": False, "first_pass": first}
    if args.verify_repro:
        print("SECOND COMPLETE PASS: verifying byte-for-byte reproducibility", flush=True)
        second = processor.render(args.group)
        processor.validate_files(second)
        hashes = {o["output"]: o["sha256"] for o in second["outputs"]}
        if first != hashes:
            raise RuntimeError("Idempotence failed: output WAV hashes differ")
        repro.update(full_processing_passes=2, verified=True, second_pass=hashes)
        report = second
    if previous:
        report['outputs'] = [o for o in previous['outputs'] if o['group'] != args.group] + report['outputs']
        replaced = {e['id'] for e in processor.config['events'] if e['group'] == args.group}
        report['events'] = [e for e in previous['events'] if e['id'] not in replaced] + report['events']
        report['previous_environment'] = previous.get('previous_environment', previous['environment'])
        report['previous_source_inventory'] = previous.get('previous_source_inventory', previous['source_inventory'])
        repro['render_group'] = args.group
        repro['preserved_prior_evidence'] = previous_repro.get('preserved_prior_evidence', previous_repro)
        processor.validate_files(report)
    save_json(processor.out / "_processing.json", report)
    save_json(processor.out / "_reproducibility.json", repro)
    processor.audition_page(report)
    processor.validate_page(report)
    validation = processor.validate_files(report)
    validation.update(all_player_links_valid=True, render_group=args.group,
                      full_processing_passes=repro['full_processing_passes'],
                      all_selected_wav_hashes_identical=repro['verified'],
                      preserved_output_hashes_unchanged=all(sha256(processor.out / o['output']) == o['sha256']
                          for o in previous['outputs'] if o['group'] != args.group) if previous else None,
                      prior_source_hash_evidence_retained=len(report.get('previous_source_inventory', {})))
    save_json(processor.out / '_validation.json', validation)
    processor.write_report(report, repro["verified"])
    print(f"COMPLETE: {len(report['outputs'])} WAVs; sources unchanged; reproducibility={repro['verified']}", flush=True)


if __name__ == "__main__":
    main()
