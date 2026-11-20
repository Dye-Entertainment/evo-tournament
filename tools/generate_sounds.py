#!/usr/bin/env python3
import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 22050
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "resources" / "sounds"
rng = random.Random(1337)


def samples(duration):
    return int(RATE * duration)


def envelope(i, n, attack=0.005, release=None, curve=4.0):
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    if release is None:
        r = math.exp(-curve * i / n)
    else:
        r = max(0.0, 1.0 - max(0, i - (n - samples(release))) / max(1, samples(release)))
    return a * r


def sweep(f0, f1, i, n):
    return f0 + (f1 - f0) * (i / n)


class Osc:
    def __init__(self):
        self.phase = 0.0

    def sine(self, freq):
        self.phase += 2 * math.pi * freq / RATE
        return math.sin(self.phase)

    def square(self, freq):
        self.phase += 2 * math.pi * freq / RATE
        return 1.0 if math.sin(self.phase) >= 0 else -1.0

    def saw(self, freq):
        self.phase += freq / RATE
        return 2.0 * (self.phase % 1.0) - 1.0


class LowPass:
    def __init__(self, cutoff):
        self.value = 0.0
        self.set(cutoff)

    def set(self, cutoff):
        rc = 1.0 / (2 * math.pi * cutoff)
        self.alpha = (1.0 / RATE) / (rc + 1.0 / RATE)

    def __call__(self, x):
        self.value += self.alpha * (x - self.value)
        return self.value


def noise():
    return rng.uniform(-1.0, 1.0)


def write(name, data, gain=0.8):
    peak = max(1e-6, max(abs(x) for x in data))
    scale = gain / peak
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / f"{name}.wav"), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        frames = bytearray()
        for x in data:
            frames += struct.pack("<h", int(max(-1.0, min(1.0, x * scale)) * 32767))
        handle.writeframes(bytes(frames))


def rifle():
    n = samples(0.11)
    o, lp = Osc(), LowPass(5000)
    return [envelope(i, n, curve=5) * (0.7 * o.saw(sweep(1400, 380, i, n)) + 0.35 * lp(noise())) for i in range(n)]


def nova_fire():
    n = samples(0.4)
    o, lp = Osc(), LowPass(1200)
    out = []
    for i in range(n):
        lp.set(300 + 2500 * (1 - i / n))
        out.append(envelope(i, n, attack=0.01, curve=3.5) * (0.6 * o.sine(sweep(260, 70, i, n)) + 0.6 * lp(noise())))
    return out


def explosion():
    n = samples(1.1)
    lp, rumble, o = LowPass(900), LowPass(120), Osc()
    out = []
    for i in range(n):
        lp.set(150 + 2200 * math.exp(-5 * i / n))
        out.append(envelope(i, n, attack=0.002, curve=4.5) * (0.9 * lp(noise()) + 0.8 * rumble(noise()) + 0.4 * o.sine(sweep(70, 35, i, n))))
    return out


def rail():
    n = samples(0.55)
    a, b, lp = Osc(), Osc(), LowPass(7000)
    out = []
    for i in range(n):
        crack = math.exp(-60 * i / n) * noise()
        tone = 0.5 * a.sine(sweep(3200, 500, i, n)) + 0.3 * b.sine(sweep(1600, 900, i, n) * (1 + 0.02 * math.sin(i * 0.02)))
        out.append(envelope(i, n, attack=0.001, curve=3.2) * (tone + 0.9 * lp(crack)))
    return out


def jump():
    n = samples(0.1)
    o = Osc()
    return [envelope(i, n, curve=6) * o.sine(sweep(260, 520, i, n)) for i in range(n)]


def land():
    n = samples(0.12)
    lp = LowPass(400)
    return [envelope(i, n, attack=0.001, curve=7) * lp(noise()) for i in range(n)]


def pad():
    n = samples(0.5)
    o = Osc()
    return [envelope(i, n, curve=3) * o.sine(sweep(180, 900, i, n) * (1 + 0.05 * math.sin(2 * math.pi * 18 * i / RATE))) for i in range(n)]


def dodge():
    n = samples(0.22)
    lp = LowPass(2000)
    out = []
    for i in range(n):
        lp.set(400 + 3000 * math.sin(math.pi * i / n))
        out.append(math.sin(math.pi * i / n) * lp(noise()))
    return out


def chime(freqs, step, length, curve=5):
    total = samples(step * (len(freqs) - 1) + length)
    out = [0.0] * total
    for k, f in enumerate(freqs):
        o, o2 = Osc(), Osc()
        start = samples(step * k)
        n = samples(length)
        for i in range(n):
            if start + i < total:
                out[start + i] += envelope(i, n, curve=curve) * (0.7 * o.sine(f) + 0.2 * o2.sine(f * 2.01))
    return out


def pickup_health():
    return chime([523, 659, 784], 0.06, 0.25)


def pickup_armor():
    return chime([392, 494, 587, 784], 0.04, 0.4, curve=4)


def pickup_weapon():
    n = samples(0.08)
    lp = LowPass(3000)
    clack = [envelope(i, n, attack=0.001, curve=8) * lp(noise()) for i in range(n)]
    tones = chime([330, 660], 0.05, 0.3)
    out = [0.0] * max(len(clack), len(tones))
    for i, x in enumerate(clack):
        out[i] += x
    for i, x in enumerate(tones):
        out[i] += 0.8 * x
    return out


def pickup_ammo():
    return chime([440, 554], 0.04, 0.15, curve=6)


def hit():
    n = samples(0.05)
    o = Osc()
    return [envelope(i, n, attack=0.001, curve=6) * o.square(1900) * 0.6 for i in range(n)]


def hurt():
    n = samples(0.18)
    o, lp = Osc(), LowPass(900)
    return [envelope(i, n, curve=4) * lp(math.tanh(3 * o.saw(sweep(170, 110, i, n)))) for i in range(n)]


def death():
    n = samples(0.7)
    o, lp = Osc(), LowPass(1500)
    return [envelope(i, n, attack=0.005, curve=3) * (0.6 * o.saw(sweep(300, 60, i, n)) + 0.5 * lp(noise())) for i in range(n)]


def frag():
    return chime([880, 1318], 0.07, 0.5, curve=4)


def spawn():
    n = samples(0.6)
    o, o2 = Osc(), Osc()
    return [math.sin(math.pi * i / n) * (0.5 * o.sine(sweep(300, 1200, i, n)) + 0.3 * o2.sine(sweep(450, 1800, i, n))) for i in range(n)]


def match_end():
    return chime([392, 523, 659, 784, 1046], 0.16, 0.9, curve=3)


def empty():
    n = samples(0.05)
    lp = LowPass(2500)
    return [envelope(i, n, attack=0.001, curve=10) * lp(noise()) for i in range(n)]


def switch():
    n = samples(0.07)
    o, lp = Osc(), LowPass(2000)
    return [envelope(i, n, attack=0.001, curve=7) * (0.5 * lp(noise()) + 0.4 * o.square(240)) for i in range(n)]


def projectile_hum():
    n = samples(0.3)
    o = Osc()
    return [math.sin(math.pi * i / n) * o.sine(110 + 20 * math.sin(i * 0.004)) for i in range(n)]


SOUNDS = {
    "rifle": (rifle, 0.55),
    "nova_fire": (nova_fire, 0.8),
    "explosion": (explosion, 0.95),
    "rail": (rail, 0.85),
    "jump": (jump, 0.35),
    "land": (land, 0.5),
    "pad": (pad, 0.6),
    "dodge": (dodge, 0.5),
    "pickup_health": (pickup_health, 0.6),
    "pickup_armor": (pickup_armor, 0.6),
    "pickup_weapon": (pickup_weapon, 0.7),
    "pickup_ammo": (pickup_ammo, 0.5),
    "hit": (hit, 0.45),
    "hurt": (hurt, 0.6),
    "death": (death, 0.75),
    "frag": (frag, 0.6),
    "spawn": (spawn, 0.5),
    "match_end": (match_end, 0.7),
    "empty": (empty, 0.4),
    "switch": (switch, 0.45),
}

if __name__ == "__main__":
    for name, (generator, gain) in SOUNDS.items():
        write(name, generator(), gain)
    print(f"wrote {len(SOUNDS)} sounds to {OUT}")
