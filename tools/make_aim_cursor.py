"""生成局内瞄准光标（assets/cursor/aim.cur）。

系统自带的 precision 准星太细，压在亮色地面 / 特效上几乎看不见。这里重画一个：
- 先铺一圈黑色描边，浅底色和深底色都能压得住；
- 中间是白芯的四向粗刻度，外圈一道青色细环，锁定 / 瞄准时眼睛能第一时间找到点；
- 中心留一个小圆点，方便对准像素级的目标；
- 热点取正中心，和原来一致。

Qt 的 ico 插件能读 .cur 的像素（但读不到热点），热点由 Cursors::loadCursor 自己解析文件头，
所以这里写成标准 CUR：类型 2 + 32 位 BGRA + 每行按 4 字节对齐的 AND 掩码。
用法：python tools/make_aim_cursor.py
"""

import os
import struct

from PIL import Image, ImageDraw

SIZE = 64          # 位图边长：运行时按 32 逻辑像素 × 屏幕缩放比再缩，留大一点更清楚
SUPERSAMPLE = 4    # 先放大画再缩，边缘才有抗锯齿

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "cursor")


def draw_mark(scale):
    """按 scale 倍尺寸画一遍准星，返回 RGBA 图。"""
    side = SIZE * scale
    img = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    c = side / 2.0

    dark = (5, 14, 18, 245)
    cyan = (74, 236, 255, 255)
    white = (248, 255, 255, 255)

    def circle(radius, width, color):
        draw.ellipse([c - radius, c - radius, c + radius, c + radius], outline=color, width=width)

    def bar(ax, ay, inner, outer, half, fill):
        """一根从内到外的粗刻度，端头留出圆角。"""
        if ax:
            a = c + ax * inner
            b = c + ax * outer
            box = [min(a, b) - half, c - half, max(a, b) + half, c + half]
        else:
            a = c + ay * inner
            b = c + ay * outer
            box = [c - half, min(a, b) - half, c + half, max(a, b) + half]
        draw.rounded_rectangle(box, radius=half, fill=fill)

    def tick(ax, ay):
        bar(ax, ay, 22.0 * scale, 30.5 * scale, 3.2 * scale, dark)

    def tick_core(ax, ay):
        bar(ax, ay, 23.5 * scale, 29.0 * scale, 1.9 * scale, cyan)

    # 描边层：圈 + 四向刻度 + 中心点，先整铺一遍
    circle(17.0 * scale, max(1, int(6.5 * scale)), dark)
    for ax, ay in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        tick(ax, ay)
    dot = 3.4 * scale
    draw.ellipse([c - dot, c - dot, c + dot, c + dot], fill=dark)

    # 亮层：青色细环、白芯刻度、中心白点
    circle(17.0 * scale, max(1, int(2.6 * scale)), cyan)
    for ax, ay in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        tick_core(ax, ay)
    dot = 1.8 * scale
    draw.ellipse([c - dot, c - dot, c + dot, c + dot], fill=white)
    return img


def write_cur(path, image, hotspot):
    """按 CUR 格式落盘：ICONDIR + 一条目录项 + DIB（BGRA 自下而上 + AND 掩码）。"""
    width, height = image.size
    px = image.load()
    rows = []
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            r, g, b, a = px[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    xor = b"".join(rows)
    # 32 位图带 alpha，AND 掩码留全 0 即可；老式渲染器仍按每行 4 字节对齐去读
    mask_row = ((width + 31) // 32) * 4
    mask = bytes(mask_row * height)
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0)
    data = header + xor + mask
    entry = struct.pack("<BBBBHHII", width % 256, height % 256, 0, 0, hotspot[0], hotspot[1], len(data), 6 + 16)
    with open(path, "wb") as out:
        out.write(struct.pack("<HHH", 0, 2, 1) + entry + data)


def main():
    mark = draw_mark(SUPERSAMPLE).resize((SIZE, SIZE), Image.LANCZOS)
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "aim.cur")
    write_cur(path, mark, (SIZE // 2, SIZE // 2))
    print(f"{os.path.basename(path)}: {SIZE}x{SIZE}  {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    main()
