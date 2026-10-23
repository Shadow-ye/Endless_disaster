"""生成三种炸雷（assets/sfx/thunder1..3.wav）。

一次落雷随机取一条播放，三条都是近处炸开的响雷。做法：
- 主炸点：1ms 起音、几十毫秒衰减的宽带冲击（"砰"的那一下）
- 撕裂层：中高频在炸开后接着炸开，100~200ms 收掉
- 炮体：200~1200Hz 的中频冲击，给"肉"
- 次低频：130Hz 一路下潜到 38Hz，给胸腔感
- 每记雷是独立的几声，中间留空隙，不做成一条连续噪声
- 最后过压缩（把峰值压平、整体抬响）+ 短混响，峰值归一化到接近满刻度
三条的区别在炸点数量、节奏和尾音长度，听感上不会以为是同一条。
用法：python tools/make_thunder_sfx.py
"""

import os
import wave

import numpy as np

SR = 22050
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sfx")


def bandpass(white, low, high, order=2):
    """把白噪声整形成 [low, high] 的带限噪声（平滑频域掩码，不抖铃）。"""
    n = white.size
    spec = np.fft.rfft(white)
    f = np.maximum(np.fft.rfftfreq(n, 1.0 / SR), 1e-6)
    hp = 1.0 / (1.0 + (low / f) ** (2 * order))
    lp = 1.0 / (1.0 + (f / high) ** (2 * order))
    out = np.fft.irfft(spec * hp * lp, n)
    peak = np.max(np.abs(out))
    return out / peak if peak > 0 else out


def env(n, start, attack, decay, hold=0.0):
    """起音 + 指数衰减的包络；start 之前为 0。"""
    t = np.arange(n) / SR - start
    out = np.zeros(n)
    live = t >= 0.0
    tt = t[live]
    rise = 1.0 - np.exp(-tt / max(attack, 1e-5))
    fall = np.exp(-np.maximum(tt - hold, 0.0) / max(decay, 1e-5))
    out[live] = rise * fall
    return out


def gliding_tone(n, start, f0, f1, decay, attack=0.004, hold=0.35):
    """下滑的正弦轰鸣：从 f0 压到 f1，给一点胸腔感。"""
    t = np.arange(n) / SR
    tt = np.clip(t - start, 0.0, hold)
    freq = f0 * (f1 / f0) ** (tt / hold)
    phase = 2.0 * np.pi * np.cumsum(freq) / SR
    return np.sin(phase) * env(n, start, attack, decay, hold=hold)


def take(noise, rng, n):
    """从一段长噪声里随机截一段，避免每一记雷的波形都一样。"""
    offset = int(rng.integers(0, max(1, noise.size - 1)))
    return np.roll(noise, -offset)[:n]


def add_crack(out, rng, layers, t0, amp, spread=0.05, count=6):
    """炸裂：十几粒极短的宽带瞬态挤在开头一百毫秒里。"""
    n = out.size
    snap = layers[0]
    for i in range(count):
        start = t0 + float(rng.uniform(0.0, spread))
        level = amp * (1.0 - i / (count + 1.0)) * float(rng.uniform(0.45, 1.0))
        decay = 0.008 + 0.012 * float(rng.uniform())
        out += level * take(snap, rng, n) * env(n, start, 0.0004, decay)


def add_blast(out, rng, layers, t0, amp=1.0, f0=150.0, f1=34.0, punch=0.85):
    """一记爆炸：击穿噪声 + 中高频撕裂 + 中频炮体 + 下潜次低频 + 起音音头。"""
    n = out.size
    snap, bright, mid, dark = layers
    out += amp * 1.80 * take(snap, rng, n) * env(n, t0, 0.0006, 0.040)
    out += amp * 1.10 * take(bright, rng, n) * env(n, t0, 0.0015, 0.150)
    out += amp * 0.95 * take(mid, rng, n) * env(n, t0, 0.0030, 0.260)
    out += amp * 0.55 * take(dark, rng, n) * env(n, t0, 0.0080, 0.520)
    out += amp * 0.95 * gliding_tone(n, t0, f0, f1, 0.420, attack=0.002, hold=0.240)
    out += amp * punch * gliding_tone(n, t0, 96.0, 44.0, 0.100, attack=0.001, hold=0.080)


def add_aftershock(out, rng, layers, t0, amp, bright_decay=0.12, dark_decay=0.70,
                   f0=70.0, f1=30.0):
    """余雷：炸点过去之后补的几记闷响。"""
    n = out.size
    snap, bright, mid, dark = layers
    out += amp * 0.75 * take(bright, rng, n) * env(n, t0, 0.003, bright_decay)
    out += amp * 0.65 * take(mid, rng, n) * env(n, t0, 0.005, bright_decay * 1.8)
    out += amp * 0.95 * take(dark, rng, n) * env(n, t0, 0.008, dark_decay)
    out += amp * 0.45 * gliding_tone(n, t0, f0, f1, dark_decay * 0.8, hold=0.30)


def convolve(x, ir):
    size = 1 << (x.size + ir.size - 1).bit_length()
    y = np.fft.irfft(np.fft.rfft(x, size) * np.fft.rfft(ir, size), size)
    return y[: x.size]


def make_ir(rng, length=0.8):
    """混响冲激：衰减的白噪声，低频留得久一点。"""
    n = int(length * SR)
    t = np.arange(n) / SR
    ir = rng.standard_normal(n) * np.exp(-t / 0.22)
    ir = bandpass(ir, 28.0, 2600.0, order=1)
    ir *= np.exp(-t / 0.28)
    ir[:12] *= np.linspace(0.15, 1.0, 12)
    return ir / (np.max(np.abs(ir)) + 1e-9)


def limit(x, ceiling, attack_ms=0.6, release_ms=70.0):
    """峰值限制器：只削尖峰、不动身体，削完再整体归一化就等于把响度顶上去。"""
    attack = float(np.exp(-1.0 / (SR * attack_ms / 1000.0)))
    release = float(np.exp(-1.0 / (SR * release_ms / 1000.0)))
    absx = np.abs(x)
    env_ = np.empty_like(absx)
    level = 0.0
    for i in range(absx.size):
        v = float(absx[i])
        coef = attack if v > level else release
        level = coef * level + (1.0 - coef) * v
        env_[i] = level
    gain = np.ones_like(absx)
    loud = env_ > ceiling
    gain[loud] = ceiling / env_[loud]
    return x * gain


def finish(dry, rng, peak_target=0.97, wet=0.16, crest_db=9.0):
    dry = dry / (np.max(np.abs(dry)) + 1e-9)
    tail = convolve(dry, make_ir(rng))
    tail *= wet * np.max(np.abs(dry)) / (np.max(np.abs(tail)) + 1e-9)
    mix = dry + tail
    mix -= np.mean(mix)
    # 峰均比压到 crest_db 左右：雷声的"炸"主要靠这一段响度，不是靠那一个尖峰。
    # 炸点是极窄的噪声尖峰，包络限幅器追不上，这里直接用软削波（tanh）当砖墙限幅，
    # 削完再归一化，身体部分自然就顶起来了
    rms = float(np.sqrt(np.mean(mix**2)))
    ceiling = 10.0 ** (crest_db / 20.0) * rms
    mix = np.tanh(mix / ceiling * 1.25) * ceiling
    mix = mix / (np.max(np.abs(mix)) + 1e-9) * peak_target
    fade_in = int(0.001 * SR)
    fade_out = int(0.18 * SR)
    mix[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    mix[-fade_out:] *= np.linspace(1.0, 0.0, fade_out) ** 1.4
    return mix


def build(seed, duration, layers, shape):
    rng = np.random.default_rng(seed)
    n = int(duration * SR)
    out = np.zeros(n)
    shape(rng, out, layers)
    return finish(out, rng)


def save(path, samples):
    data = np.clip(samples * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SR)
        out.writeframes(data.tobytes())
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples**2)))
    print(f"{os.path.basename(path)}: {len(samples) / SR:.2f}s  peak {peak:.2f}  "
          f"rms {rms:.3f}  crest {20 * np.log10(peak / rms):.1f}dB")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    def layers_for(seed, low_cut, bright_low, bright_high):
        rng = np.random.default_rng(seed + 1000)
        noise = rng.standard_normal(int(6.0 * SR))
        return (
            bandpass(noise, 400.0, 10000.0, order=1),           # 击穿
            bandpass(noise, bright_low, bright_high, order=2),  # 撕裂
            bandpass(noise, 180.0, 1200.0, order=2),            # 炮体
            bandpass(noise, low_cut, 240.0, order=2),           # 闷面
        )

    # 1 号：一记轰爆。最干脆，炸点之后两记余雷就收干净
    def shape_blast(rng, out, layers):
        add_crack(out, rng, layers, 0.004, 1.40, spread=0.12, count=14)
        add_blast(out, rng, layers, 0.0, amp=1.0, f0=140.0, f1=36.0, punch=0.70)
        add_aftershock(out, rng, layers, 0.34, 0.55, bright_decay=0.10, dark_decay=0.55)
        add_aftershock(out, rng, layers, 0.92, 0.30, bright_decay=0.14, dark_decay=0.90)

    # 2 号：双响轰爆。半秒内连炸两次，第二记稍轻
    def shape_double_blast(rng, out, layers):
        add_crack(out, rng, layers, 0.004, 1.30, spread=0.10, count=13)
        add_crack(out, rng, layers, 0.56, 0.95, spread=0.08, count=10)
        add_blast(out, rng, layers, 0.0, amp=1.0, f0=145.0, f1=36.0, punch=0.70)
        add_blast(out, rng, layers, 0.56, amp=0.82, f0=120.0, f1=33.0, punch=0.55)
        add_aftershock(out, rng, layers, 1.30, 0.35, bright_decay=0.16, dark_decay=0.95)

    # 3 号：重炮轰爆。炸点数最多、次低频最重、尾巴最长
    def shape_heavy_blast(rng, out, layers):
        add_crack(out, rng, layers, 0.004, 1.55, spread=0.16, count=18)
        add_blast(out, rng, layers, 0.0, amp=1.0, f0=165.0, f1=32.0, punch=0.85)
        add_blast(out, rng, layers, 0.30, amp=0.60, f0=120.0, f1=32.0, punch=0.45)
        add_aftershock(out, rng, layers, 0.85, 0.45, bright_decay=0.12, dark_decay=0.80)
        add_aftershock(out, rng, layers, 1.60, 0.28, bright_decay=0.18, dark_decay=1.10)

    specs = [
        (11, 2.6, (18.0, 150.0, 5200.0), shape_blast),
        (29, 3.0, (16.0, 140.0, 4600.0), shape_double_blast),
        (47, 3.4, (15.0, 120.0, 4000.0), shape_heavy_blast),
    ]
    for index, (seed, duration, bands, shape) in enumerate(specs, start=1):
        layers = layers_for(seed, bands[0], bands[1], bands[2])
        save(os.path.join(OUT_DIR, f"thunder{index}.wav"), build(seed, duration, layers, shape))


if __name__ == "__main__":
    main()
