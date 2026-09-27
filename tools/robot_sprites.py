"""Build the robot hero and Killbot sheets from their CC0 sources.

Sources (not kept in the repo):
  - Hormelz "8 - Directional Drone Robot" free pack, unzipped: <src>/drone/Free8DirRobot/...
  - patvanmackelberg "Killbot (8 Directional)": <src>/sKillBot.png

Usage: python tools/robot_sprites.py E:/Tools/tmp/robot

Output rows follow facingDir(..., 8): 0 S, 1 SE, 2 E, 3 NE, 4 N, 5 NW, 6 W, 7 SW.
"""

import random
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
DIR_ORDER = ["S", "SE", "E", "NE", "N", "NW", "W", "SW"]

# ---- hero: 256px renders -> 96px frames, feet on row 79 (frame - 17, same margin as the 64px sheets)
HERO_FRAME = 96
HERO_FOOT = HERO_FRAME - 17
DRONE_CELL = 256
DRONE_FOOT = 141
DRONE_SCALE = HERO_FRAME / DRONE_CELL
DRONE_DIR = {"SW": 1, "W": 2, "NW": 3, "N": 4, "NE": 5, "E": 6, "SE": 7, "S": 8}

# ---- minion: 24px Killbot cells placed 1:1 into 64px frames, feet on row 47
BOT_FRAME = 64
BOT_CELL = 24
BOT_WALK_ROW = {"S": 0, "E": 1, "W": 2, "N": 3, "NE": 4, "SW": 5, "NW": 6, "SE": 7}
BOT_AIM_COL = {"E": 0, "NE": 1, "N": 2, "NW": 3, "W": 4, "SW": 5, "S": 6, "SE": 7}


def drone_frames(src: Path, anim: str, direction: str, step: int) -> list[Image.Image]:
    sheet = Image.open(src / "drone" / "Free8DirRobot" / anim / f"LowPolyManny_Blue_rig_{anim}_dir{DRONE_DIR[direction]}.png").convert("RGBA")
    cols = sheet.width // DRONE_CELL
    rows = sheet.height // DRONE_CELL
    frames = []
    for i in range(cols * rows):
        cell = sheet.crop(((i % cols) * DRONE_CELL, (i // cols) * DRONE_CELL, (i % cols + 1) * DRONE_CELL, (i // cols + 1) * DRONE_CELL))
        if cell.getchannel("A").getbbox() is None:
            continue
        frames.append(cell)
    frames = frames[::step]
    out = []
    for cell in frames:
        small = cell.convert("RGBa").resize((HERO_FRAME, HERO_FRAME), Image.LANCZOS).convert("RGBA")
        alpha = small.getchannel("A").point(lambda a: 0 if a < 24 else a)
        small = outline(brighten(small), alpha)
        frame = Image.new("RGBA", (HERO_FRAME, HERO_FRAME))
        frame.alpha_composite(small, (0, HERO_FOOT - round(DRONE_FOOT * DRONE_SCALE)))
        out.append(frame)
    return out


def brighten(img: Image.Image) -> Image.Image:
    # 原渲染偏暗，叠上游戏的暗角后几乎融进草地
    rgb = img.convert("RGB")
    rgb = ImageEnhance.Brightness(rgb).enhance(1.45)
    rgb = ImageEnhance.Contrast(rgb).enhance(1.15)
    rgb = ImageEnhance.Color(rgb).enhance(1.2)
    out = rgb.convert("RGBA")
    out.putalpha(img.getchannel("A"))
    return out


def outline(img: Image.Image, alpha: Image.Image) -> Image.Image:
    solid = alpha.point(lambda a: 255 if a >= 96 else 0)
    ring = ImageChops.subtract(solid.filter(ImageFilter.MaxFilter(3)), solid)
    out = Image.new("RGBA", img.size, (18, 16, 26, 0))
    out.putalpha(ring)
    body = img.copy()
    body.putalpha(alpha)
    out.alpha_composite(body)
    return out


def tint(img: Image.Image, color: tuple[int, int, int], amount: float) -> Image.Image:
    solid = Image.new("RGBA", img.size, color + (255,))
    mixed = Image.blend(img, solid, amount)
    mixed.putalpha(img.getchannel("A"))
    return mixed


def hero_hurt(base: list[Image.Image]) -> list[Image.Image]:
    return [tint(base[0], (255, 70, 50), 0.55), tint(base[1], (255, 70, 50), 0.35), tint(base[2], (255, 120, 90), 0.15)]


def hero_death(base: Image.Image, seed: int) -> list[Image.Image]:
    rng = random.Random(seed)
    frames = []
    for i in range(8):
        t = i / 7
        squash = 1.0 - 0.62 * t ** 1.4
        body = tint(base, (40, 44, 52), 0.55 * t)
        h = max(1, round(HERO_FRAME * squash))
        body = body.resize((HERO_FRAME, h), Image.LANCZOS)
        frame = Image.new("RGBA", (HERO_FRAME, HERO_FRAME))
        frame.alpha_composite(body, (0, HERO_FOOT - round(HERO_FOOT * squash)))
        alpha = frame.getchannel("A").point(lambda a: int(a * (1.0 - 0.35 * t)))
        frame.putalpha(alpha)
        if 1 <= i <= 5:
            draw = ImageDraw.Draw(frame)
            for _ in range(10 - i):
                x = HERO_FRAME // 2 + rng.randint(-14, 14)
                y = HERO_FOOT - rng.randint(4, int(34 * squash) + 4)
                color = rng.choice([(255, 220, 90, 255), (120, 230, 255, 255), (255, 150, 60, 255)])
                draw.rectangle((x, y, x + 1, y + 1), fill=color)
        frames.append(frame)
    return frames


def write_sheet(path: Path, rows: list[list[Image.Image]], frame: int) -> None:
    cols = max(len(r) for r in rows)
    sheet = Image.new("RGBA", (cols * frame, len(rows) * frame))
    for y, row in enumerate(rows):
        for x, img in enumerate(row):
            sheet.alpha_composite(img, (x * frame, y * frame))
    path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(path, optimize=True)
    print(path.relative_to(ROOT), sheet.size)


def build_hero(src: Path) -> None:
    out = ROOT / "assets" / "hero_robot"
    idle, run, attack, hurt, death = [], [], [], [], []
    for n, d in enumerate(DIR_ORDER):
        idle_d = drone_frames(src, "IdleAim", d, 2)
        idle.append(idle_d)
        run.append(drone_frames(src, "WalkingShoot", d, 2))
        attack.append(drone_frames(src, "StandingShoot", d, 1))
        hurt.append(hero_hurt(idle_d))
        death.append(hero_death(idle_d[0], 7 + n))
    write_sheet(out / "idle.png", idle, HERO_FRAME)
    write_sheet(out / "run.png", run, HERO_FRAME)
    write_sheet(out / "attack.png", attack, HERO_FRAME)
    write_sheet(out / "hurt.png", hurt, HERO_FRAME)
    write_sheet(out / "death.png", death, HERO_FRAME)


def bot_cell(sheet: Image.Image, col: int, row: int) -> Image.Image:
    cell = sheet.crop((col * BOT_CELL, row * BOT_CELL, (col + 1) * BOT_CELL, (row + 1) * BOT_CELL))
    frame = Image.new("RGBA", (BOT_FRAME, BOT_FRAME))
    frame.alpha_composite(cell, ((BOT_FRAME - BOT_CELL) // 2, 48 - BOT_CELL))
    return frame


def bot_death(base: Image.Image) -> list[Image.Image]:
    rng = random.Random(3)
    sparks = [(rng.uniform(-1, 1), rng.uniform(-1.2, 0.4), rng.choice([(255, 230, 120), (255, 140, 40), (200, 200, 205), (90, 90, 96)])) for _ in range(18)]
    frames = []
    for i in range(7):
        t = i / 6
        frame = Image.new("RGBA", (BOT_FRAME, BOT_FRAME))
        if i < 3:
            body = tint(base, (255, 255, 255) if i == 0 else (255, 170, 60), 0.75 - 0.2 * i)
            body.putalpha(base.getchannel("A").point(lambda a: int(a * (1.0 - 0.3 * i))))
            frame.alpha_composite(body)
        draw = ImageDraw.Draw(frame)
        cx, cy = 32, 36
        if i <= 2:
            r = 2 + 3 * i
            draw.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 190, 70, 230 - 80 * i))
        for dx, dy, color in sparks:
            dist = 4 + 18 * t
            x = round(cx + dx * dist)
            y = round(cy + dy * dist + 10 * t * t)
            a = int(255 * (1.0 - t * 0.85))
            draw.rectangle((x, y, x + 1, y + 1), fill=color + (a,))
        frames.append(frame)
    return frames


def build_killbot(src: Path) -> None:
    out = ROOT / "assets" / "killbot"
    sheet = Image.open(src / "sKillBot.png").convert("RGBA")
    walk = [[bot_cell(sheet, c, BOT_WALK_ROW[d]) for c in range(4)] for d in DIR_ORDER]
    attack = [[bot_cell(sheet, BOT_AIM_COL[d], 8)] for d in DIR_ORDER]
    write_sheet(out / "walk.png", walk, BOT_FRAME)
    write_sheet(out / "attack.png", attack, BOT_FRAME)
    write_sheet(out / "death.png", [bot_death(walk[0][0])], BOT_FRAME)


if __name__ == "__main__":
    source = Path(sys.argv[1] if len(sys.argv) > 1 else "E:/Tools/tmp/robot")
    build_hero(source)
    build_killbot(source)
