# Synthesizes the AI's talking blips as 16-bit mono WAVs (no dependencies).
import math, struct, sys, wave

RATE = 44100

def write(path, samples):
    peak = max(abs(s) for s in samples) or 1.0
    scale = 0.5 / peak  # about -6 dB, leaves headroom when blips overlap
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

out_dir = sys.argv[1]
for name, gen in (("blip", blip), ("chirp", chirp), ("soft", soft)):
    write(f"{out_dir}/{name}.wav", gen())
    print(name, "ok")
