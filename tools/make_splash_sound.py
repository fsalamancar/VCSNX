"""Synthesizes the boot splash's sound (platform/switch/boot_splash), a neon-80s sting timed to the logo.

Everything is generated here from oscillators and noise, so the sound is original and free to publish:
  0.00-0.65 s  whooshes from both sides while b and o slide in, a soft thump when they meet
  0.70-1.06 s  the neon tube: a click and a 120 Hz buzz on every flash, then a quiet hum
  from 1.15 s  one synth pluck (A minor 9 arpeggio, with an echo) per letter of "studios" that drops in
  1.70-2.80 s  a pad chord that swells with the green glow
  2.80-3.05 s  the VHS glitch: stuttered grains of the pad and tape hiss
  3.05-3.85 s  the chord rings out while the logo blurs, silent when the splash ends
The times are boot_splash.cpp's; change them together.

Needs numpy. Usage: python3 -I tools/make_splash_sound.py assets/splash/sound.wav
"""
import math
import sys
import wave
from pathlib import Path

import numpy as np

RATE = 48000
END = 3.85                                  # kSplashSeconds
NEON_FLASHES = [(0.70, 0.75, 1.0), (0.80, 0.83, 0.55), (0.90, 0.97, 1.0), (1.00, 1.02, 0.3), (1.06, 2.80, 1.0)]
DROP_START, DROP_STAGGER, DROP_LANDS = 1.15, 0.08, 0.22
GLITCH, FADE_OUT = 2.80, 3.05               # kSplashGlitchStart, kSplashFadeOutStart
FINAL_FADE = 0.30                           # the last 0.3 s fade to silence
PEAK = 0.5                                  # -6 dBFS

rng = np.random.default_rng(1983)
frames = int(END * RATE)
left = np.zeros(frames)
right = np.zeros(frames)


def put(signal, start, pan=0.0, gain=1.0):
    """Mixes signal in at start seconds; pan -1 is left, +1 right (constant power)."""
    i = int(start * RATE)
    signal = signal[: max(0, frames - i)]
    left[i:i + len(signal)] += signal * gain * math.sqrt((1 - pan) / 2)
    right[i:i + len(signal)] += signal * gain * math.sqrt((1 + pan) / 2)


def seconds(n):
    return np.arange(int(n * RATE)) / RATE


def lowpass(x, cutoff):
    """One-pole low-pass, as a convolution with its impulse response cut where it drops below 1e-6."""
    a = math.exp(-2 * math.pi * cutoff / RATE)
    taps = (1 - a) * a ** np.arange(int(math.log(1e-6) / math.log(a)) + 1)
    return np.convolve(x, taps)[: len(x)]


def envelope(n, attack, decay):
    e = np.exp(-seconds(n) / decay)
    na = max(1, int(attack * RATE))
    e[:na] *= np.linspace(0, 1, na)
    return e


def midi(note):
    return 440.0 * 2 ** ((note - 69) / 12)


def saw(freq, n, detune=0.0):
    t = seconds(n)
    return sum(2 * ((t * freq * (1 + d)) % 1) - 1 for d in (-detune, 0.0, detune)) / 3


def whooshes():
    n = 0.65
    shape = np.sin(np.linspace(0, math.pi, int(n * RATE))) ** 2
    put(lowpass(rng.standard_normal(int(n * RATE)), 1800) * shape, 0.00, pan=-0.8, gain=0.35)
    put(lowpass(rng.standard_normal(int(n * RATE)), 1800) * shape, 0.02, pan=0.8, gain=0.35)
    put(np.sin(2 * math.pi * 55 * seconds(0.5)) * envelope(0.5, 0.005, 0.12), 0.58, gain=0.6)


def neon_tube():
    for start, end, level in NEON_FLASHES:
        click = rng.standard_normal(int(0.012 * RATE)) * np.linspace(1, 0, int(0.012 * RATE))
        put(click, start, gain=0.35 * level)
        n = end - start
        buzz = lowpass(saw(120, n) * 0.6 + np.sin(2 * math.pi * 240 * seconds(n)) * 0.3, 2500)
        shape = np.ones(len(buzz))
        shape[-int(0.01 * RATE):] = np.linspace(1, 0, int(0.01 * RATE))
        steady = n >= 1.0
        if steady:
            shape *= np.linspace(1, 0.5, len(buzz))
        put(buzz * shape, start, gain=0.05 if steady else 0.16 * level)


def arpeggio():
    for i, note in enumerate((69, 72, 76, 79, 83, 84, 88)):
        pluck = lowpass(saw(midi(note), 0.9, 0.004), 3500) * envelope(0.9, 0.003, 0.18)
        start = DROP_START + DROP_STAGGER * i + DROP_LANDS
        put(pluck, start, pan=-0.6 + 0.2 * i, gain=0.22)
        put(pluck, start + 0.24, pan=0.6 - 0.2 * i, gain=0.09)  # echo


def pad_and_glitch():
    n = GLITCH - 1.7
    pad = sum(lowpass(saw(midi(m), n, 0.006), 1400) for m in (45, 57, 60, 64, 71)) / 5
    put(pad * np.minimum(1, seconds(n) / 0.5), 1.7, gain=0.45)
    grain = pad[-int(0.05 * RATE):]
    for k, start in enumerate(np.arange(GLITCH, FADE_OUT, 0.05)):
        stutter = (grain * np.linspace(1, 0.2, len(grain)))[:: 1 + k % 2]
        put(stutter, start, gain=0.5 * (1 - (start - GLITCH) / 0.3))
    put(lowpass(rng.standard_normal(int((FADE_OUT - GLITCH) * RATE)), 6000) * 0.25, GLITCH, gain=0.5)


def ring_out():
    n = END - FADE_OUT
    chord = sum(lowpass(saw(midi(m), n, 0.006), 900) for m in (45, 57, 64, 71, 76)) / 5
    put(chord * envelope(n, 0.02, 0.45), FADE_OUT, gain=0.5)


def reverb(x):
    """Convolves with 0.9 s of decaying noise: a small room around the dry sound."""
    ir = rng.standard_normal(int(0.9 * RATE)) * np.exp(-seconds(0.9) / 0.25) * 0.02
    ir[0] = 1.0
    size = 1 << int(math.ceil(math.log2(len(x) + len(ir))))
    return np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size))[: len(x)]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make_splash_sound.py <output .wav>", file=sys.stderr)
        return 2
    whooshes()
    neon_tube()
    arpeggio()
    pad_and_glitch()
    ring_out()
    mix = np.stack([reverb(left), reverb(right)], axis=1)
    fade = np.ones(frames)
    fade[-int(FINAL_FADE * RATE):] = np.linspace(1, 0, int(FINAL_FADE * RATE))
    mix *= fade[:, None]
    mix *= PEAK / np.abs(mix).max()
    pcm = (mix * 32767).astype("<i2")
    out = Path(sys.argv[1])
    out.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(out), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    print(f"{out} {frames / RATE:.2f} s, {RATE} Hz stereo, {out.stat().st_size // 1024} KiB")
    return 0


if __name__ == "__main__":
    sys.exit(main())
