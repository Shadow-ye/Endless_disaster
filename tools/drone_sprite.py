"""生成机甲人蜂群技能用的小型四旋翼无人机像素图。

输出 assets/drone/drone.png：4 帧横排，每帧 16x16，帧间只有旋翼和指示灯变化。
用法：python tools/drone_sprite.py [放大 8 倍的预览图路径]
"""
import sys
from pathlib import Path

from PIL import Image

FRAME = 16
FRAMES = 4

OUTLINE = (22, 26, 32, 255)
HUB = (58, 66, 78, 255)
BODY = (136, 150, 168, 255)
LIGHT = (200, 214, 228, 255)
SHADE = (88, 98, 114, 255)
EYE = (110, 230, 255, 255)
LED_ON = (255, 72, 60, 255)
LED_OFF = (90, 40, 40, 255)
BLADE = (214, 222, 232, 255)
BLUR = (226, 234, 242, 210)
BLUR_EDGE = (200, 210, 222, 110)

HUBS = [(2, 3), (13, 3), (2, 12), (13, 12)]
ARMS = [(3, 4), (4, 5), (12, 4), (11, 5), (3, 11), (4, 10), (12, 11), (11, 10)]


def put(img, ox, x, y, color):
    if 0 <= x < FRAME and 0 <= y < FRAME:
        img.putpixel((ox + x, y), color)


def draw_frame(img, index):
    ox = index * FRAME
    for x, y in ARMS:
        put(img, ox, x, y, OUTLINE)
    for x, y in HUBS:
        put(img, ox, x, y, HUB)

    # 机身 6x6，圆角
    for y in range(5, 11):
        for x in range(5, 11):
            edge = x in (5, 10) or y in (5, 10)
            corner = x in (5, 10) and y in (5, 10)
            if corner:
                continue
            if edge:
                color = OUTLINE
            elif y == 6:
                color = LIGHT
            elif y == 9:
                color = SHADE
            else:
                color = BODY
            put(img, ox, x, y, color)
    put(img, ox, 7, 8, EYE)
    put(img, ox, 8, 8, EYE)
    put(img, ox, 8, 6, LED_ON if index < 2 else LED_OFF)

    # 旋翼在桨毂上方一格；对角两组相位相反，看起来在转
    for i, (hx, hy) in enumerate(HUBS):
        phase = (index + (1 if i in (1, 2) else 0)) % 2
        by = hy - 1
        if phase == 0:
            for dx in range(-2, 3):
                put(img, ox, hx + dx, by, BLADE)
        else:
            put(img, ox, hx, by, BLUR)
            put(img, ox, hx - 1, by, BLUR)
            put(img, ox, hx + 1, by, BLUR)
            put(img, ox, hx - 2, by, BLUR_EDGE)
            put(img, ox, hx + 2, by, BLUR_EDGE)


def main():
    img = Image.new("RGBA", (FRAME * FRAMES, FRAME), (0, 0, 0, 0))
    for i in range(FRAMES):
        draw_frame(img, i)
    out = Path(__file__).resolve().parent.parent / "assets" / "drone" / "drone.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    if len(sys.argv) > 1:
        img.resize((img.width * 8, img.height * 8), Image.NEAREST).save(sys.argv[1])
    print(out)


if __name__ == "__main__":
    main()
