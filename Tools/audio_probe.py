import argparse
import cmath
import math
import random
import re
import struct
import sys
import wave
from dataclasses import dataclass
from pathlib import Path

from repo_paths import REPO_ROOT

frame_rate = 48000
frames_per_step = 800
stress_voices = 128
stress_voices_started = stress_voices + stress_voices // 2
mix_budget = 0.25
silence_floor = 1e-4

offline_pattern = re.compile(
    r"audio: offline render wrote (?P<path>.+?), (?P<frames>\d+) frames, sample hash (?P<hash>0x[0-9a-fA-F]{16}), "
    r"(?P<started>\d+) voices started, peak (?P<peak>\d+) voices"
)
device_pattern = re.compile(
    r"audio: device ran (?P<blocks>\d+) blocks, (?P<underruns>\d+) underruns, (?P<allocations>\d+) allocations on the audio thread, "
    r"mix p50 (?P<p50>[0-9.]+ \S+) p95 (?P<p95>[0-9.]+ \S+), block period (?P<period>[0-9.]+ \S+), "
    r"p95 over block (?P<ratio>[0-9.]+), peak (?P<peak>\d+) voices"
)
dropped_pattern = re.compile(r"audio: .* was dropped")
measured_pattern =re.compile(r"bench: (\d+) frames measured")


@dataclass
class check:
    name: str
    passed: bool
    detail: str


def read_log(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def read_float_wav(path: Path) -> tuple[int, int, list[float]]:
    data = path.read_bytes()
    if data[0:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a RIFF/WAVE file")
    offset = 12
    channels = 0
    rate = 0
    samples: list[float] = []
    while offset + 8 <= len(data):
        tag = data[offset : offset + 4]
        size = struct.unpack_from("<I", data, offset + 4)[0]
        body = offset + 8
        if tag == b"fmt ":
            fmt, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", data, body)
            if fmt != 3 or bits != 32:
                raise ValueError(f"{path} is format {fmt} at {bits} bits, expected 32-bit float")
        elif tag == b"data":
            count = size // 4
            samples = list(struct.unpack_from(f"<{count}f", data, body))
        offset = body + size + (size & 1)
    return channels, rate, samples


def sweep_samples() -> list[float]:
    low = 200.0
    high = 4000.0
    growth = math.log(high / low)
    return [
        0.5 * math.sin(2.0 * math.pi * low * (math.exp(growth * (index / frame_rate)) - 1.0) / growth)
        for index in range(frame_rate)
    ]


def hum_samples() -> list[float]:
    source = random.Random(0x68756D6D)
    seconds = 2
    peak = 0.5
    top = 8000.0
    frames = seconds * frame_rate
    samples = [0.0] * frames
    for fundamental in (110.0, 110.5):
        for harmonic in range(1, int(top / fundamental) + 1):
            step = 2.0 * math.pi * fundamental * harmonic / frame_rate
            phase = source.uniform(0.0, 2.0 * math.pi)
            for index in range(frames):
                samples[index] += math.sin(step * index + phase) / harmonic
    scale = peak / max(abs(sample) for sample in samples)
    return [scale * sample for sample in samples]


def pulse_samples() -> list[float]:
    source = random.Random(0x70756C73)
    period = frame_rate // 4
    decay = 0.012 * frame_rate
    return [0.6 * math.exp(-(index % period) / decay) * source.uniform(-1.0, 1.0) for index in range(frame_rate)]


def sweep_loop_samples() -> list[float]:
    low = 200.0
    high = 4000.0
    half = frame_rate // 2
    growth = math.log(high / low)

    def phase(index: int) -> float:
        return 2.0 * math.pi * low * 0.5 * (math.exp(growth * (index / half)) - 1.0) / growth

    whole_half_turns = round(phase(half) / math.pi) * math.pi / phase(half)
    rising = [0.5 * math.sin(whole_half_turns * phase(index)) for index in range(half + 1)]
    return rising[:half] + [-rising[half - index] for index in range(half)]


def knock_samples() -> list[float]:
    source = random.Random(0x6B6E6F63)
    frames = int(0.060 * frame_rate)
    decay = 0.010 * frame_rate
    fade = int(0.005 * frame_rate)
    partials = ((420.0, 1.0), (1130.0, 0.6), (2350.0, 0.35), (4100.0, 0.2))
    grit = int(0.003 * frame_rate)
    samples: list[float] = []
    for index in range(frames):
        tone = sum(weight * math.sin(2.0 * math.pi * pitch * index / frame_rate) for pitch, weight in partials)
        noise = 0.4 * source.uniform(-1.0, 1.0) * min(index / 8.0, 1.0) if index < grit else 0.0
        tail = min((frames - 1 - index) / fade, 1.0)
        samples.append((tone + noise) * math.exp(-index / decay) * tail)
    scale = 0.8 / max(abs(sample) for sample in samples)
    return [scale * sample for sample in samples]


clip_kinds = {
    "sweep": ("stress_sweep.wav", sweep_samples),
    "hum": ("listen_hum.wav", hum_samples),
    "pulse": ("listen_pulse.wav", pulse_samples),
    "sweep_loop": ("listen_sweep.wav", sweep_loop_samples),
    "knock": ("contact_knock.wav", knock_samples),
}


def make_clip(args: argparse.Namespace) -> int:
    file_name, generate = clip_kinds[args.kind]
    out: Path = args.out or REPO_ROOT / "Sandbox" / "Assets" / "Audio" / file_name
    out.parent.mkdir(parents=True, exist_ok=True)
    samples = generate()
    frames = b"".join(struct.pack("<h", round(32767.0 * sample)) for sample in samples)
    with wave.open(str(out), "wb") as clip:
        clip.setnchannels(1)
        clip.setsampwidth(2)
        clip.setframerate(frame_rate)
        clip.writeframes(frames)
    print(f"wrote {out} ({len(samples)} frames, mono, 16-bit)")
    return 0


def check_offline(logs: list[Path]) -> list[check]:
    checks: list[check] = []
    hashes: list[str] = []
    for path in logs:
        text = read_log(path)
        match = offline_pattern.search(text)
        if not match:
            checks.append(check(f"offline {path.name}", False, "no 'audio: offline render wrote' line"))
            continue
        hashes.append(match["hash"])
        frames = int(match["frames"])
        measured = measured_pattern.search(text)
        steps_floor = int(measured[1]) if measured else 0
        whole_steps = frames % frames_per_step == 0 and frames // frames_per_step >= steps_floor > 0
        checks.append(check(
            f"offline {path.name} frames",
            whole_steps,
            f"{frames} frames = {frames / frames_per_step:g} steps of {frames_per_step}, {steps_floor} frames measured",
        ))
        peak_voices = int(match["peak"])
        started = int(match["started"])
        dropped = len(dropped_pattern.findall(text))
        checks.append(check(
            f"offline {path.name} voices",
            peak_voices >= stress_voices and started == stress_voices_started and dropped == 0,
            f"peak {peak_voices} voices (need {stress_voices}), {started} started (need {stress_voices_started}), {dropped} dropped",
        ))

        wav_path = Path(match["path"])
        if not wav_path.exists():
            checks.append(check(f"offline {path.name} wav", False, f"{wav_path} does not exist"))
            continue
        channels, rate, samples = read_float_wav(wav_path)
        level = max((abs(sample) for sample in samples), default=0.0)
        sound = (
            channels == 2
            and rate == frame_rate
            and len(samples) == frames * channels
            and all(math.isfinite(sample) for sample in samples)
            and silence_floor < level <= 1.0
        )
        checks.append(check(
            f"offline {path.name} wav",
            sound,
            f"{wav_path}: {channels} ch at {rate}, {len(samples)} samples, peak {level:.4f}, hash {match['hash']}",
        ))
    checks.append(check(
        "criterion 1: same hash in every run",
        len(hashes) == len(logs) and len(set(hashes)) == 1,
        ", ".join(hashes) if hashes else "no hashes",
    ))
    return checks


def check_device(logs: list[Path], voices_needed: int) -> list[check]:
    checks: list[check] = []
    for path in logs:
        text = read_log(path)
        match = device_pattern.search(text)
        dropped = len(dropped_pattern.findall(text))
        if not match:
            checks.append(check(f"device {path.name}", False, "no 'audio: device ran' line"))
            continue
        blocks = int(match["blocks"])
        underruns = int(match["underruns"])
        allocations = int(match["allocations"])
        ratio = float(match["ratio"])
        peak_voices = int(match["peak"])
        passed = blocks > 0 and underruns == 0 and allocations == 0 and ratio <= mix_budget and peak_voices >= voices_needed and dropped == 0
        checks.append(check(
            f"criterion 2: device {path.name}",
            passed,
            f"{blocks} blocks, {underruns} underruns, {allocations} allocations, mix p50 {match['p50']} p95 {match['p95']}, "
            f"block {match['period']}, p95/block {ratio:.4f} (budget {mix_budget}), peak {peak_voices} voices (need {voices_needed}), {dropped} dropped",
        ))
    return checks


def stage1(args: argparse.Namespace) -> int:
    checks = check_offline(args.offline) + check_device(args.device, stress_voices)
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage1: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


vector_text = r"\((-?[0-9.]+) \S+, (-?[0-9.]+) \S+, (-?[0-9.]+) \S+\)"
spatial_header_pattern = re.compile(
    r"audio_spatial_probe: speed of sound (?P<speed>[0-9.]+) \S+, listener at " + vector_text
)
spatial_sync_pattern = re.compile(r"audio_spatial_probe: sync frame (?P<frame>\d+) pitch (?P<pitch>[0-9.]+) Hz")
spatial_segment_pattern = re.compile(
    r"audio_spatial_probe: segment (?P<kind>\w+) frame (?P<frame>\d+) position " + vector_text
    + r" velocity " + vector_text + r" pitch (?P<pitch>[0-9.]+) Hz frames (?P<frames>\d+)"
)
listening_pattern = re.compile(r"audio: listening through (?P<mode>\w+), head radius (?P<radius>[0-9.]+) m\b")

interaural_delay_floor = 0.0005
interaural_delay_ceiling = 0.0008
far_ear_audible_db = 20.0
shadow_facing = 2.0
shadow_behind = 1.05 + 0.95 * math.cos(math.radians(1.2 * 180.0))

onset_floor = 1e-4
doubling_db = 20.0 * math.log10(2.0)
doubling_tolerance_db = 0.5
arrival_tolerance = 0.001
doppler_tolerance = 0.01
level_lead_in = 0.1
level_lead_out = 0.05
doppler_lead_in = 0.5
doppler_lead_out = 0.2


@dataclass
class spatial_segment:
    kind: str
    frame: int
    position: tuple[float, float, float]
    velocity: tuple[float, float, float]
    pitch: float
    frames: int


def first_sound(left: list[float], right: list[float], start: int) -> int | None:
    for index in range(max(start, 0), len(left)):
        if abs(left[index]) > onset_floor or abs(right[index]) > onset_floor:
            return index
    return None


def rms(samples: list[float]) -> float:
    return math.sqrt(sum(sample * sample for sample in samples) / len(samples)) if samples else 0.0


def decibels(ratio: float) -> float:
    return 20.0 * math.log10(ratio) if ratio > 0.0 else -math.inf


def rising_crossing_frequency(samples: list[float]) -> float:
    crossings: list[float] = []
    for index in range(1, len(samples)):
        before = samples[index - 1]
        after = samples[index]
        if before < 0.0 <= after:
            crossings.append(index - 1 + before / (before - after))
    if len(crossings) < 2:
        return 0.0
    return (len(crossings) - 1) * frame_rate / (crossings[-1] - crossings[0])


def first_sound_in(samples: list[float], start: int, end: int) -> int | None:
    for index in range(max(start, 0), min(end, len(samples))):
        if abs(samples[index]) > onset_floor:
            return index
    return None


def tone_phase(samples: list[float], pitch: float) -> float:
    step = 2.0 * math.pi * pitch / frame_rate
    in_phase = sum(sample * math.sin(step * index) for index, sample in enumerate(samples))
    quadrature = sum(sample * math.cos(step * index) for index, sample in enumerate(samples))
    return math.atan2(quadrature, in_phase)


def shadow_response(shadow: float, pitch: float, speed_of_sound: float, head_radius: float) -> complex:
    corner = 2.0 * speed_of_sound / head_radius
    turn = 1j * 2.0 * math.pi * pitch
    return 1.0 + (shadow - 1.0) * turn / (turn + corner)


def check_binaural_side(
    label: str,
    near: list[float],
    far: list[float],
    begin: int,
    end: int,
    steady: slice,
    pitch: float,
    speed_of_sound: float,
    head_radius: float,
) -> list[check]:
    near_onset = first_sound_in(near, begin, end)
    far_onset = first_sound_in(far, begin, end)
    if near_onset is None or far_onset is None:
        return [check(f"criterion 3 binaural delay: {label}", False, "one ear is silent for the whole segment")]

    delay = (far_onset - near_onset) / frame_rate
    woodworth = head_radius / speed_of_sound * (1.0 + math.pi / 2.0)
    near_level = rms(near[steady])
    far_level = rms(far[steady])
    difference = decibels(near_level / far_level) if far_level > 0.0 else math.inf

    period = 1.0 / pitch
    lag = (tone_phase(near[steady], pitch) - tone_phase(far[steady], pitch)) / (2.0 * math.pi * pitch)
    lag = (lag - interaural_delay_floor) % period + interaural_delay_floor
    near_response = shadow_response(shadow_facing, pitch, speed_of_sound, head_radius)
    far_response = shadow_response(shadow_behind, pitch, speed_of_sound, head_radius)
    predicted_lag = woodworth + (math.atan2(near_response.imag, near_response.real) - math.atan2(far_response.imag, far_response.real)) / (2.0 * math.pi * pitch)

    return [
        check(
            f"criterion 3 binaural delay: {label}",
            interaural_delay_floor <= delay <= interaural_delay_ceiling,
            f"near ear leads by {delay * 1000.0:.3f} ms (need {interaural_delay_floor * 1000.0:g} to {interaural_delay_ceiling * 1000.0:g} ms, "
            f"spherical head predicts {woodworth * 1000.0:.3f} ms); steady phase lag at {pitch:g} Hz {lag * 1000.0:.3f} ms, "
            f"model predicts {predicted_lag * 1000.0:.3f} ms (reported, not gated)",
        ),
        check(
            f"criterion 3 binaural level: {label}",
            0.0 < difference <= far_ear_audible_db,
            f"near ear {decibels(near_level):.2f} dBFS, far ear {decibels(far_level):.2f} dBFS, "
            f"difference {difference:.2f} dB (need above 0 and at most {far_ear_audible_db:g})",
        ),
    ]


def check_spatial_run(path: Path, listening: str) -> tuple[list[check], str | None]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name

    offline = offline_pattern.search(text)
    header = spatial_header_pattern.search(text)
    sync = spatial_sync_pattern.search(text)
    if not offline or not header or not sync:
        checks.append(check(f"spatial {name}", False, "missing the offline render, header or sync line"))
        return checks, None

    mode = listening_pattern.search(text)
    checks.append(check(
        f"spatial {name} listening mode",
        mode is not None and mode["mode"] == listening,
        f"ran through {mode['mode'] if mode else 'an unlogged mode'}, expected {listening}",
    ))
    if not mode:
        return checks, offline["hash"]
    head_radius = float(mode["radius"])

    speed_of_sound = float(header["speed"])
    listener = tuple(float(header[index]) for index in (2, 3, 4))
    segments = [
        spatial_segment(
            kind=match["kind"],
            frame=int(match["frame"]),
            position=tuple(float(match[index]) for index in (3, 4, 5)),
            velocity=tuple(float(match[index]) for index in (6, 7, 8)),
            pitch=float(match["pitch"]),
            frames=int(match["frames"]),
        )
        for match in spatial_segment_pattern.finditer(text)
    ]
    kinds = [segment.kind for segment in segments]
    checks.append(check(
        f"spatial {name} script",
        kinds.count("stationary") >= 3 and kinds.count("side") == 2 and kinds.count("moving") == 2,
        f"{kinds.count('stationary')} stationary, {kinds.count('side')} side, {kinds.count('moving')} moving segments",
    ))

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"spatial {name} wav", False, f"{wav_path} does not exist"))
        return checks, offline["hash"]
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"spatial {name} wav", False, f"{channels} ch at {rate}"))
        return checks, offline["hash"]
    left = samples[0::2]
    right = samples[1::2]

    sync_onset = first_sound(left, right, 0)
    if sync_onset is None:
        checks.append(check(f"spatial {name} sync", False, "the WAV is silent"))
        return checks, offline["hash"]
    sync_frame = int(sync["frame"])

    levels: list[tuple[float, float]] = []
    for segment in segments:
        offset = tuple(segment.position[axis] - listener[axis] for axis in range(3))
        distance = math.sqrt(sum(component * component for component in offset))
        begin = sync_onset + (segment.frame - sync_frame) * frames_per_step
        end = begin + segment.frames * frames_per_step
        onset = first_sound(left, right, begin)
        label = f"spatial {name} {segment.kind} at {distance:g} m"
        if onset is None or onset >= end:
            checks.append(check(label, False, "no sound inside the segment"))
            continue

        if segment.kind in ("stationary", "side"):
            expected = distance / speed_of_sound
            measured = (onset - begin) / frame_rate
            checks.append(check(
                f"criterion 3 delay: {label}",
                abs(measured - expected) <= arrival_tolerance,
                f"arrived after {measured * 1000.0:.3f} ms, expected {expected * 1000.0:.3f} ms",
            ))
            steady = slice(onset + int(level_lead_in * frame_rate), end - int(level_lead_out * frame_rate))
            left_level = rms(left[steady])
            right_level = rms(right[steady])
            if segment.kind == "stationary":
                levels.append((distance, math.hypot(left_level, right_level)))
            else:
                on_right = offset[0] > 0.0
                side_label = f"{label} on the {'right' if on_right else 'left'}"
                near, far = (right_level, left_level) if on_right else (left_level, right_level)
                checks.append(check(
                    f"criterion 3 side: {side_label}",
                    near > far,
                    f"near ear {decibels(near):.2f} dBFS, far ear {decibels(far):.2f} dBFS",
                ))
                if listening == "headphones":
                    near_ear, far_ear = (right, left) if on_right else (left, right)
                    checks += check_binaural_side(
                        side_label, near_ear, far_ear, begin, end, steady, segment.pitch, speed_of_sound, head_radius
                    )
        else:
            receding = sum(segment.velocity[axis] * offset[axis] for axis in range(3)) / distance
            expected = segment.pitch * speed_of_sound / (speed_of_sound + receding)
            window = slice(onset + int(doppler_lead_in * frame_rate), end - int(doppler_lead_out * frame_rate))
            mono = [a + b for a, b in zip(left[window], right[window])]
            measured = rising_crossing_frequency(mono)
            shift_error = abs((measured - segment.pitch) - (expected - segment.pitch)) / abs(expected - segment.pitch)
            checks.append(check(
                f"criterion 3 doppler: {label} at {-receding:+g} m/s closing",
                abs(measured - expected) <= doppler_tolerance * expected and shift_error <= doppler_tolerance,
                f"measured {measured:.3f} Hz, expected {expected:.3f} Hz, frequency off by "
                f"{abs(measured - expected) / expected * 100.0:.4f}%, shift off by {shift_error * 100.0:.3f}%",
            ))

    levels.sort()
    for (near_distance, near_level), (far_distance, far_level) in zip(levels, levels[1:]):
        doublings = math.log2(far_distance / near_distance)
        drop = decibels(near_level / far_level) / doublings if far_level > 0.0 else math.inf
        checks.append(check(
            f"criterion 3 level: {name} {near_distance:g} m to {far_distance:g} m",
            abs(drop - doubling_db) <= doubling_tolerance_db,
            f"{drop:.3f} dB per doubling ({decibels(near_level):.2f} dBFS to {decibels(far_level):.2f} dBFS)",
        ))
    return checks, offline["hash"]


def stage2(args: argparse.Namespace) -> int:
    return report_spatial("stage2", args.logs, "speakers")


def stage3(args: argparse.Namespace) -> int:
    return report_spatial("stage3", args.logs, "headphones")


def report_spatial(stage: str, logs: list[Path], listening: str) -> int:
    checks: list[check] = []
    hashes: list[str] = []
    for path in logs:
        run_checks, sample_hash = check_spatial_run(path, listening)
        checks += run_checks
        if sample_hash:
            hashes.append(sample_hash)
    checks.append(check(
        "criterion 1: same hash in every run",
        len(hashes) == len(logs) and len(set(hashes)) == 1,
        ", ".join(hashes) if hashes else "no hashes",
    ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"{stage}: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


occlusion_header_pattern = re.compile(
    r"audio_occlusion_probe: emitter at " + vector_text + r", low (?P<low>[0-9.]+) Hz, high (?P<high>[0-9.]+) Hz"
)
occlusion_sync_pattern = re.compile(r"audio_occlusion_probe: sync frame (?P<frame>\d+) pitch (?P<pitch>[0-9.]+) Hz")
occlusion_segment_pattern = re.compile(
    r"audio_occlusion_probe: segment (?P<kind>\w+) frame (?P<frame>\d+) listener " + vector_text
    + r" emitter " + vector_text + r" velocity " + vector_text + r" lead (?P<lead>\d+) travel (?P<travel>\d+) tail (?P<tail>\d+)"
)

occlusion_drop_db = 6.0
occlusion_tilt_db = 6.0
occlusion_block_step_db = 1.0
occlusion_endpoint_db = 0.5
occlusion_surface_db = 0.5
occlusion_shapes = ("sphere", "capsule", "hull")
occlusion_segments = {"reference", "occluded", "walk", "floor", *occlusion_shapes}
occlusion_block = frame_rate // 100


def tone_level(samples: list[float], pitch: float) -> float:
    step = 2.0 * math.pi * pitch / frame_rate
    in_phase = sum(sample * math.sin(step * index) for index, sample in enumerate(samples))
    quadrature = sum(sample * math.cos(step * index) for index, sample in enumerate(samples))
    return math.hypot(in_phase, quadrature) * math.sqrt(2.0) / len(samples) if samples else 0.0


def both_ears(left: list[float], right: list[float], window: slice, level) -> float:
    return math.hypot(level(left[window]), level(right[window]))


def whole_blocks(begin: int, end: int) -> list[slice]:
    return [slice(start, start + occlusion_block) for start in range(begin, end - occlusion_block + 1, occlusion_block)]


def largest_step(levels: list[float]) -> float:
    steps = [abs(decibels(after / before)) if before > 0.0 and after > 0.0 else math.inf for before, after in zip(levels, levels[1:])]
    return max(steps, default=math.inf)


def check_occlusion_run(path: Path, listening: str) -> tuple[list[check], str | None]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name

    offline = offline_pattern.search(text)
    header = occlusion_header_pattern.search(text)
    sync = occlusion_sync_pattern.search(text)
    if not offline or not header or not sync:
        checks.append(check(f"occlusion {name}", False, "missing the offline render, header or sync line"))
        return checks, None

    mode = listening_pattern.search(text)
    checks.append(check(
        f"occlusion {name} listening mode",
        mode is not None and mode["mode"] == listening,
        f"ran through {mode['mode'] if mode else 'an unlogged mode'}, expected {listening}",
    ))
    dropped = len(dropped_pattern.findall(text))
    checks.append(check(f"occlusion {name} voices", dropped == 0, f"{dropped} dropped"))

    low = float(header["low"])
    high = float(header["high"])
    segments = {match["kind"]: match for match in occlusion_segment_pattern.finditer(text)}
    if set(segments) != occlusion_segments:
        checks.append(check(f"occlusion {name} script", False, f"segments logged: {sorted(segments)}"))
        return checks, offline["hash"]

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"occlusion {name} wav", False, f"{wav_path} does not exist"))
        return checks, offline["hash"]
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"occlusion {name} wav", False, f"{channels} ch at {rate}"))
        return checks, offline["hash"]
    left = samples[0::2]
    right = samples[1::2]

    sync_onset = first_sound(left, right, 0)
    if sync_onset is None:
        checks.append(check(f"occlusion {name} sync", False, "the WAV is silent"))
        return checks, offline["hash"]
    sync_frame = int(sync["frame"])

    blocks: dict[str, list[slice]] = {}
    for kind, match in segments.items():
        begin = sync_onset + (int(match["frame"]) - sync_frame) * frames_per_step
        end = begin + (int(match["lead"]) + int(match["travel"]) + int(match["tail"])) * frames_per_step
        onset = first_sound(left, right, begin)
        if onset is None or onset >= end:
            checks.append(check(f"occlusion {name} {kind}", False, "no sound inside the segment"))
            return checks, offline["hash"]
        blocks[kind] = whole_blocks(onset + int(level_lead_in * frame_rate), end - int(level_lead_out * frame_rate))

    def steady(kind: str) -> slice:
        return slice(blocks[kind][0].start, blocks[kind][-1].stop)

    def band(pitch: float):
        return lambda window: tone_level(window, pitch)

    reference = both_ears(left, right, steady("reference"), rms)
    occluded = both_ears(left, right, steady("occluded"), rms)
    drop = decibels(reference / occluded) if occluded > 0.0 else math.inf
    checks.append(check(
        f"criterion 4 level: {name}",
        drop >= occlusion_drop_db,
        f"behind the wall {decibels(occluded):.2f} dBFS against {decibels(reference):.2f} dBFS in the clear, "
        f"a drop of {drop:.2f} dB (need {occlusion_drop_db:g})",
    ))

    band_drops: dict[float, float] = {}
    for pitch in (low, high):
        clear = both_ears(left, right, steady("reference"), band(pitch))
        hidden = both_ears(left, right, steady("occluded"), band(pitch))
        band_drops[pitch] = decibels(clear / hidden) if hidden > 0.0 else math.inf
    tilt = band_drops[high] - band_drops[low]
    checks.append(check(
        f"criterion 4 tilt: {name}",
        low < 250.0 and high > 4000.0 and tilt >= occlusion_tilt_db,
        f"{high:g} Hz drops {band_drops[high]:.2f} dB, {low:g} Hz drops {band_drops[low]:.2f} dB, "
        f"{tilt:.2f} dB more above 4 kHz than below 250 Hz (need {occlusion_tilt_db:g})",
    ))

    walk_levels = [both_ears(left, right, window, rms) for window in blocks["walk"]]
    walk_step = largest_step(walk_levels)
    band_steps = {
        pitch: largest_step([both_ears(left, right, window, band(pitch)) for window in blocks["walk"]])
        for pitch in (low, high)
    }
    checks.append(check(
        f"criterion 4 smoothness: {name}",
        walk_step <= occlusion_block_step_db,
        f"largest change between 10 ms blocks {walk_step:.3f} dB over {len(walk_levels)} blocks (limit {occlusion_block_step_db:g}); "
        f"{low:g} Hz alone {band_steps[low]:.3f} dB, {high:g} Hz alone {band_steps[high]:.3f} dB (reported, not gated)",
    ))

    starts = decibels(walk_levels[0] / occluded) if occluded > 0.0 else math.inf
    ends = decibels(walk_levels[-1] / reference) if reference > 0.0 else math.inf
    checks.append(check(
        f"occlusion {name} walk endpoints",
        abs(starts) <= occlusion_endpoint_db and abs(ends) <= occlusion_endpoint_db,
        f"walk starts {starts:+.2f} dB from the occluded level and ends {ends:+.2f} dB from the clear level "
        f"(limit {occlusion_endpoint_db:g})",
    ))

    resting = both_ears(left, right, steady("floor"), rms)
    resting_high = both_ears(left, right, steady("floor"), band(high))
    clear_high = both_ears(left, right, steady("reference"), band(high))
    surface = decibels(resting / reference) if resting > 0.0 else -math.inf
    surface_high = decibels(resting_high / clear_high) if resting_high > 0.0 else -math.inf
    checks.append(check(
        f"occlusion {name} source on a floor",
        abs(surface) <= occlusion_surface_db and abs(surface_high) <= occlusion_surface_db,
        f"resting on a floor with a clear line {surface:+.2f} dB from the floating source, {high:g} Hz alone {surface_high:+.2f} dB "
        f"(limit {occlusion_surface_db:g})",
    ))

    for shape in occlusion_shapes:
        hidden = both_ears(left, right, steady(shape), rms)
        shape_drop = decibels(reference / hidden) if hidden > 0.0 else math.inf
        checks.append(check(
            f"occlusion {name} behind a {shape}",
            shape_drop >= occlusion_drop_db,
            f"a drop of {shape_drop:.2f} dB (need {occlusion_drop_db:g})",
        ))
    return checks, offline["hash"]


def stage4(args: argparse.Namespace) -> int:
    checks: list[check] = []
    hashes: list[str] = []
    for path in args.logs:
        run_checks, sample_hash = check_occlusion_run(path, args.listening)
        checks += run_checks
        if sample_hash:
            hashes.append(sample_hash)
    checks.append(check(
        "criterion 1: same hash in every run",
        len(hashes) == len(args.logs) and len(set(hashes)) == 1,
        ", ".join(hashes) if hashes else "no hashes",
    ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage4: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


contact_number = r"(-?[0-9.]+)"
contact_point_text = r"\(" + contact_number + r" m, " + contact_number + r" m, " + contact_number + r" m\)"
contact_header_pattern = re.compile(
    r"audio_contact_probe: speed of sound (?P<speed>[0-9.]+) m/s, listener at " + contact_point_text
    + r", fixed step [0-9.]+ ms, (?P<sounds>\d+) contact sounds"
)
contact_sync_pattern = re.compile(r"audio_contact_probe: sync frame (?P<frame>\d+) pitch [0-9.]+ Hz")
contact_faller_pattern = re.compile(
    r"audio_contact_probe: faller (?P<index>\d+) body \[\d+: (?P<body>[^\]]+)\] spawned frame (?P<frame>\d+) height (?P<height>[0-9.]+) m"
)
contact_arrest_pattern = re.compile(
    r"audio_contact_probe: faller (?P<index>\d+) arrested frame (?P<frame>\d+) after falling at (?P<speed>[0-9.]+) m/s"
)
contact_rest_pattern = re.compile(r"audio_contact_probe: rest window frame (?P<frame>\d+) frames (?P<frames>\d+)")
contact_event_pattern = re.compile(
    r"audio: contact step (?P<step>\d+) bodies \[\d+: (?P<a>[^\]]+)\] \[\d+: (?P<b>[^\]]+)\] impulse (?P<impulse>-?[0-9.]+) N-s "
    r"speed (?P<speed>-?[0-9.]+) m/s point " + contact_point_text
    + r" outcome (?P<outcome>[a-z ]+), sink frame (?P<sink>\d+), physics step (?P<physics>\d+)"
)

contact_floor = "AudioContactFloor"
contact_fallers = 4
contact_gravity = 9.81
contact_speed_tolerance = 0.02
contact_single_steps = 6
contact_latency_steps = 1
contact_onset_frames = 96
contact_rest_frames = 300


@dataclass
class contact_event:
    step: int
    bodies: frozenset[str]
    impulse: float
    speed: float
    point: tuple[float, float, float]
    outcome: str
    sink: int
    physics: int


def read_contact_events(text: str) -> list[contact_event]:
    return [
        contact_event(
            step=int(match["step"]),
            bodies=frozenset((match["a"], match["b"])),
            impulse=float(match["impulse"]),
            speed=float(match["speed"]),
            point=(float(match[6]), float(match[7]), float(match[8])),
            outcome=match["outcome"],
            sink=int(match["sink"]),
            physics=int(match["physics"]),
        )
        for match in contact_event_pattern.finditer(text)
    ]


def check_contact_run(path: Path, solver: str) -> tuple[list[check], str | None]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name
    gated = solver == "cpu"
    measured = "" if gated else " (recorded, not gated on the gpu solver)"
    estimated = "" if gated else " (estimated from the velocity step across one readback)"

    offline = offline_pattern.search(text)
    header = contact_header_pattern.search(text)
    sync = contact_sync_pattern.search(text)
    rest = contact_rest_pattern.search(text)
    if not offline or not header or not sync or not rest:
        checks.append(check(f"contact {name}", False, "missing the offline render, header, sync or rest window line"))
        return checks, None

    fallers = {int(match["index"]): match for match in contact_faller_pattern.finditer(text)}
    arrests = {int(match["index"]): match for match in contact_arrest_pattern.finditer(text)}
    events = read_contact_events(text)
    dropped = len(dropped_pattern.findall(text))
    checks.append(check(
        f"contact {name} script",
        len(fallers) == contact_fallers and len(arrests) == contact_fallers and dropped == 0 and int(rest["frames"]) == contact_rest_frames,
        f"{len(fallers)} fallers spawned, {len(arrests)} arrested, {len(events)} contact events traced, {dropped} sounds dropped, "
        f"rest window of {rest['frames']} frames, sample hash {offline['hash']}",
    ))

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"contact {name} wav", False, f"{wav_path} does not exist"))
        return checks, offline["hash"]
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"contact {name} wav", False, f"{channels} ch at {rate}"))
        return checks, offline["hash"]
    left = samples[0::2]
    right = samples[1::2]

    sync_onset = first_sound(left, right, 0)
    if sync_onset is None:
        checks.append(check(f"contact {name} sync", False, "the WAV is silent"))
        return checks, offline["hash"]
    sync_frame = int(sync["frame"])
    sync_block = sync_onset - sync_onset % frames_per_step

    def sink_of(frame: int) -> int:
        return sync_block + (frame - sync_frame) * frames_per_step

    speed_of_sound = float(header["speed"])
    listener = tuple(float(header[index]) for index in (2, 3, 4))

    for index in sorted(fallers):
        body = fallers[index]["body"]
        label = f"contact {name} faller {index}"
        pair = frozenset((body, contact_floor))
        on_floor = [event for event in events if event.bodies == pair]
        if not on_floor:
            checks.append(check(f"{label} first impact", False, f"no event names {body} and {contact_floor}"))
            continue
        first = on_floor[0]
        close = [event for event in on_floor if event.step - first.step <= contact_single_steps]
        checks.append(check(
            f"{label} first impact",
            len(close) == 1 and first.impulse > 0.0 and first.speed > 0.0,
            f"step {first.step}, bodies {' and '.join(sorted(first.bodies))}, impulse {first.impulse:.3f} N-s{estimated}, relative speed {first.speed:.3f} m/s; "
            f"{len(close)} event in its first {contact_single_steps} steps (need 1), {len(on_floor) - 1} later for the pair (bounces, not gated)",
        ))

        height = float(fallers[index]["height"])
        predicted = math.sqrt(2.0 * contact_gravity * height)
        speed_error = first.speed / predicted - 1.0
        checks.append(check(
            f"{label} relative speed",
            not gated or abs(speed_error) <= contact_speed_tolerance,
            f"{first.speed:.3f} m/s against {predicted:.3f} m/s from a {height:g} m drop, off by {100.0 * speed_error:+.2f}% "
            f"(limit {100.0 * contact_speed_tolerance:g}%){measured}",
        ))

        delay = round(math.dist(first.point, listener) / speed_of_sound * frame_rate)
        quiet_before = first_sound_in(left, first.sink + delay - frames_per_step, first.sink + delay - contact_onset_frames) is None and \
            first_sound_in(right, first.sink + delay - frames_per_step, first.sink + delay - contact_onset_frames) is None
        onset = first_sound(left, right, first.sink + delay - contact_onset_frames)
        late = onset - first.sink - delay if onset is not None else None
        waited = first.physics - first.step
        arrest = arrests.get(index)
        after_arrest = (first.sink - sink_of(int(arrest["frame"]))) / frames_per_step if arrest else None
        latency_ok = (
            first.outcome == "started" and quiet_before and late is not None and abs(late) <= contact_onset_frames
            and (not gated or (waited <= contact_latency_steps and after_arrest is not None and abs(after_arrest) <= contact_latency_steps))
        )
        checks.append(check(
            f"{label} event to sound",
            latency_ok,
            f"outcome {first.outcome}; received {waited} fixed step(s) after the event's step (limit {contact_latency_steps}){measured}; "
            f"voice starts at sink frame {first.sink}, "
            + (f"{after_arrest:+.2f} steps from the frame the scenario saw the body arrested, " if after_arrest is not None else "no arrest logged, ")
            + (f"heard {late:+d} frames from that start plus {delay} frames of travel (limit {contact_onset_frames})" if late is not None else "never heard")
            + ("" if quiet_before else "; the WAV was not quiet just before it"),
        ))

    window_begin = sink_of(int(rest["frame"]))
    window_end = sink_of(int(rest["frame"]) + int(rest["frames"]))
    resting = [event for event in events if window_begin <= event.sink < window_end]
    checks.append(check(
        f"contact {name} resting bodies",
        not resting and window_end <= len(left),
        f"{len(resting)} events in the {rest['frames']}-frame rest window (sink frames {window_begin} to {window_end}, WAV holds {len(left)}), "
        f"counted before the audibility thresholds"
        + ("".join(f"; step {event.step} {' and '.join(sorted(event.bodies))} {event.outcome}" for event in resting[:4])),
    ))
    return checks, offline["hash"]


def stage5(args: argparse.Namespace) -> int:
    checks: list[check] = []
    hashes: list[str] = []
    for path in args.logs:
        run_checks, sample_hash = check_contact_run(path, args.solver)
        checks += run_checks
        if sample_hash:
            hashes.append(sample_hash)
    if args.solver == "cpu":
        checks.append(check(
            "criterion 1: same hash in every run",
            len(hashes) == len(args.logs) and len(set(hashes)) == 1,
            ", ".join(hashes) if hashes else "no hashes",
        ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage5: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


yard_solo_pattern = re.compile(
    r"audio_yard_solo: frame (?P<frame>\d+) emitter " + vector_text + r" listener " + vector_text
    + r" clip (?P<clip>\S+) gain (?P<gain>[0-9.]+)"
)

yard_settle = 0.2
yard_level_loops = 2
yard_level_db = 0.5
yard_balance_db = 0.1
yard_peak_headroom = 0.75
yard_repeat_db = 60.0
yard_band_floor_db = 40.0
yard_band_db = 0.5
yard_band_top_loss_db = 1.5
yard_band_tight_ceiling = 8000.0
yard_band_centers = [1000.0 * 2.0 ** (index / 3.0) for index in range(-10, 13)]


def read_clip(path: Path) -> list[float]:
    with wave.open(str(path), "rb") as clip:
        if clip.getnchannels() != 1 or clip.getsampwidth() != 2 or clip.getframerate() != frame_rate:
            raise ValueError(f"{path} is not mono 16-bit at {frame_rate} Hz")
        frames = clip.readframes(clip.getnframes())
    return [sample / 32768.0 for (sample,) in struct.iter_unpack("<h", frames)]


def smallest_factor(count: int) -> int:
    factor = 2
    while factor * factor <= count:
        if count % factor == 0:
            return factor
        factor += 1
    return count


def spectrum(samples: list[float]) -> list[complex]:
    count = len(samples)
    turns = [complex(math.cos(2.0 * math.pi * index / count), -math.sin(2.0 * math.pi * index / count)) for index in range(count)]

    def transform(values: list[float]) -> list[complex]:
        size = len(values)
        if size == 1:
            return [complex(values[0])]
        radix = smallest_factor(size)
        part = size // radix
        stride = count // size
        parts = [transform(values[offset::radix]) for offset in range(radix)]
        return [
            sum(turns[(offset * bin_index % size) * stride] * parts[offset][bin_index % part] for offset in range(radix))
            for bin_index in range(size)
        ]

    return transform(samples)


def band_powers(samples: list[float]) -> list[float]:
    bins = spectrum(samples)
    count = len(samples)
    width = 2.0 ** (1.0 / 6.0)
    powers: list[float] = []
    for center in yard_band_centers:
        first = math.ceil(center / width * count / frame_rate)
        last = min(math.ceil(center * width * count / frame_rate), count // 2)
        powers.append(2.0 * sum(abs(bins[index]) ** 2 for index in range(first, last)) / (count * count))
    return powers


def power_decibels(ratio: float) -> float:
    return 10.0 * math.log10(ratio) if ratio > 0.0 else -math.inf


def check_yard_run(path: Path, clips: dict[str, tuple[list[float], list[float]]]) -> list[check]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name

    offline = offline_pattern.search(text)
    solo = yard_solo_pattern.search(text)
    if not offline or not solo:
        checks.append(check(f"yard {name}", False, "missing the offline render or audio_yard_solo line"))
        return checks

    clip_name = solo["clip"]
    label = f"yard {name} {clip_name}"
    mode = listening_pattern.search(text)
    dropped = len(dropped_pattern.findall(text))
    started = int(offline["started"])
    checks.append(check(
        f"{label} voices",
        started == 1 and dropped == 0 and mode is not None and mode["mode"] == "speakers",
        f"{started} started (need 1), {dropped} dropped, listening through {mode['mode'] if mode else 'an unlogged mode'}, "
        f"sample hash {offline['hash']} (reported, not gated)",
    ))

    if clip_name not in clips:
        clip_path = REPO_ROOT / "Sandbox" / "Assets" / f"{clip_name}.wav"
        if not clip_path.exists():
            checks.append(check(f"{label} clip", False, f"{clip_path} does not exist"))
            return checks
        clip_samples = read_clip(clip_path)
        clips[clip_name] = (clip_samples, band_powers(clip_samples))
    clip, clip_bands = clips[clip_name]
    loop = len(clip)

    inner_step = max(abs(after - before) for before, after in zip(clip, clip[1:]))
    seam_step = abs(clip[0] - clip[-1])
    checks.append(check(
        f"{label} loop seam",
        seam_step <= inner_step,
        f"the clip steps {seam_step:.5f} across its loop point, {inner_step:.5f} at most inside ({loop} frames)",
    ))

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"{label} wav", False, f"{wav_path} does not exist"))
        return checks
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"{label} wav", False, f"{channels} ch at {rate}"))
        return checks
    left = samples[0::2]
    right = samples[1::2]

    onset = first_sound(left, right, 0)
    begin = onset + int(yard_settle * frame_rate) if onset is not None else 0
    if onset is None or begin + (yard_level_loops + 1) * loop > len(left):
        checks.append(check(f"{label} wav", False, f"fewer than {yard_level_loops + 1} loops of sound after frame {onset}"))
        return checks

    gain = float(solo["gain"])
    emitter = tuple(float(solo[index]) for index in (2, 3, 4))
    listener = tuple(float(solo[index]) for index in (5, 6, 7))
    distance = math.dist(emitter, listener)
    reach = gain / max(distance, 1.0)

    level_window = slice(begin, begin + yard_level_loops * loop)
    left_level = rms(left[level_window])
    right_level = rms(right[level_window])
    heard = math.hypot(left_level, right_level)
    expected = rms(clip) * reach
    level_error = decibels(heard / expected) if heard > 0.0 else -math.inf
    checks.append(check(
        f"{label} level",
        abs(level_error) <= yard_level_db,
        f"{decibels(heard):.2f} dBFS at {distance:.3f} m with gain {gain:g}, the clip predicts {decibels(expected):.2f} dBFS, "
        f"off by {level_error:+.3f} dB (limit {yard_level_db:g})",
    ))

    balance = decibels(left_level / right_level) if left_level > 0.0 and right_level > 0.0 else math.inf
    checks.append(check(
        f"{label} balance",
        abs(balance) <= yard_balance_db,
        f"left against right {balance:+.4f} dB (limit {yard_balance_db:g})",
    ))

    finite = all(math.isfinite(sample) for sample in samples)
    peak = max((abs(sample) for sample in samples), default=0.0)
    peak_limit = max(abs(sample) for sample in clip) * reach * yard_peak_headroom
    checks.append(check(
        f"{label} no clipping",
        finite and peak <= peak_limit and peak < 1.0,
        f"peak {peak:.4f}, limit {peak_limit:.4f}, every sample finite: {finite}",
    ))

    one_loop = slice(begin, begin + loop)
    next_loop = slice(begin + loop, begin + 2 * loop)
    drifts = [
        rms([after - before for before, after in zip(ear[one_loop], ear[next_loop])])
        for ear in (left, right)
    ]
    drift = math.hypot(*drifts)
    loop_level = both_ears(left, right, one_loop, rms)
    repeat = decibels(loop_level / drift) if drift > 0.0 else math.inf
    checks.append(check(
        f"{label} repeats",
        repeat >= yard_repeat_db,
        f"one loop differs from the next by {-repeat:.1f} dB of its level (need {-yard_repeat_db:g} or lower)",
    ))

    heard_bands = [a + b for a, b in zip(band_powers(left[one_loop]), band_powers(right[one_loop]))]
    clip_total = sum(clip_bands)
    heard_total = sum(heard_bands)
    misses: list[str] = []
    widest = 0.0
    stray = -math.inf
    for center, clip_band, heard_band in zip(yard_band_centers, clip_bands, heard_bands):
        if power_decibels(clip_band / clip_total) > -yard_band_floor_db:
            error = power_decibels(heard_band / (clip_band * reach * reach))
            floor = -yard_band_db if center <= yard_band_tight_ceiling else -yard_band_top_loss_db
            widest = max(widest, abs(error))
            if not floor <= error <= yard_band_db:
                misses.append(f"{center:.0f} Hz {error:+.2f} dB")
        else:
            level = power_decibels(heard_band / heard_total)
            stray = max(stray, level)
            if level > -yard_band_floor_db:
                misses.append(f"{center:.0f} Hz holds {level:.1f} dB of the output where the clip is empty")
    checks.append(check(
        f"{label} spectrum",
        not misses,
        f"{len(yard_band_centers)} third-octave bands, largest error {widest:.3f} dB, loudest band the clip leaves empty {stray:.1f} dB; "
        + (", ".join(misses) if misses else "all within limits"),
    ))
    return checks


def yard(args: argparse.Namespace) -> int:
    clips: dict[str, tuple[list[float], list[float]]] = {}
    checks: list[check] = []
    for path in args.logs:
        checks += check_yard_run(path, clips)
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"yard: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


modal_header_pattern = re.compile(
    r"audio_modal_probe: speed of sound (?P<speed>[0-9.]+) m/s, listener at " + vector_text + r", fixed step [0-9.]+ ms"
)
modal_bar_pattern = re.compile(
    r"audio_modal_probe: bar (?P<index>\d+) body \[\d+: (?P<body>[^\]]+)\] spawned frame (?P<frame>\d+) height (?P<height>[0-9.]+) m "
    r"tilt (?P<tilt>[0-9.]+) deg at " + vector_text
)
modal_voice_pattern = re.compile(
    r"audio: modal voice body \[\d+: (?P<body>[^\]]+)\] material \[\d+: (?P<material>[^\]]+)\] box " + vector_text
    + r" density (?P<density>[0-9.]+) kg/m\^3 young modulus (?P<young>[0-9.]+) GPa poisson ratio (?P<poisson>[0-9.]+)"
    r" mass damping (?P<mass_damping>[0-9.]+) 1/s stiffness damping (?P<stiffness_damping>[0-9.]+) ns, (?P<modes>\d+) modes"
)
modal_mode_pattern = re.compile(
    r"audio: modal mode (?P<index>\d+) body \[\d+: (?P<body>[^\]]+)\] frequency (?P<frequency>[0-9.]+) Hz decay (?P<decay>[0-9.]+) 1/s"
    r" axis share \(([0-9.]+), ([0-9.]+), ([0-9.]+)\)"
)

modal_floor = "AudioModalFloor"
modal_flat_bars = 5
modal_oblique_bars = 1
modal_gravity = 9.81
modal_speed_tolerance = 0.02
modal_free_beam_roots = (4.730041, 7.853205, 10.995608, 14.137165)
modal_bending_share = 0.9
modal_baked_tolerance = 0.05
modal_peak_tolerance = 0.01
modal_decay_tolerance = 0.10
modal_model_tolerance = 0.001
modal_block_frames = 2400
modal_phase_hop_frames = 240
modal_settle_frames = 480
modal_decay_blocks = 16
modal_decay_span = math.log(100.0)
modal_separation = 150.0
modal_fastest_decay = 60.0
modal_excited_db = -60.0
modal_level_frames = 4800
modal_shortest_level_frames = 480
modal_radius_tolerance = 0.001
modal_sounding = ("started", "rang")
modal_materials = ("Acoustics/steel", "Acoustics/wood", "Acoustics/glass")
modal_ringing_needed = 64
modal_ringing_share = 0.75
modal_ringing_pattern = re.compile(
    r"audio: modal voices peaked at (?P<peak>\d+) of (?P<limit>\d+), at the limit on (?P<at_limit>\d+) of (?P<frames>\d+) frames"
)


@dataclass
class baked_mode:
    index: int
    frequency: float
    decay: float
    share: tuple[float, float, float]


def demodulate(samples: list[float], start: int, frequency: float, window: list[float]) -> complex:
    step = cmath.exp(-2j * math.pi * frequency / frame_rate)
    turn = cmath.exp(-2j * math.pi * frequency * start / frame_rate)
    total = 0j
    for offset, weight in enumerate(window):
        total += samples[start + offset] * weight * turn
        turn *= step
    return total


def fitted_decay(magnitudes: list[float], hop: int) -> float:
    times = [index * hop / frame_rate for index in range(len(magnitudes))]
    logs = [math.log(value) for value in magnitudes]
    mean_time = sum(times) / len(times)
    mean_log = sum(logs) / len(logs)
    spread = sum((time - mean_time) ** 2 for time in times)
    return -sum((time - mean_time) * (log - mean_log) for time, log in zip(times, logs)) / spread


def check_modal_run(path: Path) -> tuple[list[check], str | None]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name

    offline = offline_pattern.search(text)
    header = modal_header_pattern.search(text)
    if not offline or not header:
        checks.append(check(f"modal {name}", False, "missing the offline render or header line"))
        return checks, None

    bars = {int(match["index"]): match for match in modal_bar_pattern.finditer(text)}
    voices = list(modal_voice_pattern.finditer(text))
    events = read_contact_events(text)
    dropped = len(dropped_pattern.findall(text))
    listener = tuple(float(header[index]) for index in (2, 3, 4))
    speed_of_sound = float(header["speed"])

    radii = [math.hypot(float(bar[6]) - listener[0], float(bar[8]) - listener[2]) for bar in bars.values()]
    voiced = sorted(voice["body"] for voice in voices)
    expected = sorted(bar["body"] for bar in bars.values())
    flat = sorted(index for index, bar in bars.items() if float(bar["tilt"]) == 0.0)
    scripted = len(flat) == modal_flat_bars and len(bars) == modal_flat_bars + modal_oblique_bars
    checks.append(check(
        f"modal {name} script",
        scripted and voiced == expected and dropped == 0 and max(radii) - min(radii) <= modal_radius_tolerance,
        f"{len(flat)} bars dropped flat and {len(bars) - len(flat)} tilted, {len(voices)} modal voices started (need one per bar), {dropped} sounds dropped, "
        f"bars {min(radii):.4f} to {max(radii):.4f} m from the listener across the floor, sample hash {offline['hash']}",
    ))
    if not scripted or voiced != expected:
        return checks, offline["hash"]

    voice = voices[0]
    size = tuple(float(voice[index]) for index in (3, 4, 5))
    density = float(voice["density"])
    young = float(voice["young"]) * 1e9
    mass_damping = float(voice["mass_damping"])
    stiffness_damping = float(voice["stiffness_damping"]) * 1e-9
    modes = [
        baked_mode(int(match["index"]), float(match["frequency"]), float(match["decay"]), (float(match[5]), float(match[6]), float(match[7])))
        for match in modal_mode_pattern.finditer(text)
        if match["body"] == voice["body"]
    ]
    checks.append(check(
        f"modal {name} bake",
        len(modes) == int(voice["modes"]) and len(modes) > 0,
        f"material {voice['material']}, box {size[0]:g} x {size[1]:g} x {size[2]:g} m, density {density:g} kg/m^3, Young's modulus {young / 1e9:g} GPa, "
        f"Poisson ratio {voice['poisson']}, mass damping {mass_damping:g} 1/s, stiffness damping {stiffness_damping * 1e9:g} ns, {len(modes)} modes logged",
    ))
    if not modes:
        return checks, offline["hash"]

    thin = min(range(3), key=lambda axis: size[axis])
    long = max(size)
    gyration = size[thin] / math.sqrt(12.0)
    bending = sorted((mode for mode in modes if mode.share[thin] >= modal_bending_share), key=lambda mode: mode.frequency)[: len(modal_free_beam_roots)]
    for order, root in enumerate(modal_free_beam_roots):
        analytic = root * root / (2.0 * math.pi * long * long) * math.sqrt(young / density) * gyration
        if order >= len(bending):
            checks.append(check(f"modal {name} bending mode {order + 1} baked", False, f"no baked mode moves along the thin axis for the analytic {analytic:.2f} Hz"))
            continue
        error = bending[order].frequency / analytic - 1.0
        checks.append(check(
            f"modal {name} bending mode {order + 1} baked",
            abs(error) <= modal_baked_tolerance,
            f"baked mode {bending[order].index} at {bending[order].frequency:.2f} Hz against the free-free beam's {analytic:.2f} Hz, "
            f"off by {100.0 * error:+.2f}% (limit {100.0 * modal_baked_tolerance:g}%)",
        ))

    def modelled_decay(mode: baked_mode) -> float:
        return 0.5 * (mass_damping + stiffness_damping * (2.0 * math.pi * mode.frequency) ** 2)

    worst_model = max(abs(mode.decay / modelled_decay(mode) - 1.0) for mode in modes)
    checks.append(check(
        f"modal {name} damping model",
        worst_model <= modal_model_tolerance,
        f"logged decay rates follow (mass damping + stiffness damping x angular frequency squared) / 2 within {100.0 * worst_model:.4f}% "
        f"(limit {100.0 * modal_model_tolerance:g}%); slowest mode T60 {math.log(1000.0) / min(mode.decay for mode in modes):.3f} s",
    ))

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"modal {name} wav", False, f"{wav_path} does not exist"))
        return checks, offline["hash"]
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"modal {name} wav", False, f"{channels} ch at {rate}"))
        return checks, offline["hash"]
    left = samples[0::2]
    right = samples[1::2]
    mono = [a + b for a, b in zip(left, right)]

    def separated(mode: baked_mode) -> bool:
        return all(abs(other.frequency - mode.frequency) >= modal_separation for other in modes if other.index != mode.index)

    measurable = [mode for mode in modes if separated(mode) and mode.decay <= modal_fastest_decay]
    window = [0.5 - 0.5 * math.cos(2.0 * math.pi * (index + 0.5) / modal_block_frames) for index in range(modal_block_frames)]

    sounding: dict[int, list[contact_event]] = {}
    for index, bar in bars.items():
        pair = frozenset((bar["body"], modal_floor))
        sounding[index] = [event for event in events if event.bodies == pair and event.outcome in modal_sounding]

    first_strike = min((struck[0].sink for struck in sounding.values() if struck), default=len(left))
    peak = max((abs(sample) for sample in samples[first_strike * 2 :]), default=0.0)
    checks.append(check(
        f"modal {name} headroom",
        0.0 < peak < 1.0,
        f"largest sample from the first strike on {decibels(peak):.2f} dBFS" if peak > 0.0 else "silent after the first strike",
    ))

    levels: list[tuple[float, float, float]] = []
    heard: set[int] = set()
    for index in sorted(bars):
        label = f"modal {name} bar {index}"
        struck = sounding[index]
        if not struck or struck[0].outcome != "started":
            checks.append(check(f"{label} impact", False, "no contact event started a modal voice for this bar"))
            continue
        first = struck[0]
        delay = round(math.dist(first.point, listener) / speed_of_sound * frame_rate)

        level_end = first.sink + delay + modal_level_frames
        if len(struck) > 1:
            level_end = min(level_end, struck[1].sink + delay)
        level_begin = first.sink + delay
        if level_end - level_begin < modal_shortest_level_frames:
            checks.append(check(f"{label} impact", False, f"only {level_end - level_begin} frames before the next strike, too few to take a level"))
            continue
        power = sum(left[frame] ** 2 + right[frame] ** 2 for frame in range(level_begin, level_end)) / (level_end - level_begin)
        if index in flat:
            height = float(bars[index]["height"])
            predicted = math.sqrt(2.0 * modal_gravity * height)
            speed_error = first.speed / predicted - 1.0
            levels.append((height, first.speed, power_decibels(power)))
            checks.append(check(
                f"{label} impact",
                abs(speed_error) <= modal_speed_tolerance,
                f"struck at {first.speed:.3f} m/s against {predicted:.3f} m/s from a {height:g} m drop, off by {100.0 * speed_error:+.2f}% "
                f"(limit {100.0 * modal_speed_tolerance:g}%); level {power_decibels(power):.2f} dBFS over {(level_end - level_begin) / frame_rate * 1000.0:.0f} ms",
            ))

        ring_begin = struck[-1].sink + delay + modal_settle_frames
        later = [sounding[other][0].sink for other in bars if other > index and sounding[other]]
        ring_end = min(later) if later else len(mono)
        clear = ring_end - ring_begin - modal_block_frames - modal_phase_hop_frames
        if clear <= 0:
            checks.append(check(f"{label} ring", False, f"no clear window after its last strike at sink frame {struck[-1].sink}"))
            continue

        starts: dict[int, tuple[float, float]] = {}
        for mode in measurable:
            early = demodulate(mono, ring_begin, mode.frequency, window)
            later_block = demodulate(mono, ring_begin + modal_phase_hop_frames, mode.frequency, window)
            if abs(early) == 0.0 or abs(later_block) == 0.0:
                continue
            offset = cmath.phase(later_block * early.conjugate()) * frame_rate / (2.0 * math.pi * modal_phase_hop_frames)
            starts[mode.index] = (abs(early), mode.frequency + offset)
        if not starts:
            checks.append(check(f"{label} ring", False, "no baked mode is present after the last strike"))
            continue
        strongest = max(magnitude for magnitude, _ in starts.values())

        quiet: list[str] = []
        for mode in measurable:
            if mode.index not in starts:
                quiet.append(f"{mode.frequency:.0f} Hz")
                continue
            magnitude, measured = starts[mode.index]
            relative = decibels(magnitude / strongest)
            if relative < modal_excited_db:
                quiet.append(f"{mode.frequency:.0f} Hz ({relative:.0f} dB)")
                continue
            heard.add(mode.index)

            span_frames = min(int(modal_decay_span / mode.decay * frame_rate), clear)
            hop = max(1, span_frames // (modal_decay_blocks - 1))
            magnitudes = [abs(demodulate(mono, ring_begin + block * hop, mode.frequency, window)) for block in range(modal_decay_blocks)]
            if min(magnitudes) <= 0.0:
                checks.append(check(f"{label} mode {mode.index}", False, f"{mode.frequency:.2f} Hz fell silent inside its decay window"))
                continue
            decay = fitted_decay(magnitudes, hop)
            peak_error = measured / mode.frequency - 1.0
            decay_time = math.log(1000.0) / decay if decay > 0.0 else math.inf
            modelled_time = math.log(1000.0) / modelled_decay(mode)
            time_error = decay_time / modelled_time - 1.0
            checks.append(check(
                f"{label} mode {mode.index}",
                abs(peak_error) <= modal_peak_tolerance and abs(time_error) <= modal_decay_tolerance,
                f"peak {measured:.3f} Hz against the baked {mode.frequency:.3f} Hz, off by {100.0 * peak_error:+.4f}% (limit {100.0 * modal_peak_tolerance:g}%); "
                f"T60 {decay_time:.4f} s against the model's {modelled_time:.4f} s, off by {100.0 * time_error:+.2f}% "
                f"(limit {100.0 * modal_decay_tolerance:g}%); {relative:+.1f} dB against the strongest mode, "
                f"fitted over {(modal_decay_blocks - 1) * hop / frame_rate * 1000.0:.0f} ms",
            ))
        ungated = [mode for mode in modes if mode not in measurable]
        checks.append(check(
            f"{label} ring",
            True,
            f"{len(struck)} strikes, measured from sink frame {ring_begin} to {ring_end}; "
            f"{len(ungated)} baked modes not measured because a neighbour is within {modal_separation:g} Hz or they decay faster than {modal_fastest_decay:g} 1/s; "
            + (f"not excited above {modal_excited_db:g} dB and not gated: {', '.join(quiet)}" if quiet else "every measurable mode was excited"),
        ))

    unheard = [mode for mode in measurable if mode.index not in heard]
    checks.append(check(
        f"modal {name} every measurable mode rendered",
        not unheard and all(mode in measurable for mode in bending),
        f"{len(heard)} of {len(measurable)} measurable baked modes were excited and measured in at least one bar, the four bending modes among them"
        if not unheard and all(mode in measurable for mode in bending)
        else f"never excited above {modal_excited_db:g} dB in any bar: {', '.join(f'{mode.frequency:.0f} Hz' for mode in unheard) or 'none'}; "
        f"bending modes that cannot be measured: {', '.join(f'{mode.frequency:.0f} Hz' for mode in bending if mode not in measurable) or 'none'}",
    ))

    rising = len(levels) == modal_flat_bars and all(later_level[2] > earlier[2] for earlier, later_level in zip(levels, levels[1:]))
    checks.append(check(
        f"modal {name} level rises with impact speed",
        rising,
        ", ".join(f"{height:g} m at {speed:.3f} m/s: {level:.2f} dBFS" for height, speed, level in levels) if levels else "no impact measured",
    ))
    return checks, offline["hash"]


def stage6(args: argparse.Namespace) -> int:
    checks: list[check] = []
    runs: dict[str, list[tuple[bool, str | None]]] = {}
    for path in args.logs:
        run_checks, sample_hash = check_modal_run(path)
        checks += run_checks
        voice = modal_voice_pattern.search(read_log(path))
        material = voice["material"] if voice else f"unknown ({path.name})"
        runs.setdefault(material, []).append((all(item.passed for item in run_checks), sample_hash))
    for material, results in runs.items():
        hashes = [sample_hash for _, sample_hash in results if sample_hash]
        checks.append(check(
            f"criterion 1: same hash in every {material} run",
            len(hashes) == len(results) and len(set(hashes)) == 1,
            ", ".join(hashes) if hashes else "no hashes",
        ))
    for material in modal_materials:
        results = runs.get(material, [])
        passing = sum(1 for passed, _ in results if passed)
        checks.append(check(
            f"criterion 6: material {material}",
            len(results) > 0 and passing == len(results),
            f"{passing} of {len(results)} runs passed every check" if results else "no run rang this material",
        ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage6: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


def stage6_budget(args: argparse.Namespace) -> int:
    checks = check_device(args.device, modal_ringing_needed)
    for path in args.device:
        match = modal_ringing_pattern.search(read_log(path))
        if not match:
            checks.append(check(f"criterion 6: ringing {path.name}", False, "no 'audio: modal voices peaked' line"))
            continue
        peak = int(match["peak"])
        at_limit = int(match["at_limit"])
        frames = int(match["frames"])
        share = at_limit / frames if frames else 0.0
        checks.append(check(
            f"criterion 6: ringing {path.name}",
            peak >= modal_ringing_needed and share >= modal_ringing_share,
            f"peak {peak} ringing voices of {match['limit']} (need {modal_ringing_needed}), at the limit on {at_limit} of {frames} frames, "
            f"{100.0 * share:.1f}% (need {100.0 * modal_ringing_share:g}%)",
        ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage6-budget: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


rub_header_pattern = re.compile(
    r"(?P<label>audio_(?P<kind>slide|roll)_probe): speed of sound (?P<speed>[0-9.]+) m/s, listener at " + vector_text + r", fixed step [0-9.]+ ms"
)
rub_sync_pattern = re.compile(r"audio_(?:slide|roll)_probe: sync frame (?P<frame>\d+) pitch [0-9.]+ Hz")
rub_quiet_pattern = re.compile(r"audio_(?:slide|roll)_probe: body \[\d+: (?P<body>[^\]]+)\] quiet frame (?P<frame>\d+) frames (?P<frames>\d+)")
rub_launch_pattern = re.compile(r"audio_(?:slide|roll)_probe: launch (?P<index>\d+) frame (?P<frame>\d+) velocity " + vector_text)
rub_window_pattern = re.compile(
    r"audio_(?:slide|roll)_probe: launch (?P<index>\d+) window frames (?P<begin>\d+) (?P<end>\d+) mean speed (?P<speed>[0-9.]+) m/s"
)
rub_rest_pattern = re.compile(r"audio_(?:slide|roll)_probe: launch (?P<index>\d+) rest frame (?P<frame>\d+)")
rub_trace_pattern = re.compile(
    r"audio: rub step (?P<step>\d+) bodies \[\d+: (?P<body>[^\]]+)\] \[\d+: (?P<other>[^\]]+)\] slip (?P<slip>-?[0-9.]+) m/s "
    r"roll (?P<roll>-?[0-9.]+) m/s load (?P<load>-?[0-9.]+) N kind (?P<kind>[a-z]+) level (?P<level>[0-9.]+) cutoff (?P<cutoff>[0-9.]+) Hz "
    r"outcome (?P<outcome>[a-z ]+), sink frame (?P<sink>\d+)"
)

rub_launches = 3
rub_sustained_db = -70.0
rub_silent_db = -80.0
rub_rest_frames = 4800
rub_tail_frames = 30 * frames_per_step
rub_kind_share = 0.9
rub_kinds = ("slide", "roll")


def block_levels(left: list[float], right: list[float], begin: int, end: int) -> list[float]:
    return [decibels(both_ears(left, right, block, rms)) for block in whole_blocks(begin, end)]


def largest_sample_step(left: list[float], right: list[float], begin: int, end: int) -> float:
    return max(
        (abs(ear[index] - ear[index - 1]) for ear in (left, right) for index in range(max(begin, 1), end)),
        default=0.0,
    )


def check_rub_run(path: Path) -> tuple[list[check], str | None, str | None]:
    checks: list[check] = []
    text = read_log(path)
    name = path.name

    offline = offline_pattern.search(text)
    header = rub_header_pattern.search(text)
    sync = rub_sync_pattern.search(text)
    quiet = rub_quiet_pattern.search(text)
    if not offline or not header or not sync or not quiet:
        checks.append(check(f"rub {name}", False, "missing the offline render, header, sync or quiet window line"))
        return checks, None, None
    kind = header["kind"]
    label = f"{kind} {name}"

    launches = {int(match["index"]): int(match["frame"]) for match in rub_launch_pattern.finditer(text)}
    windows = {int(match["index"]): match for match in rub_window_pattern.finditer(text)}
    rests = {int(match["index"]): int(match["frame"]) for match in rub_rest_pattern.finditer(text)}
    traces = list(rub_trace_pattern.finditer(text))
    impacts = [event for event in read_contact_events(text) if event.outcome in modal_sounding]
    dropped = len(dropped_pattern.findall(text))
    scripted = all(len(found) == rub_launches for found in (launches, windows, rests))
    checks.append(check(
        f"{label} script",
        scripted and dropped == 0 and not impacts,
        f"{len(launches)} launches, {len(windows)} measurement windows, {len(rests)} rests (need {rub_launches} of each), {len(traces)} rub lines traced, "
        f"{len(impacts)} impact voices (need 0), {dropped} sounds dropped, sample hash {offline['hash']}",
    ))
    if not scripted:
        return checks, offline["hash"], kind

    wav_path = Path(offline["path"])
    if not wav_path.exists():
        checks.append(check(f"{label} wav", False, f"{wav_path} does not exist"))
        return checks, offline["hash"], kind
    channels, rate, samples = read_float_wav(wav_path)
    if channels != 2 or rate != frame_rate:
        checks.append(check(f"{label} wav", False, f"{channels} ch at {rate}"))
        return checks, offline["hash"], kind
    left = samples[0::2]
    right = samples[1::2]

    sync_onset = first_sound(left, right, 0)
    if sync_onset is None:
        checks.append(check(f"{label} sync", False, "the WAV is silent"))
        return checks, offline["hash"], kind
    sync_frame = int(sync["frame"])
    sync_block = sync_onset - sync_onset % frames_per_step

    def sample_of(frame: int) -> int:
        return sync_block + (frame - sync_frame) * frames_per_step

    before = block_levels(left, right, sample_of(int(quiet["frame"])), sample_of(launches[0]))
    checks.append(check(
        f"{label} silent before the first launch",
        bool(before) and max(before) < rub_silent_db,
        f"loudest 10 ms block {max(before):.1f} dBFS over {len(before)} blocks (limit {rub_silent_db:g} dBFS)" if before else "no whole block before the first launch",
    ))

    levels: list[tuple[float, float]] = []
    for index in range(rub_launches):
        window = windows[index]
        begin = sample_of(int(window["begin"])) + frames_per_step
        end = sample_of(int(window["end"])) - frames_per_step
        speed = float(window["speed"])
        blocks = block_levels(left, right, begin, end)
        if not blocks:
            checks.append(check(f"{label} launch {index} sustained", False, f"the window of {end - begin} frames holds no whole 10 ms block"))
            continue
        level = decibels(both_ears(left, right, slice(begin, end), rms))
        levels.append((speed, level))
        checks.append(check(
            f"{label} launch {index} sustained",
            min(blocks) > rub_sustained_db,
            f"mean speed {speed:.3f} m/s, level {level:.2f} dBFS over {(end - begin) / frame_rate * 1000.0:.0f} ms, "
            f"quietest 10 ms block {min(blocks):.1f} dBFS (need above {rub_sustained_db:g} dBFS)",
        ))

        inside = [trace for trace in traces if begin <= int(trace["sink"]) < end]
        matching = sum(1 for trace in inside if trace["kind"] == kind)
        share = matching / len(inside) if inside else 0.0
        checks.append(check(
            f"{label} launch {index} kind",
            share >= rub_kind_share,
            f"{matching} of {len(inside)} rub lines in the window are {kind} (need {100.0 * rub_kind_share:g}%)",
        ))

        rest_begin = sample_of(rests[index]) + rub_rest_frames
        rest_end = sample_of(launches[index + 1]) if index + 1 in launches else sample_of(rests[index]) + rub_tail_frames
        after = block_levels(left, right, rest_begin, min(rest_end, len(left)))
        checks.append(check(
            f"{label} launch {index} rest",
            bool(after) and max(after) < rub_silent_db,
            f"loudest 10 ms block from {rub_rest_frames / frame_rate * 1000.0:g} ms after rest {max(after):.1f} dBFS over {len(after)} blocks "
            f"(limit {rub_silent_db:g} dBFS)" if after else "no whole block between rest and the next launch",
        ))

        steady = largest_sample_step(left, right, begin, end)
        release = largest_sample_step(left, right, end, min(rest_begin, len(left)))
        checks.append(check(
            f"{label} launch {index} release",
            release <= steady,
            f"largest sample-to-sample step {release:.6f} from the window's end to {rub_rest_frames / frame_rate * 1000.0:g} ms after rest, "
            f"against {steady:.6f} inside the window",
        ))

    ordered = sorted(levels)
    rising = len(ordered) == rub_launches and all(later[1] > earlier[1] for earlier, later in zip(ordered, ordered[1:]))
    checks.append(check(
        f"{label} level rises with speed",
        rising,
        ", ".join(f"{speed:.3f} m/s: {level:.2f} dBFS" for speed, level in ordered),
    ))
    return checks, offline["hash"], kind


def stage7(args: argparse.Namespace) -> int:
    checks: list[check] = []
    runs: dict[str, list[tuple[bool, str | None]]] = {}
    for path in args.logs:
        run_checks, sample_hash, kind = check_rub_run(path)
        checks += run_checks
        runs.setdefault(kind or f"unknown ({path.name})", []).append((all(item.passed for item in run_checks), sample_hash))
    for kind, results in runs.items():
        hashes = [sample_hash for _, sample_hash in results if sample_hash]
        checks.append(check(
            f"criterion 1: same hash in every {kind} run",
            len(hashes) == len(results) and len(set(hashes)) == 1,
            ", ".join(hashes) if hashes else "no hashes",
        ))
    for kind in rub_kinds:
        results = runs.get(kind, [])
        passing = sum(1 for passed, _ in results if passed)
        checks.append(check(
            f"criterion 7: {kind}",
            len(results) > 0 and passing == len(results),
            f"{passing} of {len(results)} runs passed every check" if results else f"no audio_{kind}_probe run was given",
        ))
    for item in checks:
        print(f"{'PASS' if item.passed else 'FAIL'}  {item.name}: {item.detail}")
    failed = [item for item in checks if not item.passed]
    print(f"stage7: {len(checks) - len(failed)} of {len(checks)} checks passed")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Checks the simulated-audio goal's criteria from scenario logs and WAV files.")
    commands = parser.add_subparsers(dest="command", required=True)

    clip = commands.add_parser("make-clip", help="write a generated clip: the audio_stress sweep, a loop for the Audio Yard, or the contact knock")
    clip.add_argument("--kind", choices=tuple(clip_kinds), default="sweep", help="which clip to generate")
    clip.add_argument("--out", type=Path, help="where to write it; defaults to the clip's place under Sandbox/Assets/Audio")
    clip.set_defaults(run=make_clip)

    first = commands.add_parser("stage1", help="criteria 1 and 2 from audio_stress and audio_stress_device logs")
    first.add_argument("--offline", type=Path, nargs="+", required=True, help="logs of audio_stress runs")
    first.add_argument("--device", type=Path, nargs="+", required=True, help="logs of audio_stress_device runs")
    first.set_defaults(run=stage1)

    second = commands.add_parser("stage2", help="criterion 3 (without the binaural line) from audio_spatial_probe logs")
    second.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_spatial_probe runs")
    second.set_defaults(run=stage2)

    third = commands.add_parser("stage3", help="criterion 3 with its binaural line from audio_binaural_probe logs")
    third.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_binaural_probe runs")
    third.set_defaults(run=stage3)

    fourth = commands.add_parser("stage4", help="criterion 4 from audio_occlusion_probe logs")
    fourth.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_occlusion_probe runs")
    fourth.add_argument("--listening", choices=("speakers", "headphones"), default="speakers", help="the listening mode the runs were pinned to")
    fourth.set_defaults(run=stage4)

    fifth = commands.add_parser("stage5", help="criterion 5 from audio_contact_probe logs")
    fifth.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_contact_probe or audio_contact_probe_gpu runs")
    fifth.add_argument("--solver", choices=("cpu", "gpu"), default="cpu", help="which solver the runs used; gpu records latency and the hash without gating them")
    fifth.set_defaults(run=stage5)

    solo = commands.add_parser("yard", help="each Audio Yard emitter against its source clip, from audio_yard_solo_* logs")
    solo.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_yard_solo_* runs")
    solo.set_defaults(run=yard)

    sixth = commands.add_parser("stage6", help="criterion 6 (baked modes, rendered peaks and decay, level against speed) from audio_modal_probe logs")
    sixth.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_modal_probe runs")
    sixth.set_defaults(run=stage6)

    budget = commands.add_parser("stage6-budget", help="criterion 6's 64 ringing objects against criterion 2's budget, from audio_modal_stress_device logs")
    budget.add_argument("--device", type=Path, nargs="+", required=True, help="logs of audio_modal_stress_device runs")
    budget.set_defaults(run=stage6_budget)

    seventh = commands.add_parser("stage7", help="criterion 7 (sustained sliding and rolling sound, level against speed, silence after rest) from audio_slide_probe and audio_roll_probe logs")
    seventh.add_argument("--logs", type=Path, nargs="+", required=True, help="logs of audio_slide_probe and audio_roll_probe runs")
    seventh.set_defaults(run=stage7)

    args = parser.parse_args()
    return args.run(args)


if __name__ == "__main__":
    sys.exit(main())
