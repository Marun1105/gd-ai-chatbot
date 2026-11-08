# Synthesizes the mod's sounds as 16-bit mono WAVs (no dependencies):
# the AI's talking blips, and Grin's roar, screen crack and glass shatter.
import math, random, struct, sys, wave

RATE = 44100

def write(path, samples, level=0.5):
    peak = max(abs(s) for s in samples) or 1.0
    scale = level / peak  # 0.5 is about -6 dB, leaves headroom when blips overlap
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s * scale * 32767)) for s in samples))

def envelope(i, n, attack):
    a = int(n * attack)
    if i < a:
        return i / a
    return (1 - (i - a) / (n - a)) ** 2  # quick, smooth decay

def lowpass(samples, alpha):
    out, prev = [], 0.0
    for s in samples:
        prev += alpha * (s - prev)
        out.append(prev)
    return out

def blip():
    # Soft square wave: the classic RPG dialogue blip
    n = int(RATE * 0.045)
    raw = [(1 if math.sin(2 * math.pi * 520 * i / RATE) >= 0 else -1) * envelope(i, n, 0.05) for i in range(n)]
    return lowpass(raw, 0.25)

def chirp():
    # Quick upward sweep, brighter and friendlier
    n = int(RATE * 0.05)
    phase, out = 0.0, []
    for i in range(n):
        f = 600 + 450 * (i / n)
        phase += 2 * math.pi * f / RATE
        out.append(math.sin(phase) * envelope(i, n, 0.08))
    return out

def soft():
    # Gentle sine with a quiet octave, for a calm character
    n = int(RATE * 0.06)
    return [(math.sin(2 * math.pi * 330 * i / RATE) + 0.35 * math.sin(2 * math.pi * 660 * i / RATE)) * envelope(i, n, 0.2)
            for i in range(n)]

def highpass(samples, alpha):
    low = lowpass(samples, alpha)
    return [s - l for s, l in zip(samples, low)]

def roar():
    # A growling monster roar: detuned saws gliding down, roughened by a fast
    # flutter, with breathy noise and a sub-bass hit at the start
    rnd = random.Random(7)
    n = int(RATE * 1.7)
    phases = [0.0, 0.0, 0.0]
    detune = [1.0, 1.013, 0.987]
    drift = 0.0
    out = []
    for i in range(n):
        t = i / RATE
        drift += rnd.uniform(-1, 1) * 0.6
        drift *= 0.995
        f0 = 115 - 50 * min(1.0, t / 1.4) + drift
        growl = 0.55 + 0.45 * math.sin(2 * math.pi * 33 * t + 2.2 * math.sin(2 * math.pi * 7 * t))
        saw = 0.0
        for k in range(3):
            phases[k] = (phases[k] + f0 * detune[k] / RATE) % 1.0
            saw += 2 * phases[k] - 1
        breath = rnd.uniform(-1, 1)
        if t < 0.12:
            env = t / 0.12
        elif t < 1.15:
            env = 1.0
        else:
            env = max(0.0, 1 - (t - 1.15) / 0.55) ** 1.6
        hit = math.sin(2 * math.pi * 48 * t) * math.exp(-t * 9) * 1.4
        out.append((saw * growl * 0.6 + breath * 0.5) * env + hit)
    # Throaty: keep the body, soften the fizz, then drive it
    body = lowpass(out, 0.12)
    return [math.tanh(3.2 * s) for s in body]

def crack():
    # Glass cracking under a hit: a sharp click, bright fizz and a few pings
    rnd = random.Random(11)
    n = int(RATE * 0.4)
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    bright = highpass(noise, 0.35)
    pings = [(rnd.uniform(3000, 7500), rnd.uniform(0.0, 0.08)) for _ in range(6)]
    out = []
    for i in range(n):
        t = i / RATE
        s = bright[i] * math.exp(-t * 28) * 0.9
        s += (1.0 if i < 40 else 0.0) * (1 - i / 40)  # the click
        for f, start in pings:
            if t >= start:
                s += math.sin(2 * math.pi * f * (t - start)) * math.exp(-(t - start) * 45) * 0.25
        out.append(s)
    return out

def shatter():
    # The screen falling apart: a burst, then many glass tinkles thinning out
    rnd = random.Random(23)
    n = int(RATE * 1.3)
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    crunch = lowpass(noise, 0.18)
    bright = highpass(noise, 0.3)
    out = [crunch[i] * math.exp(-(i / RATE) * 7) * 0.8 + bright[i] * math.exp(-(i / RATE) * 11) * 0.7 for i in range(n)]
    for _ in range(90):
        start = min(1.15, rnd.expovariate(4.0))
        f = rnd.uniform(2200, 8500)
        decay = rnd.uniform(25, 70)
        amp = rnd.uniform(0.15, 0.45) * math.exp(-start * 1.8)
        begin = int(start * RATE)
        for j in range(int(RATE * 0.09)):
            if begin + j >= n:
                break
            tt = j / RATE
            out[begin + j] += math.sin(2 * math.pi * f * tt) * math.exp(-tt * decay) * amp
    return out

def shot():
    # A punchy blaster: a falling zap over a short noise crack
    rnd = random.Random(31)
    n = int(RATE * 0.22)
    phase, out = 0.0, []
    for i in range(n):
        t = i / RATE
        f = 1400 * math.exp(-t * 18) + 140
        phase += 2 * math.pi * f / RATE
        zap = (1 if math.sin(phase) >= 0 else -1) * math.exp(-t * 16) * 0.6
        crack_ = rnd.uniform(-1, 1) * math.exp(-t * 60)
        out.append(zap + crack_)
    return lowpass(out, 0.55)

def hit():
    # The shot landing: a thick thud with a wet crunch on top
    rnd = random.Random(41)
    n = int(RATE * 0.28)
    noise = [rnd.uniform(-1, 1) for _ in range(n)]
    crunch = lowpass(noise, 0.25)
    out = []
    for i in range(n):
        t = i / RATE
        thud = math.sin(2 * math.pi * (90 + 60 * math.exp(-t * 30)) * t) * math.exp(-t * 14)
        out.append(thud * 1.2 + crunch[i] * math.exp(-t * 22) * 0.9)
    return [math.tanh(1.8 * s) for s in out]

out_dir = sys.argv[1]
for name, gen, level in (
    ("blip", blip, 0.5), ("chirp", chirp, 0.5), ("soft", soft, 0.5),
    ("roar", roar, 0.9), ("crack", crack, 0.8), ("shatter", shatter, 0.8),
    ("shot", shot, 0.7), ("hit", hit, 0.8),
):
    write(f"{out_dir}/{name}.wav", gen(), level)
    print(name, "ok")
