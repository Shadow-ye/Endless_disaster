"""生成机甲人「蜂群」锁定目标时的提示音（assets/sfx/lock.wav）。

就是三声干脆的「嘀 嘀 嘀」，像火控系统的锁定提示：
- 三声等间隔的短促蜂鸣，前两声同音高，第三声拔高一点收尾当成"咬住"；
- 每一声前面垫一记极短的带限噪声，起音才有"嘀"的颗粒感，不会糊成一声长鸣；
- 整体 0.42 秒、纯干声，挂在技能音后面不打架。

用法：python tools/make_lock_sfx.py
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


def env(tt, attack, decay, length):
    """起音 + 指数衰减；tt 是相对音符起点的时间，超过 length 后收干净。"""
    rise = 1.0 - np.exp(-tt / max(attack, 1e-5))
    fall = np.exp(-decay * np.clip(tt / max(length, 1e-5), 0.0, 1.0))
    alive = (tt >= 0.0) & (tt < length)
    return np.where(alive, rise * fall, 0.0)


def beep(out, start, freq, length, amp, attack=0.0015, decay=3.0, harmonic=0.30):
    """一声短蜂鸣：正弦 + 一点三次谐波，听起来是电子音而不是纯音。"""
    t = np.arange(out.size) / SR - start
    tt = np.clip(t, 0.0, length)
    phase = 2.0 * np.pi * freq * tt
    sig = np.sin(phase) + harmonic * np.sin(3.0 * phase)
    out += amp * sig * env(t, attack, decay, length)


def tick(out, rng, start, length, amp, low, high, decay=140.0):
    """起音颗粒：一小段带限噪声，压得极快，给每一声"嘀"一个锐利的头。"""
    t = np.arange(out.size) / SR - start
    noise = bandpass(rng.standard_normal(out.size), low, high, order=1)
    out += amp * noise * env(t, 0.0004, decay, length)


def save(path, samples):
    data = np.clip(samples * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(SR)
        out.writeframes(data.tobytes())
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples**2)))
    print(f"{os.path.basename(path)}: {len(samples) / SR:.2f}s  peak {peak:.2f}  rms {rms:.3f}")


def main():
    rng = np.random.default_rng(17)
    duration = 0.42
    n = int(duration * SR)
    out = np.zeros(n)

    # 三声「嘀」：间隔 0.125 秒，最后一声拔高，听觉上是"嘀 嘀 嘀↑"
    beeps = [(0.000, 1568.0, 0.075, 0.34), (0.125, 1568.0, 0.075, 0.34), (0.250, 2093.0, 0.085, 0.36)]
    for start, freq, length, amp in beeps:
        tick(out, rng, start, 0.006, 0.16, 1500.0, 9000.0)
        beep(out, start, freq, length, amp)

    out -= np.mean(out)
    out = out / (np.max(np.abs(out)) + 1e-9) * 0.86
    fade_in = int(0.001 * SR)
    fade_out = int(0.05 * SR)
    out[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    out[-fade_out:] *= np.linspace(1.0, 0.0, fade_out)

    os.makedirs(OUT_DIR, exist_ok=True)
    save(os.path.join(OUT_DIR, "lock.wav"), out)


if __name__ == "__main__":
    main()
