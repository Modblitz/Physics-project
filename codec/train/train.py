from __future__ import annotations

import math
import os
import random
import struct
import sys
import wave
from pathlib import Path

K = 16
H = 64
EPOCHS = 35
BATCH = 64
LR = 0.02
MOMENTUM = 0.9
TRAIN_SAMPLES = 80_000
SEED = 1

REPO_ROOT = Path(__file__).resolve().parents[2]
AUDIO_DIR = REPO_ROOT / "test_audio"
WEIGHTS_HPP = REPO_ROOT / "codec" / "src" / "weights.hpp"


def read_wav_mono_int16(path: Path) -> list[int]:
    with wave.open(str(path), "rb") as w:
        sw = w.getsampwidth()
        nch = w.getnchannels()
        n = w.getnframes()
        if sw != 2:
            raise RuntimeError(f"{path}: only 16-bit supported (got {sw*8})")
        raw = w.readframes(n)
    fmt = "<" + "h" * (n * nch)
    flat = struct.unpack(fmt, raw)
    if nch == 1:
        return list(flat)
    return list(flat[0::nch])


def build_training_pairs(samples: list[int]) -> list[tuple[list[float], float]]:
    pairs = []
    inv = 1.0 / 32768.0
    for n in range(K, len(samples)):
        lin = 2 * samples[n - 1] - samples[n - 2]
        target = (samples[n] - lin) * inv
        window = [samples[n - K + i] * inv for i in range(K)]
        pairs.append((window, target))
    return pairs


def init_params(rng: random.Random) -> dict:
    w1_scale = math.sqrt(1.0 / K)
    w2_scale = math.sqrt(1.0 / H)
    W1 = [[rng.uniform(-w1_scale, w1_scale) for _ in range(K)] for _ in range(H)]
    b1 = [0.0 for _ in range(H)]
    W2 = [rng.uniform(-w2_scale, w2_scale) for _ in range(H)]
    b2 = 0.0
    return {"W1": W1, "b1": b1, "W2": W2, "b2": b2}


def forward(p: dict, x: list[float]) -> tuple[float, list[float], list[float]]:
    W1, b1, W2, b2 = p["W1"], p["b1"], p["W2"], p["b2"]
    z = [0.0] * H
    for j in range(H):
        s = b1[j]
        wj = W1[j]
        for i in range(K):
            s += wj[i] * x[i]
        z[j] = s
    h = [math.tanh(v) for v in z]
    y = b2
    for j in range(H):
        y += W2[j] * h[j]
    return y, h, z


def train(pairs: list[tuple[list[float], float]]) -> dict:
    rng = random.Random(SEED)
    p = init_params(rng)

    vW1 = [[0.0] * K for _ in range(H)]
    vb1 = [0.0] * H
    vW2 = [0.0] * H
    vb2 = 0.0

    baseline_mse = sum(t * t for _, t in pairs) / len(pairs)
    print(f"[train] baseline (linear-only) MSE = {baseline_mse:.6e}")

    n_pairs = len(pairs)
    indices = list(range(n_pairs))
    lr = LR
    for epoch in range(EPOCHS):
        rng.shuffle(indices)
        total = 0.0
        if epoch == EPOCHS // 2:
            lr *= 0.3
        if epoch == int(EPOCHS * 0.8):
            lr *= 0.3
        for start in range(0, n_pairs, BATCH):
            batch_idx = indices[start:start + BATCH]
            gW1 = [[0.0] * K for _ in range(H)]
            gb1 = [0.0] * H
            gW2 = [0.0] * H
            gb2 = 0.0
            for bi in batch_idx:
                x, t = pairs[bi]
                y, h, z = forward(p, x)
                err = y - t
                total += err * err
                dy = err
                gb2 += dy
                W2 = p["W2"]
                for j in range(H):
                    gW2[j] += dy * h[j]
                for j in range(H):
                    dh = dy * W2[j]
                    dz = dh * (1.0 - h[j] * h[j])
                    gb1[j] += dz
                    row = gW1[j]
                    for i in range(K):
                        row[i] += dz * x[i]
            bs = len(batch_idx)
            inv_bs = 1.0 / bs
            for j in range(H):
                vb1[j] = MOMENTUM * vb1[j] - lr * gb1[j] * inv_bs
                p["b1"][j] += vb1[j]
                vW2[j] = MOMENTUM * vW2[j] - lr * gW2[j] * inv_bs
                p["W2"][j] += vW2[j]
                row_v = vW1[j]
                row_p = p["W1"][j]
                row_g = gW1[j]
                for i in range(K):
                    row_v[i] = MOMENTUM * row_v[i] - lr * row_g[i] * inv_bs
                    row_p[i] += row_v[i]
            vb2 = MOMENTUM * vb2 - lr * gb2 * inv_bs
            p["b2"] += vb2
        mse = total / n_pairs
        improv = 100.0 * (1.0 - mse / baseline_mse)
        print(f"[train] epoch {epoch+1:2d}/{EPOCHS}  mse={mse:.6e}  "
              f"({improv:+.2f}% vs linear)  lr={lr:.4f}")
    return p


def emit_weights_hpp(p: dict, out: Path) -> None:
    def fmt(v: float) -> str:
        return f"{v:.8e}f"

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("namespace pres_weights {")
    lines.append("")
    lines.append(f"constexpr int K = {K};")
    lines.append(f"constexpr int H = {H};")
    lines.append("")
    lines.append(f"constexpr float W1[H][K] = {{")
    for j in range(H):
        row = ", ".join(fmt(v) for v in p["W1"][j])
        lines.append(f"    {{ {row} }},")
    lines.append("};")
    lines.append("")
    b1_row = ", ".join(fmt(v) for v in p["b1"])
    lines.append(f"constexpr float b1[H] = {{ {b1_row} }};")
    lines.append("")
    w2_row = ", ".join(fmt(v) for v in p["W2"])
    lines.append(f"constexpr float W2[H] = {{ {w2_row} }};")
    lines.append("")
    lines.append(f"constexpr float b2 = {fmt(p['b2'])};")
    lines.append("")
    lines.append("}")
    lines.append("")
    out.write_text("\n".join(lines))
    print(f"[train] wrote {out}  ({out.stat().st_size} bytes)")


def main() -> int:
    if not AUDIO_DIR.is_dir():
        print(f"audio dir not found: {AUDIO_DIR}", file=sys.stderr)
        return 1
    wavs = sorted(AUDIO_DIR.glob("*.wav"))
    if not wavs:
        print(f"no .wav files in {AUDIO_DIR}", file=sys.stderr)
        return 1

    all_pairs: list[tuple[list[float], float]] = []
    for w in wavs:
        s = read_wav_mono_int16(w)
        pairs = build_training_pairs(s)
        print(f"[load] {w.name}: {len(s)} samples -> {len(pairs)} pairs")
        all_pairs.extend(pairs)

    print(f"[load] total pairs: {len(all_pairs)}")
    rng = random.Random(SEED)
    if len(all_pairs) > TRAIN_SAMPLES:
        all_pairs = rng.sample(all_pairs, TRAIN_SAMPLES)
        print(f"[load] subsampled to {len(all_pairs)}")

    params = train(all_pairs)
    emit_weights_hpp(params, WEIGHTS_HPP)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
