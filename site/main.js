'use strict';

const REPO = 'Shadow-ye/Endless_disaster';
const FONT = '"PingFang SC","Microsoft YaHei","Noto Sans SC",sans-serif';
const $ = (s) => document.querySelector(s);
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const rand = (a, b) => a + Math.random() * (b - a);
const pick = (arr) => arr[(Math.random() * arr.length) | 0];
const TAU = Math.PI * 2;

/* ================= 精灵 ================= */

const SHEETS = {
  warrior: { dir: 'hero_warrior', size: 64, anims: ['idle', 'run', 'attack', 'hurt', 'death'] },
  sword: { dir: 'hero_sword', size: 64, anims: ['idle', 'run', 'attack', 'hurt', 'death'] },
  mage: { dir: 'hero_mage', size: 64, anims: ['idle', 'run', 'attack', 'hurt', 'death'] },
  robot: { dir: 'hero_robot', size: 96, anims: ['idle', 'run', 'attack', 'hurt', 'death'] },
  slime: { dir: 'slime', size: 64, anims: ['idle', 'walk', 'death'] },
  skeleton: { dir: 'skeleton', size: 64, anims: ['idle', 'walk', 'attack', 'defense', 'hurt', 'death'] },
  mushroom: { dir: 'mushroom', size: 64, anims: ['idle', 'jump', 'attack', 'death'] },
  flyer: { dir: 'flyer', size: 64, anims: ['idle', 'fly', 'attack', 'hurt', 'death'] },
  killbot: { dir: 'killbot', size: 64, anims: ['walk', 'attack', 'death'] },
};
const SPR = {};
let TILES = null;
let DRONE = null;

function loadImage(src) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error('无法加载 ' + src));
    img.src = src;
  });
}

async function loadSprites() {
  const jobs = [];
  for (const [key, def] of Object.entries(SHEETS)) {
    SPR[key] = {};
    for (const anim of def.anims) {
      jobs.push(loadImage(`sprites/${def.dir}/${anim}.png`).then((img) => {
        SPR[key][anim] = {
          img, size: def.size,
          frames: Math.max(1, Math.floor(img.width / def.size)),
          dirs: Math.max(1, Math.floor(img.height / def.size)),
          cache: {},
        };
      }));
    }
  }
  jobs.push(loadImage('sprites/tiles/tilemaps.png').then((img) => { TILES = img; }));
  jobs.push(loadImage('sprites/drone/drone.png').then((img) => { DRONE = img; }));
  await Promise.all(jobs);
}

// 与游戏相同的着色：像素颜色与目标色各取一半；white 为受击闪白
function tinted(s, color) {
  if (!s.cache[color]) {
    const c = document.createElement('canvas');
    c.width = s.img.width;
    c.height = s.img.height;
    const g = c.getContext('2d');
    g.drawImage(s.img, 0, 0);
    g.globalCompositeOperation = 'source-atop';
    g.globalAlpha = color === 'white' ? 0.85 : 0.5;
    g.fillStyle = color === 'white' ? '#fff' : color;
    g.fillRect(0, 0, c.width, c.height);
    s.cache[color] = c;
  }
  return s.cache[color];
}

function frameAt(s, t, fps, loop) {
  const n = Math.floor(t * fps);
  return loop ? n % s.frames : Math.min(n, s.frames - 1);
}

// (x, y) 是脚底位置；帧内脚底距帧底 17 像素，与游戏一致
function drawSprite(ctx, s, frame, x, y, o = {}) {
  const f = clamp(frame | 0, 0, s.frames - 1);
  const d = clamp(o.dir || 0, 0, s.dirs - 1);
  const sc = o.scale || 1;
  const size = s.size * sc;
  const img = o.tint ? tinted(s, o.tint) : s.img;
  ctx.save();
  ctx.translate(x, y - (o.lift || 0));
  if (o.flip) ctx.scale(-1, 1);
  if (o.alpha != null) ctx.globalAlpha *= o.alpha;
  ctx.drawImage(img, f * s.size, d * s.size, s.size, s.size, -size / 2, -(s.size - 17) * sc, size, size);
  ctx.restore();
}

// 8 方向行序：0 南、1 东南、2 东、3 东北、4 北、5 西北、6 西、7 西南
function facingDir(fx, fy) {
  const k = Math.round(Math.atan2(fy, fx) / (Math.PI / 4));
  return (((2 - k) % 8) + 8) % 8;
}

function hash(x, y) {
  let h = (Math.imul(x, 374761393) + Math.imul(y, 668265263)) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return (h ^ (h >>> 16)) >>> 0;
}

function grassCanvas(tilesW, tilesH, scale, seed) {
  const c = document.createElement('canvas');
  c.width = tilesW * 16 * scale;
  c.height = tilesH * 16 * scale;
  const g = c.getContext('2d');
  g.imageSmoothingEnabled = false;
  for (let ty = 0; ty < tilesH; ty++) {
    for (let tx = 0; tx < tilesW; tx++) {
      const d = 16 * scale;
      g.drawImage(TILES, 80, 16, 16, 16, tx * d, ty * d, d, d);
      if (hash(tx + seed, ty) % 11 === 0) g.drawImage(TILES, 16, 0, 16, 16, tx * d, ty * d, d, d);
    }
  }
  return c;
}

/* ================= 动画调度：只更新可见区域 ================= */

const tickers = [];
const visIO = new IntersectionObserver((entries) => {
  for (const e of entries) {
    for (const t of tickers) if (t.el === e.target) t.on = e.isIntersecting;
  }
}, { rootMargin: '120px' });

function addTicker(el, fn) {
  tickers.push({ el, fn, on: false });
  visIO.observe(el);
}

let lastNow = performance.now();
function loop(now) {
  requestAnimationFrame(loop);
  const dt = Math.min(0.05, (now - lastNow) / 1000);
  lastNow = now;
  for (const t of tickers) {
    if (!t.on) continue;
    try {
      t.fn(dt, now / 1000);
    } catch (e) {
      t.on = false;
      console.error(e);
    }
  }
}

/* ================= 首屏：实时战斗 ================= */

const HERO_KEYS = ['warrior', 'sword', 'mage', 'robot'];
const HERO_CN = { warrior: '战士', sword: '女剑客', mage: '女魔法师', robot: '机甲人' };
const HERO_COLOR = { warrior: '#ff6b4a', sword: '#8fb8ff', mage: '#b98cff', robot: '#5eead4' };
const HERO_CRIT = { warrior: 0.12, sword: 0.22, mage: 0.12, robot: 0.12 };

const MON = {
  slime: { sheet: 'slime', hp: 18, speed: 20, move: 'walk', death: 'death', xp: 1, scale: 1, reach: 13, dmg: 3 },
  skeleton: { sheet: 'skeleton', hp: 32, speed: 17, move: 'walk', attack: 'attack', death: 'death', xp: 2, scale: 1, reach: 18, dmg: 5 },
  mushroom: { sheet: 'mushroom', hp: 24, speed: 22, move: 'idle', attack: 'jump', death: 'death', xp: 1, scale: 1.25, reach: 18, dmg: 4 },
  flyer: { sheet: 'flyer', hp: 20, speed: 30, move: 'fly', attack: 'attack', death: 'death', xp: 2, scale: 1, lift: 12, reach: 18, dmg: 5 },
  caster: { sheet: 'mushroom', hp: 26, shield: 8, speed: 18, move: 'idle', attack: 'jump', death: 'death', xp: 3, scale: 1.15, tint: 'rgb(88,42,112)', ranged: 105, dmg: 4 },
  killbot: { sheet: 'killbot', hp: 28, speed: 32, move: 'walk', attack: 'attack', death: 'death', xp: 3, scale: 1.2, ranged: 90, orbit: true, dmg: 4 },
};

function createBattle(canvas) {
  const ctx = canvas.getContext('2d');
  const hud = {
    cls: $('#hudClass'), lv: $('#hudLevel'), hp: $('#hudHp'), mp: $('#hudMp'), xp: $('#hudXp'),
    time: $('#hudTime'), kills: $('#hudKills'),
  };
  let W = 0, H = 0, S = 3, dpr = 1, VW = 0, VH = 0;
  const pattern = ctx.createPattern(grassCanvas(32, 32, 1, 7), 'repeat');

  const P = {
    x: 0, y: 0, fx: 1, fy: 0, anim: 'idle', animT: 0,
    atkT: 0, atkLen: 0, hitAt: 0, hitDone: true, cd: 0,
    skillCd: 1.5, bigCd: 4, healCd: 0, hurtT: 0, dashT: 0, dashVx: 0, dashVy: 0, dashHit: new Set(),
    qiQueue: 0, qiT: 0, berserkT: 0, fireT: 0, ammo: 30, reloadT: 0,
    hp: 120, maxHp: 120, mp: 60, maxMp: 60, level: 1, xp: 0, need: 6,
    heroIdx: 0, heroT: 0, moving: false,
  };
  const cam = { x: 0, y: 0 };
  let shake = 0;
  let monsters = [], bolts = [], fx = [], floats = [], orbs = [], drones = [], parts = [];
  let time = 0, kills = 0, hudT = 0;

  const hero = () => HERO_KEYS[P.heroIdx];

  function resize() {
    dpr = Math.min(window.devicePixelRatio || 1, 2);
    W = canvas.clientWidth;
    H = canvas.clientHeight;
    canvas.width = Math.round(W * dpr);
    canvas.height = Math.round(H * dpr);
    S = clamp(Math.round(Math.min(W * 1.3, H) / 320), 2, 5);
    VW = W / S;
    VH = H / S;
  }

  function applyHeroStats() {
    const stats = { warrior: [120, 60], sword: [95, 85], mage: [78, 130], robot: [105, 90] }[hero()];
    const ratio = P.hp / P.maxHp;
    P.maxHp = stats[0] + (P.level - 1) * 6;
    P.maxMp = stats[1];
    P.hp = P.maxHp * ratio;
    P.mp = Math.min(P.mp, P.maxMp);
  }

  function switchHero() {
    P.heroIdx = (P.heroIdx + 1) % HERO_KEYS.length;
    P.heroT = 0;
    P.atkT = 0;
    P.qiQueue = 0;
    P.berserkT = 0;
    P.skillCd = 0.8;
    P.bigCd = 2.5;
    P.ammo = 30;
    applyHeroStats();
    const c = HERO_COLOR[hero()];
    fx.push({ kind: 'ring', x: P.x, y: P.y, r: 60, t: 0, dur: 0.6, color: c, w: 3 });
    fx.push({ kind: 'pillar', x: P.x, y: P.y, t: 0, dur: 0.7, color: c });
    for (let i = 0; i < 26; i++) burstPart(P.x, P.y - 10, c, 90);
    say(HERO_CN[hero()], c, 10, 1.4);
    hud.cls.textContent = HERO_CN[hero()];
  }

  function say(text, color, size = 8, dur = 1) {
    floats.push({ x: P.x, y: P.y - 44, text, color, size, t: 0, dur, big: true });
  }

  function burstPart(x, y, color, speed = 60, size = 2) {
    const a = rand(0, TAU), v = rand(speed * 0.3, speed);
    parts.push({ x, y, vx: Math.cos(a) * v, vy: Math.sin(a) * v - 20, t: 0, dur: rand(0.3, 0.7), color, size });
  }

  function spawnMonster(near) {
    const kinds = ['slime', 'slime', 'skeleton', 'skeleton'];
    if (time > 4) kinds.push('flyer', 'flyer');
    if (time > 8) kinds.push('mushroom', 'mushroom');
    if (time > 12) kinds.push('caster');
    if (time > 16) kinds.push('killbot', 'killbot');
    const type = pick(kinds);
    const def = MON[type];
    const a = rand(0, TAU);
    const d = near ? rand(90, 150) : Math.hypot(VW, VH) * 0.55 + rand(10, 40);
    const mult = Math.min(2.2, 1 + time / 120);
    monsters.push({
      type, def, x: P.x + Math.cos(a) * d, y: P.y + Math.sin(a) * d,
      hp: def.hp * mult, maxHp: def.hp * mult, shield: def.shield || 0,
      anim: def.move, animT: rand(0, 1), fx: -Math.cos(a), fy: -Math.sin(a), flip: false,
      atkT: 0, atkCd: rand(0.5, 1.5), hitDone: false, hurtT: 0, defT: 0, dead: false, deadT: 0,
      orbitDir: Math.random() < 0.5 ? 1 : -1, speedMul: rand(0.85, 1.15),
    });
  }

  function nearest(x, y, maxD = Infinity) {
    let best = null, bd = maxD;
    for (const m of monsters) {
      if (m.dead) continue;
      const d = Math.hypot(m.x - x, m.y - y);
      if (d < bd) { bd = d; best = m; }
    }
    return [best, bd];
  }

  function countNear(r) {
    let n = 0;
    for (const m of monsters) if (!m.dead && Math.hypot(m.x - P.x, m.y - P.y) < r) n++;
    return n;
  }

  function hurtMonster(m, base, knock, fromX, fromY) {
    if (m.dead) return;
    const crit = Math.random() < HERO_CRIT[hero()];
    let dmg = base * rand(0.85, 1.15) * (1 + (P.level - 1) * 0.07) * (P.berserkT > 0 ? 2 : 1);
    if (crit) dmg *= 1.8;
    if (m.type === 'skeleton') {
      if (m.defT > 0) dmg *= 0.4;
      else if (Math.random() < 0.18 && m.atkT <= 0) { m.defT = 0.8; }
    }
    dmg = Math.max(1, Math.round(dmg));
    if (m.shield > 0) {
      const absorbed = Math.min(m.shield, dmg);
      m.shield -= absorbed;
      m.hp -= dmg - absorbed;
    } else {
      m.hp -= dmg;
    }
    m.hurtT = crit ? 0.22 : 0.12;
    const lift = m.def.lift || 0;
    floats.push({ x: m.x + rand(-5, 5), y: m.y - 26 - lift, text: String(dmg), color: crit ? '#ffd166' : '#ffffff', size: crit ? 9 : 7, t: 0, dur: 0.75, crit });
    for (let i = 0; i < (crit ? 7 : 3); i++) burstPart(m.x, m.y - 10 - lift, crit ? '#ffd166' : '#ffe9d6', 70, 1.5);
    if (knock > 0) {
      const dx = m.x - fromX, dy = m.y - fromY, d = Math.hypot(dx, dy) || 1;
      m.x += (dx / d) * knock;
      m.y += (dy / d) * knock;
    }
    if (m.hp <= 0) killMonster(m);
  }

  function killMonster(m) {
    m.dead = true;
    m.deadT = 0;
    m.anim = m.def.death;
    m.animT = 0;
    kills++;
    const lift = m.def.lift || 0;
    for (let i = 0; i < 10; i++) burstPart(m.x, m.y - 8 - lift, m.type === 'caster' ? '#b98cff' : '#ff8a6a', 80);
    for (let i = 0; i < m.def.xp; i++) orbs.push({ x: m.x + rand(-6, 6), y: m.y + rand(-4, 4), t: rand(0, 3), v: 0, age: 0 });
  }

  function hurtPlayer(dmg) {
    if (P.dashT > 0 || P.hurtT > 0) return;
    P.hp = Math.max(P.maxHp * 0.12, P.hp - dmg);
    P.hurtT = 0.5;
    floats.push({ x: P.x + rand(-4, 4), y: P.y - 34, text: '-' + dmg, color: '#ff5a4e', size: 7, t: 0, dur: 0.7 });
  }

  function melee(range, dotMin, base, knock) {
    for (const m of monsters) {
      if (m.dead) continue;
      const dx = m.x - P.x, dy = m.y - P.y, d = Math.hypot(dx, dy);
      if (d < range && (d < 6 || (dx * P.fx + dy * P.fy) / d > dotMin)) hurtMonster(m, base, knock, P.x, P.y);
    }
  }

  function area(x, y, r, base, knock) {
    for (const m of monsters) {
      if (!m.dead && Math.hypot(m.x - x, m.y - y) < r) hurtMonster(m, base, knock, x, y);
    }
  }

  function fireBolt(kind, angleOffset = 0) {
    const a = Math.atan2(P.fy, P.fx) + angleOffset;
    const spec = {
      mage: { speed: 230, life: 0.7, dmg: 12, r: 8 },
      robot: { speed: 320, life: 0.5, dmg: 10, r: 7 },
      scatter: { speed: 320, life: 0.45, dmg: 9, r: 7 },
      qi: { speed: 210, life: 0.55, dmg: 18, r: 14, pierce: true },
    }[kind];
    const lift = kind === 'robot' || kind === 'scatter' ? 14 : kind === 'qi' ? 10 : 12;
    bolts.push({
      kind, x: P.x + Math.cos(a) * 12, y: P.y + Math.sin(a) * 12, lift,
      vx: Math.cos(a) * spec.speed, vy: Math.sin(a) * spec.speed,
      life: spec.life, dmg: spec.dmg, r: spec.r, pierce: !!spec.pierce, hit: new Set(), trail: [],
    });
  }

  function startAttack(len, hitAt) {
    P.atkT = len;
    P.atkLen = len;
    P.hitAt = hitAt;
    P.hitDone = false;
  }

  function faceTo(tx, ty) {
    const dx = tx - P.x, dy = ty - P.y, d = Math.hypot(dx, dy);
    if (d > 0.01) { P.fx = dx / d; P.fy = dy / d; }
  }

  function tryCastSkills(target, nd) {
    const h = hero();
    if (P.hp < P.maxHp * 0.45 && P.healCd <= 0 && P.mp >= 20) {
      const amount = h === 'mage' ? Math.round(P.maxHp * 0.15) : 22;
      P.hp = Math.min(P.maxHp, P.hp + amount);
      P.mp -= 20;
      P.healCd = h === 'mage' ? 12 : 5.5;
      fx.push({ kind: 'heal', x: P.x, y: P.y, t: 0, dur: 0.9 });
      floats.push({ x: P.x, y: P.y - 40, text: '+' + amount, color: '#7ee07e', size: 8, t: 0, dur: 0.9 });
      say(h === 'mage' ? '治疗术' : '恢复', '#7ee07e', 7, 0.9);
      return;
    }
    if (!target) return;
    if (h === 'warrior') {
      if (P.bigCd <= 0 && nd < 60 && P.mp >= 18) {
        P.mp -= 18; P.bigCd = 8; P.berserkT = 3;
        say('狂化', '#ff4b3e');
        fx.push({ kind: 'ring', x: P.x, y: P.y, r: 36, t: 0, dur: 0.5, color: '#ff4b3e', w: 3 });
      } else if (P.skillCd <= 0 && countNear(46) >= 2 && P.mp >= 16) {
        P.mp -= 16; P.skillCd = 2.8;
        startAttack(0.4, 0.05);
        P.hitDone = true;
        fx.push({ kind: 'spin', x: P.x, y: P.y, r: 42, t: 0, dur: 0.4, color: '#ffe0c2' });
        area(P.x, P.y, 42, 16, 14);
        say('回旋斩', '#ffd166', 7, 0.8);
      }
    } else if (h === 'sword') {
      if (P.bigCd <= 0 && nd < 90 && nd > 24 && P.mp >= 8) {
        P.mp -= 8; P.bigCd = 3.2;
        faceTo(target.x, target.y);
        P.dashT = 0.2; P.dashVx = P.fx * 260; P.dashVy = P.fy * 260; P.dashHit = new Set();
        startAttack(0.3, 0.3);
        P.hitDone = true;
        say('突刺', '#8fb8ff', 7, 0.7);
      } else if (P.skillCd <= 0 && nd < 110 && P.mp >= 10) {
        P.mp -= 10; P.skillCd = 4;
        faceTo(target.x, target.y);
        P.qiQueue = 3; P.qiT = 0;
        say('剑气 ×3', '#8fb8ff', 7, 0.8);
      }
    } else if (h === 'mage') {
      if (P.bigCd <= 0 && countNear(110) >= 5 && P.mp >= 40) {
        P.mp -= 40; P.bigCd = 10;
        startAttack(0.55, 0.55);
        P.hitDone = true;
        fx.push({ kind: 'burial', x: P.x, y: P.y, r: 110, t: 0, dur: 1.4, fired: false });
        say('万葬', '#d8b4fe', 10, 1.3);
      } else if (P.skillCd <= 0 && countNear(64) >= 2 && P.mp >= 22) {
        P.mp -= 22; P.skillCd = 4.2;
        startAttack(0.35, 0.05);
        P.hitDone = true;
        fx.push({ kind: 'ring', x: P.x, y: P.y, r: 64, t: 0, dur: 0.45, color: '#c4a1ff', w: 4, fill: true });
        area(P.x, P.y, 64, 12, 12);
        say('新星', '#c4a1ff', 7, 0.8);
      }
    } else if (h === 'robot') {
      if (P.bigCd <= 0 && P.mp >= 22) {
        P.mp -= 22; P.bigCd = 7;
        for (let i = 0; i < 6; i++) {
          const a = (i / 6) * TAU;
          drones.push({ x: P.x + Math.cos(a) * 14, y: P.y + Math.sin(a) * 14, vx: Math.cos(a) * 60, vy: Math.sin(a) * 60, t: 0, target: null });
        }
        say('蜂群', '#5eead4');
      } else if (P.skillCd <= 0 && nd < 120 && P.mp >= 14) {
        P.mp -= 14; P.skillCd = 2.4;
        faceTo(target.x, target.y);
        for (let i = -2; i <= 2; i++) fireBolt('scatter', i * 0.18);
        startAttack(0.3, 0.3);
        P.hitDone = true;
        say('散射', '#5eead4', 7, 0.7);
      }
    }
  }

  function updatePlayer(dt) {
    const h = hero();
    const ranged = h === 'mage' || h === 'robot';
    P.heroT += dt;
    if (P.heroT > 11) switchHero();
    P.cd -= dt; P.skillCd -= dt; P.bigCd -= dt; P.healCd -= dt; P.hurtT -= dt; P.berserkT -= dt;
    P.mp = Math.min(P.maxMp, P.mp + 7 * dt);
    P.hp = Math.min(P.maxHp, P.hp + 1.2 * dt);

    const [target, nd] = nearest(P.x, P.y);
    let mx = 0, my = 0;

    if (P.dashT > 0) {
      P.dashT -= dt;
      P.x += P.dashVx * dt;
      P.y += P.dashVy * dt;
      parts.push({ x: P.x, y: P.y - 12, vx: 0, vy: 0, t: 0, dur: 0.25, color: 'rgba(143,184,255,.6)', size: 6 });
      for (const m of monsters) {
        if (!m.dead && !P.dashHit.has(m) && Math.hypot(m.x - P.x, m.y - P.y) < 20) {
          P.dashHit.add(m);
          hurtMonster(m, 14, 12, P.x, P.y);
        }
      }
    } else {
      if (P.atkT <= 0) tryCastSkills(target, nd);
      const busy = P.atkT > 0 && !ranged;
      if (!busy) {
        if (target && !ranged && nd < 170 && nd > 18) {
          mx = (target.x - P.x) / nd; my = (target.y - P.y) / nd;
        } else if (target && ranged && nd < 48) {
          mx = (P.x - target.x) / nd; my = (P.y - target.y) / nd;
        } else if (!target || nd > (ranged ? 125 : 170)) {
          const hx = time * 9 + Math.cos(time * 0.13) * 70;
          const hy = Math.sin(time * 0.09) * 80;
          const dx = hx - P.x, dy = hy - P.y, d = Math.hypot(dx, dy);
          if (d > 8) { mx = dx / d; my = dy / d; }
        }
      }
      const speed = (h === 'sword' ? 52 : 44) * (ranged && P.atkT > 0 ? 0.55 : 1);
      P.x += mx * speed * dt;
      P.y += my * speed * dt;
    }
    P.moving = mx !== 0 || my !== 0;

    // 剑气连发
    if (P.qiQueue > 0) {
      P.qiT -= dt;
      if (P.qiT <= 0) {
        if (target) faceTo(target.x, target.y);
        fireBolt('qi');
        startAttack(0.22, 0.22);
        P.hitDone = true;
        P.qiQueue--;
        P.qiT = 0.14;
      }
    }

    // 普攻
    if (P.atkT > 0) {
      P.atkT -= dt;
      if (!P.hitDone && P.atkLen - P.atkT >= P.hitAt) {
        P.hitDone = true;
        if (h === 'mage') fireBolt('mage');
        else {
          melee(36, 0.1, h === 'warrior' ? 11 : 10, 10);
          fx.push({ kind: 'slash', x: P.x, y: P.y, ang: Math.atan2(P.fy, P.fx), r: 30, t: 0, dur: 0.18, color: P.berserkT > 0 ? '#ff7a6a' : '#ffffff' });
        }
      }
    } else if (target && P.dashT <= 0 && P.qiQueue <= 0) {
      if (h === 'robot') {
        // 点射，由下面的连射逻辑处理
      } else if (!ranged && nd < 30 && P.cd <= 0) {
        faceTo(target.x, target.y);
        const rate = P.berserkT > 0 ? 2 : 1;
        startAttack(0.5 / rate, 0.2 / rate);
        P.cd = 0.08;
      } else if (h === 'mage' && nd < 125 && P.cd <= 0) {
        faceTo(target.x, target.y);
        startAttack(0.5, 0.22);
        P.cd = 0.25;
      }
    }

    // 机甲人：弹匣 30 发，打空后换弹
    let firing = false;
    if (h === 'robot' && P.dashT <= 0) {
      if (P.reloadT > 0) {
        P.reloadT -= dt;
        if (P.reloadT <= 0) P.ammo = 30;
      } else if (target && nd < 130) {
        firing = true;
        faceTo(target.x, target.y);
        P.fireT -= dt;
        if (P.fireT <= 0) {
          fireBolt('robot', rand(-0.05, 0.05));
          P.fireT = 0.12;
          P.ammo--;
          if (P.ammo <= 0) {
            P.reloadT = 0.9;
            floats.push({ x: P.x, y: P.y - 44, text: '换弹', color: '#e6e0ee', size: 6, t: 0, dur: 0.8 });
          }
        }
      }
    }

    if (P.moving && P.atkT <= 0 && !firing && P.dashT <= 0) {
      const d = Math.hypot(mx, my);
      P.fx = mx / d; P.fy = my / d;
    }

    let anim = 'idle';
    if (P.hurtT > 0.18 && P.atkT <= 0 && !firing && P.dashT <= 0) anim = 'hurt';
    else if (P.atkT > 0 || firing || P.dashT > 0) anim = 'attack';
    else if (P.moving) anim = 'run';
    if (anim !== P.anim) { P.anim = anim; P.animT = 0; }
    P.animT += dt;
  }

  function updateMonsters(dt) {
    for (const m of monsters) {
      m.animT += dt;
      m.hurtT -= dt;
      m.defT -= dt;
      if (m.dead) { m.deadT += dt; continue; }
      const def = m.def;
      const dx = P.x - m.x, dy = P.y - m.y, d = Math.hypot(dx, dy) || 1;
      m.fx = dx / d; m.fy = dy / d;
      m.flip = dx < 0;
      m.atkCd -= dt;
      let vx = 0, vy = 0;
      const speed = def.speed * m.speedMul * (m.hurtT > 0 ? 0.35 : 1);
      if (m.atkT > 0) {
        m.atkT -= dt;
        if (!m.hitDone && m.atkT < 0.2) {
          m.hitDone = true;
          if (!def.ranged && d < (def.reach || 18) + 10) hurtPlayer(def.dmg + ((Math.random() * 3) | 0));
        }
      } else if (def.ranged) {
        if (def.orbit && d < def.ranged + 15) {
          const radial = (d - def.ranged) / 20;
          vx = (-m.fy * m.orbitDir + m.fx * radial) * speed;
          vy = (m.fx * m.orbitDir + m.fy * radial) * speed;
        } else if (d > def.ranged) {
          vx = m.fx * speed; vy = m.fy * speed;
        } else if (d < def.ranged - 30) {
          vx = -m.fx * speed * 0.6; vy = -m.fy * speed * 0.6;
        }
        if (m.atkCd <= 0 && d < def.ranged + 40) {
          m.atkCd = m.type === 'killbot' ? rand(1.4, 2.2) : rand(2, 2.8);
          m.atkT = 0.3;
          m.hitDone = true;
          const sp = m.type === 'killbot' ? 150 : 110;
          bolts.push({
            kind: m.type === 'killbot' ? 'enemy' : 'curse', x: m.x + m.fx * 8, y: m.y + m.fy * 8, lift: 10,
            vx: m.fx * sp, vy: m.fy * sp, life: 1.6, dmg: def.dmg, r: 7, enemy: true, hit: new Set(), trail: [],
          });
        }
      } else if (d > (def.reach || 16)) {
        vx = m.fx * speed; vy = m.fy * speed;
      } else if (m.atkCd <= 0) {
        m.atkCd = rand(1.1, 1.6);
        if (def.attack) {
          m.atkT = SPR[def.sheet][def.attack].frames / 12;
          m.hitDone = false;
        } else {
          hurtPlayer(def.dmg);
        }
      }
      if (m.defT > 0) { vx *= 0.2; vy *= 0.2; }
      m.x += vx * dt;
      m.y += vy * dt;
      let anim = def.move;
      if (m.atkT > 0 && def.attack) anim = def.attack;
      else if (m.defT > 0 && m.type === 'skeleton') anim = 'defense';
      if (anim !== m.anim) { m.anim = anim; m.animT = 0; }
    }
    // 互相推开
    for (let i = 0; i < monsters.length; i++) {
      const a = monsters[i];
      if (a.dead) continue;
      for (let j = i + 1; j < monsters.length; j++) {
        const b = monsters[j];
        if (b.dead) continue;
        const dx = b.x - a.x, dy = b.y - a.y, d = Math.hypot(dx, dy);
        if (d < 13 && d > 0.01) {
          const push = (13 - d) * 0.5;
          a.x -= (dx / d) * push; a.y -= (dy / d) * push;
          b.x += (dx / d) * push; b.y += (dy / d) * push;
        }
      }
    }
    const far = Math.hypot(VW, VH) * 0.9;
    monsters = monsters.filter((m) => {
      if (m.dead) return m.deadT < SPR[m.def.sheet][m.def.death].frames / 10 + 0.6;
      return Math.hypot(m.x - P.x, m.y - P.y) < far;
    });
  }

  function updateBolts(dt) {
    for (const b of bolts) {
      b.trail.push(b.x, b.y);
      if (b.trail.length > 12) b.trail.splice(0, 2);
      b.x += b.vx * dt;
      b.y += b.vy * dt;
      b.life -= dt;
      if (b.enemy) {
        if (Math.hypot(b.x - P.x, b.y - P.y) < 8) {
          hurtPlayer(b.dmg + ((Math.random() * 2) | 0));
          b.life = 0;
        }
        continue;
      }
      for (const m of monsters) {
        if (m.dead || b.hit.has(m)) continue;
        if (Math.hypot(b.x - m.x, b.y - m.y + 4) < b.r) {
          hurtMonster(m, b.dmg, b.kind === 'qi' ? 12 : 5, b.x - b.vx, b.y - b.vy);
          b.hit.add(m);
          if (!b.pierce) { b.life = 0; break; }
        }
      }
      if (b.life <= 0 && b.kind === 'mage') {
        for (let i = 0; i < 5; i++) burstPart(b.x, b.y - b.lift, '#c4a1ff', 50, 1.5);
      }
    }
    bolts = bolts.filter((b) => b.life > 0);
  }

  function updateDrones(dt) {
    for (const d of drones) {
      d.t += dt;
      if (!d.target || d.target.dead) d.target = nearest(d.x, d.y, 220)[0];
      if (d.target) {
        const dx = d.target.x - d.x, dy = d.target.y - d.y, dist = Math.hypot(dx, dy) || 1;
        d.vx += ((dx / dist) * 150 - d.vx) * Math.min(1, 4 * dt);
        d.vy += ((dy / dist) * 150 - d.vy) * Math.min(1, 4 * dt);
        if (dist < 8) d.boom = true;
      } else {
        d.vx *= 0.96; d.vy *= 0.96;
      }
      d.x += d.vx * dt;
      d.y += d.vy * dt;
      if (d.t > 6) d.boom = true;
      if (d.boom) {
        fx.push({ kind: 'burst', x: d.x, y: d.y - 6, r: 26, t: 0, dur: 0.35 });
        for (let i = 0; i < 8; i++) burstPart(d.x, d.y - 6, pick(['#ffb347', '#ff6b3d', '#ffe08a']), 90);
        area(d.x, d.y, 26, 11, 10);
        shake = Math.max(shake, 1.5);
      }
    }
    drones = drones.filter((d) => !d.boom);
  }

  function updateMisc(dt) {
    for (const f of fx) {
      f.t += dt;
      if (f.kind === 'burial' && !f.fired && f.t > 0.8) {
        f.fired = true;
        area(f.x, f.y, f.r, 40, 18);
        shake = 5;
        for (let i = 0; i < 40; i++) burstPart(f.x + rand(-f.r, f.r) * 0.7, f.y + rand(-f.r, f.r) * 0.5, pick(['#c4a1ff', '#7c3aed', '#f5d0fe']), 110);
      }
    }
    fx = fx.filter((f) => f.t < f.dur);
    for (const p of parts) {
      p.t += dt;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.vy += 120 * dt;
      p.vx *= 0.96;
    }
    parts = parts.filter((p) => p.t < p.dur);
    for (const f of floats) f.t += dt;
    floats = floats.filter((f) => f.t < f.dur);
    for (const o of orbs) {
      o.t += dt;
      o.age += dt;
      const dx = P.x - o.x, dy = P.y - 8 - o.y, d = Math.hypot(dx, dy);
      if (o.age > 0.4 && d < 80) {
        o.v = Math.min(260, o.v + 400 * dt);
        o.x += (dx / d) * o.v * dt;
        o.y += (dy / d) * o.v * dt;
      }
      if (d < 6) {
        o.got = true;
        P.xp++;
        if (P.xp >= P.need) levelUp();
      }
    }
    orbs = orbs.filter((o) => !o.got && o.age < 25);
    shake *= Math.pow(0.02, dt);
  }

  function levelUp() {
    P.xp = 0;
    P.level++;
    P.need = 6 + P.level * 3;
    applyHeroStats();
    P.hp = P.maxHp;
    fx.push({ kind: 'pillar', x: P.x, y: P.y, t: 0, dur: 0.9, color: '#ffd166' });
    fx.push({ kind: 'ring', x: P.x, y: P.y, r: 40, t: 0, dur: 0.6, color: '#ffd166', w: 2 });
    say('LEVEL UP', '#ffd166', 9, 1.2);
  }

  function update(dt) {
    time += dt;
    updatePlayer(dt);
    const alive = monsters.reduce((n, m) => n + (m.dead ? 0 : 1), 0);
    const want = Math.min(26, 9 + time * 0.4);
    if (alive < want) spawnMonster(false);
    updateMonsters(dt);
    updateBolts(dt);
    updateDrones(dt);
    updateMisc(dt);
    cam.x += (P.x - VW / 2 - cam.x) * Math.min(1, dt * 3);
    cam.y += (P.y - VH * 0.74 - cam.y) * Math.min(1, dt * 3);
  }

  /* ---------- 绘制 ---------- */

  function shadow(x, y, rx, ry, a = 0.35) {
    ctx.fillStyle = `rgba(10,14,8,${a})`;
    ctx.beginPath();
    ctx.ellipse(x, y, rx, ry, 0, 0, TAU);
    ctx.fill();
  }

  function drawPlayer() {
    const h = hero();
    const s = SPR[h][P.anim];
    const fps = P.anim === 'attack' ? 18 : P.anim === 'run' ? 12 : P.anim === 'hurt' ? 18 : 8;
    const loopAnim = P.anim !== 'hurt' && !(P.anim === 'attack' && h !== 'robot');
    const frame = frameAt(s, P.animT, fps, loopAnim);
    shadow(P.x, P.y, 9, 3.5);
    if (P.berserkT > 0) {
      ctx.fillStyle = `rgba(255,60,40,${0.18 + Math.sin(time * 20) * 0.08})`;
      ctx.beginPath();
      ctx.ellipse(P.x, P.y - 12, 14, 18, 0, 0, TAU);
      ctx.fill();
    }
    drawSprite(ctx, s, frame, P.x, P.y, {
      dir: s.dirs >= 4 ? facingDir(P.fx, P.fy) : 0,
      flip: s.dirs < 4 && P.fx < 0,
      tint: P.hurtT > 0.38 ? 'rgb(255,80,60)' : null,
    });
    ctx.font = `700 5px ${FONT}`;
    ctx.textAlign = 'center';
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = 'rgba(0,0,0,.75)';
    ctx.strokeText('玩家', P.x, P.y - (h === 'robot' ? 44 : 38));
    ctx.fillStyle = '#e8f6ff';
    ctx.fillText('玩家', P.x, P.y - (h === 'robot' ? 44 : 38));
  }

  function drawMonster(m) {
    const def = m.def;
    const s = SPR[def.sheet][m.anim];
    const loopAnim = !m.dead && !(m.atkT > 0);
    const frame = frameAt(s, m.animT, m.atkT > 0 ? 12 : 10, loopAnim);
    const lift = def.lift || 0;
    let alpha = 1;
    if (m.dead) alpha = clamp(1 - (m.deadT - s.frames / 10) / 0.6, 0, 1);
    shadow(m.x, m.y, lift ? 6 : 8 * def.scale, lift ? 2.5 : 3 * def.scale, 0.3 * alpha);
    let tint = def.tint || null;
    if (m.hurtT > 0.1) tint = 'rgb(255,220,80)';
    else if (m.hurtT > 0) tint = 'white';
    drawSprite(ctx, s, frame, m.x, m.y, {
      scale: def.scale, lift, alpha, tint,
      dir: s.dirs >= 4 ? facingDir(m.fx, m.fy) : 0,
      flip: s.dirs < 4 && m.flip,
    });
    if (!m.dead && m.hp < m.maxHp) {
      const y = m.y - 30 * def.scale - lift;
      ctx.fillStyle = 'rgba(10,6,6,.85)';
      ctx.fillRect(m.x - 9, y, 18, 3);
      ctx.fillStyle = '#e8352b';
      ctx.fillRect(m.x - 8.5, y + 0.5, 17 * clamp(m.hp / m.maxHp, 0, 1), 2);
      if (m.shield > 0) {
        ctx.fillStyle = '#96c4e6';
        ctx.fillRect(m.x - 8.5, y - 1.5, 17 * clamp(m.shield / (def.shield || 1), 0, 1), 1);
      }
    }
  }

  function drawProp(p) {
    shadow(p.x + 8, p.y + 10, 7, 4, p.rock ? 0.5 : 0.45);
    ctx.drawImage(TILES, p.rock ? 112 : 64, 48, 32, 32, p.x - 8, p.y - 14, 32, 32);
  }

  function drawBolt(b) {
    const y = b.y - b.lift;
    if (b.kind === 'qi') {
      const a = Math.atan2(b.vy, b.vx);
      const alpha = clamp(b.life / 0.2, 0, 1);
      ctx.save();
      ctx.translate(b.x, y);
      ctx.rotate(a);
      ctx.globalAlpha = alpha;
      ctx.strokeStyle = '#bfe0ff';
      ctx.lineWidth = 3;
      ctx.shadowColor = '#8fb8ff';
      ctx.shadowBlur = 8;
      ctx.beginPath();
      ctx.arc(-6, 0, 12, -1.1, 1.1);
      ctx.stroke();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 1.2;
      ctx.beginPath();
      ctx.arc(-6, 0, 12, -0.9, 0.9);
      ctx.stroke();
      ctx.restore();
      return;
    }
    const color = { mage: '#c4a1ff', robot: '#ffe08a', scatter: '#ffe08a', enemy: '#ff5a4e', curse: '#d946ef' }[b.kind];
    ctx.strokeStyle = color;
    ctx.globalAlpha = 0.5;
    ctx.lineWidth = b.kind === 'mage' || b.kind === 'curse' ? 3 : 1.5;
    ctx.lineCap = 'round';
    ctx.beginPath();
    for (let i = 0; i < b.trail.length; i += 2) {
      const ty = b.trail[i + 1] - b.lift;
      if (i === 0) ctx.moveTo(b.trail[i], ty);
      else ctx.lineTo(b.trail[i], ty);
    }
    ctx.lineTo(b.x, y);
    ctx.stroke();
    ctx.globalAlpha = 1;
    ctx.fillStyle = color;
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.beginPath();
    ctx.arc(b.x, y, b.kind === 'mage' || b.kind === 'curse' ? 3 : 1.6, 0, TAU);
    ctx.fill();
    ctx.shadowBlur = 0;
  }

  function drawGroundFx(f) {
    const p = f.t / f.dur;
    if (f.kind === 'burial') {
      const a = p < 0.15 ? p / 0.15 : p > 0.75 ? (1 - p) / 0.25 : 1;
      ctx.save();
      ctx.translate(f.x, f.y);
      ctx.scale(1, 0.55);
      ctx.globalAlpha = a;
      ctx.shadowColor = '#a855f7';
      ctx.shadowBlur = 10;
      ctx.strokeStyle = '#c084fc';
      ctx.lineWidth = 2;
      for (const r of [f.r, f.r * 0.78, f.r * 0.4]) {
        ctx.beginPath();
        ctx.arc(0, 0, r, 0, TAU);
        ctx.stroke();
      }
      ctx.rotate(f.t * 1.4);
      ctx.beginPath();
      for (let i = 0; i < 6; i++) {
        const a1 = (i / 6) * TAU, a2 = a1 + (TAU * 2) / 6;
        ctx.moveTo(Math.cos(a1) * f.r * 0.78, Math.sin(a1) * f.r * 0.78);
        ctx.lineTo(Math.cos(a2) * f.r * 0.78, Math.sin(a2) * f.r * 0.78);
      }
      ctx.stroke();
      ctx.fillStyle = '#f5d0fe';
      for (let i = 0; i < 16; i++) {
        const an = (i / 16) * TAU - f.t * 2.2;
        ctx.fillRect(Math.cos(an) * f.r * 0.89 - 2, Math.sin(an) * f.r * 0.89 - 2, 4, 4);
      }
      if (f.fired) {
        ctx.globalAlpha = a * 0.35;
        ctx.fillStyle = '#7c3aed';
        ctx.beginPath();
        ctx.arc(0, 0, f.r, 0, TAU);
        ctx.fill();
      }
      ctx.restore();
    } else if (f.kind === 'heal') {
      ctx.strokeStyle = `rgba(126,224,126,${1 - p})`;
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.ellipse(f.x, f.y, 10 + p * 16, (10 + p * 16) * 0.45, 0, 0, TAU);
      ctx.stroke();
    }
  }

  function drawTopFx(f) {
    const p = f.t / f.dur;
    if (f.kind === 'ring') {
      const r = f.r * (1 - Math.pow(1 - p, 3));
      ctx.save();
      ctx.translate(f.x, f.y - 8);
      ctx.scale(1, 0.6);
      ctx.globalAlpha = 1 - p;
      ctx.strokeStyle = f.color;
      ctx.lineWidth = f.w || 2;
      ctx.shadowColor = f.color;
      ctx.shadowBlur = 10;
      ctx.beginPath();
      ctx.arc(0, 0, r, 0, TAU);
      ctx.stroke();
      if (f.fill) {
        ctx.globalAlpha = (1 - p) * 0.25;
        ctx.fillStyle = f.color;
        ctx.fill();
      }
      ctx.restore();
    } else if (f.kind === 'spin') {
      ctx.save();
      ctx.translate(f.x, f.y - 10);
      ctx.scale(1, 0.6);
      ctx.globalAlpha = 1 - p;
      ctx.strokeStyle = f.color;
      ctx.lineWidth = 5 * (1 - p) + 1;
      ctx.shadowColor = '#ffb37a';
      ctx.shadowBlur = 12;
      const start = p * TAU * 1.5;
      ctx.beginPath();
      ctx.arc(0, 0, f.r * (0.6 + 0.4 * p), start, start + Math.PI * 1.4);
      ctx.stroke();
      ctx.restore();
    } else if (f.kind === 'slash') {
      ctx.save();
      ctx.translate(f.x, f.y - 12);
      ctx.rotate(f.ang);
      ctx.globalAlpha = 1 - p;
      ctx.strokeStyle = f.color;
      ctx.lineWidth = 4 * (1 - p) + 1;
      ctx.shadowColor = f.color;
      ctx.shadowBlur = 8;
      ctx.beginPath();
      ctx.arc(0, 0, f.r * (0.75 + 0.25 * p), -1.0 + p * 0.6, 1.0 + p * 0.6);
      ctx.stroke();
      ctx.restore();
    } else if (f.kind === 'burst') {
      ctx.globalAlpha = 1 - p;
      ctx.fillStyle = '#ff9a3c';
      ctx.beginPath();
      ctx.arc(f.x, f.y, f.r * Math.sqrt(p), 0, TAU);
      ctx.fill();
      ctx.fillStyle = '#fff4c2';
      ctx.beginPath();
      ctx.arc(f.x, f.y, f.r * 0.5 * Math.sqrt(p) * (1 - p), 0, TAU);
      ctx.fill();
      ctx.globalAlpha = 1;
    } else if (f.kind === 'pillar') {
      const g = ctx.createLinearGradient(0, f.y - 90, 0, f.y);
      g.addColorStop(0, 'rgba(255,255,255,0)');
      g.addColorStop(1, f.color);
      ctx.globalAlpha = (1 - p) * 0.6;
      ctx.fillStyle = g;
      const w = 14 * (1 - p * 0.5);
      ctx.fillRect(f.x - w / 2, f.y - 90, w, 90);
      ctx.globalAlpha = 1;
    }
  }

  function draw() {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.imageSmoothingEnabled = false;
    const sx = (Math.random() - 0.5) * shake;
    const sy = (Math.random() - 0.5) * shake;
    const cx = Math.round((cam.x + sx) * S) / S;
    const cy = Math.round((cam.y + sy) * S) / S;
    ctx.setTransform(dpr * S, 0, 0, dpr * S, -cx * dpr * S, -cy * dpr * S);
    ctx.fillStyle = pattern;
    ctx.fillRect(cx, cy, VW + 1, VH + 1);

    for (const f of fx) if (f.kind === 'burial' || f.kind === 'heal') drawGroundFx(f);

    for (const o of orbs) {
      const bob = Math.sin(o.t * 5) * 1.2;
      ctx.fillStyle = 'rgba(94,234,212,.25)';
      ctx.beginPath();
      ctx.arc(o.x, o.y - 3 + bob, 3.5, 0, TAU);
      ctx.fill();
      ctx.fillStyle = '#a7fff0';
      ctx.fillRect(o.x - 1, o.y - 4 + bob, 2, 2);
    }

    const list = [];
    const tx0 = Math.floor(cx / 16) - 2, tx1 = Math.ceil((cx + VW) / 16) + 2;
    const ty0 = Math.floor(cy / 16) - 2, ty1 = Math.ceil((cy + VH) / 16) + 2;
    for (let ty = ty0; ty <= ty1; ty++) {
      for (let tx = tx0; tx <= tx1; tx++) {
        const hv = hash(tx + 911, ty - 377) % 83;
        if (hv < 2) list.push({ y: ty * 16 + 12, draw: drawProp, arg: { x: tx * 16, y: ty * 16, rock: hv === 0 } });
      }
    }
    for (const m of monsters) list.push({ y: m.y - (m.dead ? 20 : 0), draw: drawMonster, arg: m });
    list.push({ y: P.y, draw: drawPlayer, arg: null });
    list.sort((a, b) => a.y - b.y);
    for (const it of list) it.draw(it.arg);

    for (const d of drones) {
      shadow(d.x, d.y, 4, 1.5, 0.3);
      ctx.drawImage(DRONE, (Math.floor(d.t * 14) % 4) * 16, 0, 16, 16, d.x - 8, d.y - 18, 16, 16);
    }
    for (const b of bolts) drawBolt(b);
    for (const f of fx) if (f.kind !== 'burial' && f.kind !== 'heal') drawTopFx(f);
    for (const p of parts) {
      ctx.globalAlpha = 1 - p.t / p.dur;
      ctx.fillStyle = p.color;
      ctx.fillRect(p.x - p.size / 2, p.y - p.size / 2, p.size, p.size);
    }
    ctx.globalAlpha = 1;

    ctx.textAlign = 'center';
    for (const f of floats) {
      const p = f.t / f.dur;
      const pop = f.crit || f.big ? 1 + Math.max(0, 0.25 - f.t) * 2 : 1;
      ctx.globalAlpha = p > 0.7 ? (1 - p) / 0.3 : 1;
      ctx.font = `900 ${f.size * pop}px ${FONT}`;
      ctx.lineWidth = 2;
      ctx.strokeStyle = 'rgba(0,0,0,.8)';
      const y = f.y - (f.big ? p * 10 : p * 16);
      const text = f.crit ? f.text + '!' : f.text;
      ctx.strokeText(text, f.x, y);
      ctx.fillStyle = f.color;
      ctx.fillText(text, f.x, y);
    }
    ctx.globalAlpha = 1;
  }

  function updateHud(dt) {
    hudT -= dt;
    if (hudT > 0) return;
    hudT = 0.1;
    hud.hp.style.width = (100 * P.hp / P.maxHp).toFixed(1) + '%';
    hud.mp.style.width = (100 * P.mp / P.maxMp).toFixed(1) + '%';
    hud.xp.style.width = (100 * P.xp / P.need).toFixed(1) + '%';
    hud.lv.textContent = 'Lv ' + P.level;
    const sec = Math.floor(time);
    hud.time.textContent = String(Math.floor(sec / 60)).padStart(2, '0') + ':' + String(sec % 60).padStart(2, '0');
    hud.kills.textContent = kills;
  }

  resize();
  window.addEventListener('resize', resize);
  cam.x = P.x - VW / 2;
  cam.y = P.y - VH * 0.74;
  for (let i = 0; i < 8; i++) spawnMonster(true);

  return (dt) => {
    // 切回标签页时 dt 被限制过，这里再分步，避免大步长穿透
    const steps = Math.ceil(dt / 0.02);
    for (let i = 0; i < steps; i++) update(dt / steps);
    draw();
    updateHud(dt);
  };
}

/* ================= 职业 ================= */

const SKILLS = {
  spin: ['回旋斩', '攻击身边 42 距离内敌人。基础伤害 16，破韧 12。消耗 16 MP，冷却 2.8 秒。'],
  qi: ['剑气', '向前方斩出月牙剑气，距离 112。可叠层，最高 3 层；释放无间隔。每层冷却 1.5 秒。消耗 10 MP。'],
  thrust: ['突刺', '朝面向突进并伤害前方。可叠层，最高 3 层；释放无间隔。每层冷却 1.0 秒。消耗 8 MP。'],
  berserk: ['狂化', '攻击力翻倍，攻速增加 100%，持续 3 秒。消耗 18 MP，冷却 8 秒。'],
  bolt: ['飞弹', '朝鼠标方向射出法弹。基础伤害 20。消耗 14 MP，冷却 1.6 秒。'],
  nova: ['新星', '攻击身边 64 距离内敌人。基础伤害 12，破韧 8。消耗 22 MP，冷却 4.2 秒。'],
  flight: ['飞行', '开关飞行。优先消耗 STA，再消耗 MP。耗尽后落地。'],
  burial: ['万葬', '以自身为中心大范围法阵，重创范围内敌人。消耗 40 MP，冷却 9 秒。释放时还有专属配音。'],
  mirror: ['逆反之盾', '吸收伤害并把每次伤害的 5% 反弹给攻击者。持续 10 秒，单次过高或累计达上限也会破碎。上限随等级提高。消耗 24 MP，冷却 15 秒。'],
  heal: ['治疗术', '回复最大生命值的 15%。消耗 22 MP，冷却 20 秒。'],
  scatter: ['散射', '朝面向扇形射出 5 发子弹，每发基础伤害 9。消耗 14 MP，冷却 2.4 秒。'],
  missile: ['爆破弹', '射出榴弹，命中、撞墙或飞到尽头时爆炸，半径 44 内基础伤害 26，破韧 16。消耗 20 MP，冷却 4.5 秒。'],
  boost: ['推进', '朝面向喷射突进，起步冲击波伤害身边 36 距离内敌人，基础伤害 10。突进中无敌。消耗 12 MP，冷却 3 秒。'],
  overload: ['过载', '持续 10 秒：普攻与技能伤害 +40%，移速 +25%，射速 +60%，但其他技能耗蓝 +50%。消耗 18 MP，冷却 20 秒。'],
  swarm: ['蜂群', '身边召唤 6 架小型无人机，成群撞向离自己最近的怪物并爆炸，半径 26 内基础伤害 11 并击退。无人机会被怪物子弹打爆，12 秒后自毁。消耗 22 MP，冷却 7 秒。'],
  field: ['磁力场', '展开磁力场 6 秒：受到的伤害只剩 5%，每 0.4 秒对贴身（半径 30）的怪物造成基础伤害 8 并轻微推开。消耗 20 MP，冷却 10 秒。'],
  medkit: ['战术医疗包', '3 秒内持续回复 24% 最大生命。消耗 16 MP，冷却 16 秒。'],
  jetpack: ['喷气背包', '开关喷气背包，离地飞行，可越过岩石和灌木。每秒消耗 22，优先扣 STA，再扣 MP，耗尽后落地；飞行中 STA 不回复。'],
  melee: ['肘击', '按下技能键用肘部撞击一次：身前 34 距离扇形内基础伤害 11，能劈掉敌方飞弹，击败克苏鲁之眼后还能劈开迷宫墙。消耗 8 STA，冷却 0.9 秒。不占用普攻，点按左键仍是点射。'],
};

const HEROES = [
  {
    key: 'warrior', name: '战士', role: 'WARRIOR · 近战', accent: '#ff6b4a', portrait: 'img/warrior.png',
    desc: '血量与护甲最高的前排。轻击近战劈砍，还能劈掉敌方飞弹；按住左键放出小剑气（距离 56，基础伤害 24）。',
    stats: { HP: 120, ARM: 14, MP: 60, CRT: 12 }, pool: '技能四选三 · 装配到 R / F / C',
    skills: ['spin', 'qi', 'thrust', 'berserk'],
  },
  {
    key: 'sword', name: '女剑客', role: 'SWORDSWOMAN · 近战', accent: '#8fb8ff', portrait: 'img/sword.png',
    desc: '以速度与暴击见长：移速 +16，暴击率 22%。同样能劈砍飞弹、重击放小剑气，靠身法在怪潮中穿梭。',
    stats: { HP: 95, ARM: 8, MP: 85, CRT: 22 }, pool: '技能四选三 · 装配到 R / F / C',
    skills: ['spin', 'qi', 'thrust', 'berserk'],
  },
  {
    key: 'mage', name: '女魔法师', role: 'MAGE · 远程', accent: '#b98cff', portrait: 'img/mage.png',
    desc: '身板最脆，魔力最足。轻击发射飞弹，重击是穿透激光；闪避换成闪现，并多出一个技能键 V。',
    stats: { HP: 78, ARM: 4, MP: 130, CRT: 12 }, pool: '四键自选 · R / F / C / V',
    skills: ['bolt', 'nova', 'flight', 'burial', 'mirror', 'heal'],
  },
  {
    key: 'robot', name: '机甲人', role: 'MECH · 远程', accent: '#5eead4', portrait: null,
    desc: '8 方向像素角色。普攻为快速点射，弹匣 30 发；没有重击，长按左键即换弹补满。技能覆盖火力、机动、近身与生存。',
    stats: { HP: 105, ARM: 12, MP: 90, CRT: 12 }, pool: '技能九选三 · 装配到 R / F / C',
    skills: ['scatter', 'missile', 'boost', 'overload', 'swarm', 'field', 'medkit', 'jetpack', 'melee'],
  },
];
const STAT_MAX = { HP: 130, ARM: 15, MP: 140, CRT: 25 };
const ANIM_CN = { idle: '待机', run: '奔跑', attack: '攻击', hurt: '受击', death: '倒下' };

function createHeroShowcase() {
  const tabs = $('#heroTabs');
  const panel = $('#heroPanel');
  const art = panel.querySelector('.hp-art');
  const portrait = $('#heroPortrait');
  const stage = $('#heroStage');
  const sctx = stage.getContext('2d');
  const holo = $('#heroHolo');
  const hctx = holo.getContext('2d');
  const animBar = $('#animButtons');
  const ground = grassCanvas(4, 4, 4, 3);
  const tabCanvases = [];
  let cur = 0, anim = 'idle', animT = 0, dirT = 0, dir = 0, locked = null;
  const AUTO = [['idle', 2.2], ['run', 2.4], ['attack', 0], ['attack', 0], ['idle', 1.2], ['hurt', 0], ['run', 1.6]];
  let autoIdx = 0, autoT = 0;

  HEROES.forEach((h, i) => {
    const b = document.createElement('button');
    b.className = 'hero-tab';
    b.setAttribute('role', 'tab');
    b.style.setProperty('--accent', h.accent);
    const c = document.createElement('canvas');
    c.width = c.height = 48;
    tabCanvases.push(c);
    b.append(c, document.createTextNode(h.name));
    b.addEventListener('click', () => select(i));
    tabs.append(b);
  });

  for (const a of ['idle', 'run', 'attack', 'hurt', 'death']) {
    const b = document.createElement('button');
    b.textContent = ANIM_CN[a];
    b.dataset.anim = a;
    b.addEventListener('click', () => {
      locked = locked === a ? null : a;
      setAnim(locked || 'idle');
      autoIdx = 0; autoT = 0;
      refreshAnimButtons();
    });
    animBar.append(b);
  }
  const autoBtn = document.createElement('button');
  autoBtn.textContent = '自动';
  autoBtn.addEventListener('click', () => { locked = null; refreshAnimButtons(); });
  animBar.append(autoBtn);

  function refreshAnimButtons() {
    for (const b of animBar.children) {
      b.classList.toggle('on', b === autoBtn ? !locked : b.dataset.anim === locked);
    }
  }

  function setAnim(a) { anim = a; animT = 0; }

  function select(i) {
    cur = i;
    const h = HEROES[i];
    panel.style.setProperty('--accent', h.accent);
    [...tabs.children].forEach((b, j) => b.setAttribute('aria-selected', String(j === i)));
    art.classList.toggle('robot', !h.portrait);
    if (h.portrait) {
      portrait.src = h.portrait;
      portrait.alt = h.name + ' 立绘';
      portrait.classList.remove('swap');
      void portrait.offsetWidth;
      portrait.classList.add('swap');
    }
    $('#heroRole').textContent = h.role;
    $('#heroName').textContent = h.name;
    $('#heroDesc').textContent = h.desc;
    $('#poolTitle').textContent = h.pool;
    const stats = $('#heroStats');
    stats.innerHTML = '';
    for (const [k, v] of Object.entries(h.stats)) {
      stats.insertAdjacentHTML('beforeend', `<span>${k}</span><div class="stat-bar"><i></i></div><em>${v}${k === 'CRT' ? '%' : ''}</em>`);
    }
    requestAnimationFrame(() => {
      const bars = stats.querySelectorAll('i');
      Object.entries(h.stats).forEach(([k, v], j) => { bars[j].style.width = (100 * v / STAT_MAX[k]) + '%'; });
    });
    const skills = $('#heroSkills');
    skills.innerHTML = '';
    h.skills.forEach((id, j) => {
      const b = document.createElement('button');
      b.className = 'skill';
      b.textContent = SKILLS[id][0];
      const show = () => {
        for (const x of skills.children) x.classList.toggle('on', x === b);
        $('#skillDetail').innerHTML = `<b>${SKILLS[id][0]}</b>：${SKILLS[id][1]}`;
      };
      b.addEventListener('mouseenter', show);
      b.addEventListener('click', show);
      skills.append(b);
      if (j === 0) show();
    });
    locked = null;
    autoIdx = 0; autoT = 0;
    setAnim('idle');
    refreshAnimButtons();
  }

  function drawTabs(t) {
    HEROES.forEach((h, i) => {
      const s = SPR[h.key].idle;
      const g = tabCanvases[i].getContext('2d');
      g.imageSmoothingEnabled = false;
      g.clearRect(0, 0, 48, 48);
      const f = frameAt(s, t, 8, true);
      if (s.size === 96) g.drawImage(s.img, f * 96 + 22, 26, 52, 52, 0, 0, 48, 48);
      else g.drawImage(s.img, f * 64 + 12, 8, 40, 40, 0, 0, 48, 48);
    });
  }

  function tick(dt, t) {
    const h = HEROES[cur];
    const set = SPR[h.key];
    animT += dt;
    dirT += dt;
    if (!locked) {
      const [a, dur] = AUTO[autoIdx];
      if (anim !== a) setAnim(a);
      autoT += dt;
      const s = set[a];
      const len = dur || s.frames / (a === 'attack' || a === 'hurt' ? 14 : 10) + 0.15;
      if (autoT > len) { autoT = 0; autoIdx = (autoIdx + 1) % AUTO.length; setAnim(AUTO[autoIdx][0]); }
    } else if (locked === 'death' || locked === 'hurt' || locked === 'attack') {
      const s = set[locked];
      if (animT > s.frames / (locked === 'death' ? 10 : 14) + 0.9) animT = 0;
    }
    if (dirT > 1.1) { dirT = 0; dir = (dir + 1) % 8; }

    sctx.imageSmoothingEnabled = false;
    sctx.drawImage(ground, 0, 0);
    const vg = sctx.createRadialGradient(128, 150, 40, 128, 128, 190);
    vg.addColorStop(0, 'rgba(0,0,0,0)');
    vg.addColorStop(1, 'rgba(0,0,0,.55)');
    sctx.fillStyle = vg;
    sctx.fillRect(0, 0, 256, 256);
    sctx.fillStyle = 'rgba(10,14,8,.4)';
    sctx.beginPath();
    sctx.ellipse(128, 202, 30, 10, 0, 0, TAU);
    sctx.fill();
    const s = set[anim];
    const fps = anim === 'attack' || anim === 'hurt' ? 14 : anim === 'run' ? 12 : 8;
    const frame = frameAt(s, animT, fps, anim === 'idle' || anim === 'run');
    const robotDir = anim === 'death' || anim === 'hurt' ? 1 : dir;
    drawSprite(sctx, s, frame, 128, 202, { scale: s.size === 96 ? 2.5 : 3, dir: s.dirs >= 4 ? robotDir : 0 });

    drawTabs(t);

    if (!h.portrait) {
      const rs = SPR.robot.idle;
      hctx.clearRect(0, 0, 192, 192);
      hctx.imageSmoothingEnabled = false;
      hctx.globalAlpha = 0.85 + Math.sin(t * 30) * 0.05 * (Math.sin(t * 1.7) > 0.9 ? 4 : 1);
      drawSprite(hctx, rs, frameAt(rs, t, 8, true), 96, 186, { scale: 2.6, dir: Math.floor(t / 0.7) % 8, tint: 'rgb(94,234,212)' });
      hctx.globalAlpha = 1;
      hctx.globalCompositeOperation = 'destination-out';
      hctx.fillStyle = 'rgba(0,0,0,.35)';
      for (let y = (t * 20) % 4; y < 192; y += 4) hctx.fillRect(0, y, 192, 1.5);
      hctx.globalCompositeOperation = 'source-over';
    }
  }

  select(0);
  addTicker(panel, tick);
  addTicker(tabs, (dt, t) => { if (!tickers.find((x) => x.el === panel).on) drawTabs(t); });
}

/* ================= 图鉴 ================= */

const MONSTER_CARDS = [
  { key: 'slime', sheet: 'slime', name: '史莱姆', tag: '开局', desc: '成群蠕动的黏液，靠数量把你淹没。', loop: 'walk', acts: ['death'], scale: 4 },
  { key: 'skeleton', sheet: 'skeleton', name: '骷髅', tag: '15 秒后', desc: '持盾骷髅兵，举盾时伤害只吃四成。', loop: 'walk', acts: ['attack', 'defense', 'hurt', 'death'], scale: 3 },
  { key: 'mushroom', sheet: 'mushroom', name: '蘑菇', tag: '25 秒后', desc: '体型更大，蓄力后猛地扑向你。', loop: 'idle', acts: ['jump', 'attack', 'death'], scale: 4 },
  { key: 'flyer', sheet: 'flyer', name: '飞行怪', tag: '空中', desc: '在低空盘旋，被击退时能越过障碍。', loop: 'fly', acts: ['attack', 'hurt', 'death'], scale: 3 },
  { key: 'caster', sheet: 'mushroom', name: '施法怪', tag: '20 秒后 · 带护盾', desc: '紫色的蘑菇术士，保持距离放出法弹。', loop: 'idle', acts: ['jump', 'death'], scale: 4, tint: 'rgb(88,42,112)' },
  { key: 'killbot', sheet: 'killbot', name: '机器人小兵', tag: '30 秒后', desc: '8 方向行动，绕着你转圈并点射。', loop: 'walk', acts: ['attack', 'death'], scale: 4, dirs: true },
  { key: 'elite', sheet: 'skeleton', name: '精英怪物', tag: '被诅咒后', desc: '诅咒「存在被克苏鲁余光注意！」召来的精英，种类随机：血量翻倍、韧性更高，击杀积分也是双倍。', loop: 'walk', acts: ['attack', 'hurt', 'death'], scale: 3, tint: 'rgb(132,62,196)' },
];

function createBestiary() {
  const grid = $('#monsterGrid');
  const cards = MONSTER_CARDS.map((m) => {
    const el = document.createElement('article');
    el.className = 'monster reveal';
    el.innerHTML = `<span class="tag">${m.tag}</span><canvas width="168" height="168"></canvas><h3>${m.name}</h3><p>${m.desc}</p>`;
    grid.append(el);
    const card = { m, el, ctx: el.querySelector('canvas').getContext('2d'), anim: m.loop, t: Math.random(), queue: [], dir: 0, dirT: 0 };
    const play = () => {
      if (card.queue.length) return;
      card.queue = [...m.acts];
      card.anim = card.queue[0];
      card.t = 0;
      el.classList.add('active');
    };
    el.addEventListener('mouseenter', play);
    el.addEventListener('click', play);
    return card;
  });

  addTicker(grid, (dt) => {
    for (const c of cards) {
      const set = SPR[c.m.sheet];
      c.t += dt;
      c.dirT += dt;
      if (c.dirT > 0.9) { c.dirT = 0; c.dir = (c.dir + 1) % 8; }
      let s = set[c.anim];
      if (c.queue.length) {
        const hold = c.anim === 'death' ? 0.7 : c.anim === 'attack' && s.frames === 1 ? 0.5 : 0.05;
        if (c.t > s.frames / 10 + hold) {
          c.queue.shift();
          c.t = 0;
          c.anim = c.queue[0] || c.m.loop;
          if (!c.queue.length) c.el.classList.remove('active');
          s = set[c.anim];
        }
      }
      const g = c.ctx;
      g.imageSmoothingEnabled = false;
      g.clearRect(0, 0, 168, 168);
      g.fillStyle = 'rgba(0,0,0,.35)';
      g.beginPath();
      g.ellipse(84, 140, 26, 7, 0, 0, TAU);
      g.fill();
      const frame = frameAt(s, c.t, 10, !c.queue.length);
      drawSprite(g, s, frame, 84, 140, {
        scale: c.m.scale,
        dir: s.dirs >= 4 ? c.dir : 0,
        tint: c.m.tint,
        lift: c.m.key === 'flyer' ? 14 + Math.sin(c.t * 4) * 3 : 0,
      });
    }
  });
}

/* ================= 迷宫遗迹 ================= */

function createMaze(canvas) {
  const ctx = canvas.getContext('2d');
  const caption = $('#mazeCaption');
  const C = 10, N = C * 2 + 1, T = canvas.width / N;
  const PL0 = 4, PL1 = 5;
  let grid, ops, opIdx, phase, phaseT, path, entrance, eyeLook = { x: 0, y: 0, tx: 0, ty: 0, t: 0 };

  function build() {
    grid = new Uint8Array(N * N).fill(1);
    ops = [];
    const visited = new Uint8Array(C * C);
    for (let cy = PL0; cy <= PL1; cy++) for (let cx = PL0; cx <= PL1; cx++) visited[cy * C + cx] = 1;
    // 广场本身先打开
    for (let y = PL0 * 2 + 1; y <= PL1 * 2 + 1; y++) for (let x = PL0 * 2 + 1; x <= PL1 * 2 + 1; x++) ops.push(y * N + x);
    // 广场只开一个门：从门外的格子开始生成树，保证进广场只有一条路
    const doors = [[PL0 - 1, PL0 + (Math.random() * 2 | 0), 1, 0], [PL1 + 1, PL0 + (Math.random() * 2 | 0), -1, 0],
      [PL0 + (Math.random() * 2 | 0), PL0 - 1, 0, 1], [PL0 + (Math.random() * 2 | 0), PL1 + 1, 0, -1]];
    const [dx0, dy0, ddx, ddy] = pick(doors);
    ops.push((dy0 * 2 + 1 + ddy) * N + (dx0 * 2 + 1 + ddx));
    const stack = [[dx0, dy0]];
    visited[dy0 * C + dx0] = 1;
    ops.push((dy0 * 2 + 1) * N + dx0 * 2 + 1);
    while (stack.length) {
      const [cx, cy] = stack[stack.length - 1];
      const nbrs = [[1, 0], [-1, 0], [0, 1], [0, -1]]
        .map(([ox, oy]) => [cx + ox, cy + oy, ox, oy])
        .filter(([nx, ny]) => nx >= 0 && ny >= 0 && nx < C && ny < C && !visited[ny * C + nx]);
      if (!nbrs.length) { stack.pop(); continue; }
      const [nx, ny, ox, oy] = pick(nbrs);
      visited[ny * C + nx] = 1;
      ops.push((cy * 2 + 1 + oy) * N + (cx * 2 + 1 + ox));
      ops.push((ny * 2 + 1) * N + (nx * 2 + 1));
      stack.push([nx, ny]);
    }
    // 外墙唯一入口
    const side = (Math.random() * 4) | 0;
    const k = (Math.random() * C) | 0;
    const pos = [[k * 2 + 1, 0], [k * 2 + 1, N - 1], [0, k * 2 + 1], [N - 1, k * 2 + 1]][side];
    entrance = pos;
    opIdx = 0;
    phase = 'gen';
    phaseT = 0;
    path = null;
    caption.textContent = '迷宫遗迹出现……';
  }

  function solve() {
    const start = entrance[1] * N + entrance[0];
    const prev = new Int32Array(N * N).fill(-1);
    prev[start] = start;
    const q = [start];
    const goal = (PL0 * 2 + 1 + 1) * N + (PL0 * 2 + 1 + 1);
    while (q.length) {
      const i = q.shift();
      if (i === goal) break;
      const x = i % N, y = (i / N) | 0;
      for (const [ox, oy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const nx = x + ox, ny = y + oy;
        if (nx < 0 || ny < 0 || nx >= N || ny >= N) continue;
        const j = ny * N + nx;
        if (grid[j] || prev[j] !== -1) continue;
        prev[j] = i;
        q.push(j);
      }
    }
    const out = [];
    for (let i = goal; i !== start; i = prev[i]) {
      if (i < 0) return [];
      out.push(i);
    }
    out.push(start);
    return out.reverse();
  }

  function drawEye(cx, cy, r, t, open) {
    const glow = ctx.createRadialGradient(cx, cy, r * 0.3, cx, cy, r * 2.4);
    glow.addColorStop(0, 'rgba(255,40,40,.45)');
    glow.addColorStop(1, 'rgba(255,40,40,0)');
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(cx, cy, r * 2.4, 0, TAU);
    ctx.fill();
    // 触须
    ctx.strokeStyle = '#7a1c1c';
    ctx.lineWidth = 3;
    for (let i = 0; i < 6; i++) {
      const a = Math.PI * 0.25 + (i / 5) * Math.PI * 0.5;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(a) * r * 0.8, cy + Math.sin(a) * r * 0.8);
      ctx.quadraticCurveTo(
        cx + Math.cos(a) * r * 1.4 + Math.sin(t * 3 + i) * 5, cy + Math.sin(a) * r * 1.4,
        cx + Math.cos(a) * r * 1.9 + Math.sin(t * 2 + i) * 7, cy + Math.sin(a) * r * 1.9);
      ctx.stroke();
    }
    ctx.save();
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, TAU);
    ctx.fillStyle = '#f3ede4';
    ctx.fill();
    ctx.clip();
    ctx.strokeStyle = 'rgba(200,30,30,.7)';
    ctx.lineWidth = 1.2;
    for (let i = 0; i < 9; i++) {
      const a = (i / 9) * TAU + 0.3;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r);
      ctx.lineTo(cx + Math.cos(a + 0.2) * r * 0.55, cy + Math.sin(a + 0.2) * r * 0.55);
      ctx.stroke();
    }
    const ix = cx + eyeLook.x * r * 0.35, iy = cy + eyeLook.y * r * 0.35;
    ctx.fillStyle = '#b3141b';
    ctx.beginPath();
    ctx.arc(ix, iy, r * 0.48, 0, TAU);
    ctx.fill();
    ctx.fillStyle = '#e8434a';
    ctx.beginPath();
    ctx.arc(ix, iy, r * 0.36, 0, TAU);
    ctx.fill();
    ctx.fillStyle = '#120608';
    ctx.beginPath();
    ctx.ellipse(ix, iy, r * 0.12, r * 0.28, 0, 0, TAU);
    ctx.fill();
    ctx.fillStyle = 'rgba(255,255,255,.8)';
    ctx.fillRect(ix - r * 0.2, iy - r * 0.25, r * 0.12, r * 0.12);
    // 眼睑
    if (open < 1) {
      ctx.fillStyle = '#3b0d0d';
      ctx.fillRect(cx - r, cy - r, r * 2, r * (1 - open));
      ctx.fillRect(cx - r, cy + r * open, r * 2, r * (1 - open));
    }
    ctx.restore();
    ctx.strokeStyle = '#4a0e0e';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, TAU);
    ctx.stroke();
  }

  function tick(dt, t) {
    phaseT += dt;
    if (phase === 'gen') {
      const perSec = ops.length / 2.6;
      const target = Math.min(ops.length, Math.floor(phaseT * perSec));
      while (opIdx < target) grid[ops[opIdx++]] = 0;
      if (opIdx >= ops.length) {
        grid[entrance[1] * N + entrance[0]] = 0;
        path = solve();
        phase = 'path';
        phaseT = 0;
        caption.innerHTML = '天赋「世界指引」：按 <kbd>G</kbd> 画出唯一通路';
      }
    } else if (phase === 'path' && phaseT > 2.4) {
      phase = 'eye'; phaseT = 0;
      caption.innerHTML = '<b class="red">克苏鲁之眼</b> 苏醒';
    } else if (phase === 'eye' && phaseT > 3.6) {
      phase = 'fade'; phaseT = 0;
    } else if (phase === 'fade' && phaseT > 0.7) {
      build();
    }

    ctx.globalAlpha = 1;
    ctx.fillStyle = '#1a1210';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    const plazaPulse = 0.5 + 0.5 * Math.sin(t * 3);
    for (let y = 0; y < N; y++) {
      for (let x = 0; x < N; x++) {
        const px = x * T, py = y * T;
        const inPlaza = x >= PL0 * 2 + 1 && x <= PL1 * 2 + 1 && y >= PL0 * 2 + 1 && y <= PL1 * 2 + 1;
        if (grid[y * N + x]) {
          ctx.fillStyle = '#463c36';
          ctx.fillRect(px, py, T, T);
          ctx.fillStyle = '#6a5c52';
          ctx.fillRect(px, py, T, 4);
          ctx.fillStyle = '#2c2522';
          ctx.fillRect(px, py + T - 3, T, 3);
        } else if (inPlaza) {
          ctx.fillStyle = `rgb(${92 + plazaPulse * 30},${42 + plazaPulse * 6},40)`;
          ctx.fillRect(px, py, T, T);
        } else {
          ctx.fillStyle = (x + y) % 2 ? '#2b221e' : '#302621';
          ctx.fillRect(px, py, T, T);
        }
      }
    }
    // 入口标记
    if (phase !== 'gen') {
      ctx.fillStyle = '#5eead4';
      ctx.globalAlpha = 0.5 + 0.5 * Math.sin(t * 6);
      ctx.fillRect(entrance[0] * T + 4, entrance[1] * T + 4, T - 8, T - 8);
      ctx.globalAlpha = 1;
    }
    if (path && path.length && phase !== 'gen') {
      const prog = phase === 'path' ? Math.min(1, phaseT / 1.8) : 1;
      const n = Math.max(1, Math.floor(prog * (path.length - 1)) + 1);
      ctx.save();
      ctx.strokeStyle = '#5eead4';
      ctx.shadowColor = '#5eead4';
      ctx.shadowBlur = 12;
      ctx.lineWidth = 4;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      ctx.setLineDash([2, 7]);
      ctx.lineDashOffset = -t * 30;
      ctx.beginPath();
      for (let i = 0; i < n; i++) {
        const x = (path[i] % N) * T + T / 2, y = ((path[i] / N) | 0) * T + T / 2;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
      ctx.restore();
      const head = path[n - 1];
      ctx.fillStyle = '#e8fffb';
      ctx.beginPath();
      ctx.arc((head % N) * T + T / 2, ((head / N) | 0) * T + T / 2, 4, 0, TAU);
      ctx.fill();
    }
    if (phase === 'eye' || phase === 'fade') {
      eyeLook.t -= dt;
      if (eyeLook.t <= 0) { eyeLook.t = rand(0.5, 1.3); const a = rand(0, TAU), d = rand(0, 1); eyeLook.tx = Math.cos(a) * d; eyeLook.ty = Math.sin(a) * d; }
      eyeLook.x += (eyeLook.tx - eyeLook.x) * Math.min(1, dt * 8);
      eyeLook.y += (eyeLook.ty - eyeLook.y) * Math.min(1, dt * 8);
      const grow = phase === 'eye' ? Math.min(1, phaseT / 0.8) : 1;
      const blink = phase === 'eye' && phaseT > 2.2 && phaseT < 2.45 ? Math.abs(phaseT - 2.325) / 0.125 : 1;
      const c = (PL0 * 2 + 1 + 1.5) * T;
      drawEye(c, c, T * 1.25 * (0.3 + 0.7 * grow) * (1 + Math.sin(t * 4) * 0.03), t, Math.min(grow, blink));
    }
    if (phase === 'fade') {
      ctx.fillStyle = `rgba(11,10,16,${Math.min(1, phaseT / 0.7)})`;
      ctx.fillRect(0, 0, canvas.width, canvas.height);
    }
  }

  build();
  addTicker(canvas, tick);
}

/* ================= 天赋 ================= */

const TALENTS = [
  { name: '重手', icon: '力', c: '#ff6b4a', detail: '造成的伤害变为 1.2 倍。', req: '累计造成伤害', goal: 250 },
  { name: '远行', icon: '行', c: '#7ee07e', detail: '移动速度 +18。', req: '累计移动距离', goal: 900 },
  { name: '轻身', icon: '轻', c: '#8fb8ff', detail: '闪避消耗的 STA 从 24 降到 14。', req: '累计闪避次数', goal: 6 },
  { name: '熟练', icon: '熟', c: '#ffd166', detail: '战斗熟练度：技能冷却 -25%，攻击速度 +25%。', req: '累计释放技能', goal: 12 },
  { name: '世界指引', icon: '引', c: '#5eead4', detail: '获得额外技能「寻路」，按 G 开关，指向迷宫遗迹并画出通路。', req: '击杀入侵世界的怪物', goal: 20 },
  { name: '以小博大', icon: '博', c: '#b98cff', detail: '对等级高于自己的敌人，造成的伤害变为 1.3 倍。', req: '击杀等级超过自己的怪物', goal: 10 },
];

function createTalents() {
  const grid = $('#talentGrid');
  const items = TALENTS.map((tl, i) => {
    const el = document.createElement('article');
    el.className = 'talent reveal';
    el.style.setProperty('--c', tl.c);
    el.innerHTML = `<span class="got">已解锁</span><div class="talent-icon">${tl.icon}</div><h3>${tl.name}</h3><p>${tl.detail}</p>
      <div class="req">${tl.req} <b class="count">0</b> / ${tl.goal}</div><div class="progress"><i></i></div>`;
    grid.append(el);
    return { tl, el, bar: el.querySelector('.progress i'), count: el.querySelector('.count'), v: 0, speed: 0.14 + i * 0.03 + Math.random() * 0.08 };
  });
  let wait = 0;
  addTicker(grid, (dt) => {
    let done = true;
    for (const it of items) {
      if (it.v < 1) {
        done = false;
        it.v = Math.min(1, it.v + dt * it.speed * (0.6 + Math.random()));
        it.bar.style.width = (it.v * 100).toFixed(1) + '%';
        it.count.textContent = Math.floor(it.v * it.tl.goal);
        if (it.v >= 1) it.el.classList.add('unlocked');
      }
    }
    if (done) {
      wait += dt;
      if (wait > 4) {
        wait = 0;
        for (const it of items) { it.v = 0; it.el.classList.remove('unlocked'); }
      }
    }
  });
}

/* ================= 操作 ================= */

const KB_ROWS = [
  { o: 0, keys: ['Esc'] },
  { o: 0, keys: ['Tab', 'Q', 'W', 'E', 'R'] },
  { o: 0, keys: ['Shift', 'A', 'S', 'D', 'F', 'G'] },
  { o: 2.2, keys: ['C', 'V'] },
  { o: 1.2, keys: ['Space'] },
];
const ACTIONS = [
  { keys: ['W', 'A', 'S', 'D'], seq: true, label: '移动', dt: 'W A S D' },
  { keys: ['LMB'], label: '轻击（点按；机甲人每发耗 1 发弹药）', dt: '左键' },
  { keys: ['LMB'], hold: true, label: '重击（按住约 0.42 秒）/ 机甲人换弹', dt: '按住左键' },
  { keys: ['Shift', 'RMB'], label: '闪避 / 法师闪现', dt: 'Shift / 右键' },
  { keys: ['Space'], label: '跳跃', dt: 'Space' },
  { keys: ['Q'], label: '防御', dt: 'Q' },
  { keys: ['E'], label: '恢复', dt: 'E' },
  { keys: ['R', 'F', 'C', 'V'], seq: true, label: '技能栏（V 仅法师）', dt: 'R F C V' },
  { keys: ['G'], label: '寻路（天赋「世界指引」）', dt: 'G' },
  { keys: ['Tab'], label: '技能与天赋说明', dt: 'Tab' },
  { keys: ['Esc'], label: '暂停', dt: 'Esc' },
];

function createControls() {
  const kb = $('#keyboard');
  const legend = $('#keyLegend');
  const keyEls = {};
  const used = new Set(ACTIONS.flatMap((a) => a.keys));
  const keysCol = document.createElement('div');
  keysCol.className = 'kb-keys';
  for (const row of KB_ROWS) {
    const r = document.createElement('div');
    r.className = 'krow';
    r.style.setProperty('--o', row.o);
    for (const k of row.keys) {
      const el = document.createElement('div');
      el.className = 'key' + (k.length > 1 ? (k === 'Space' ? ' space' : ' wide') : '') + (used.has(k) ? ' used' : '');
      el.textContent = k;
      keyEls[k] = el;
      r.append(el);
    }
    keysCol.append(r);
  }
  const mouse = document.createElement('div');
  mouse.className = 'mouse';
  for (const [k, label] of [['LMB', '左键'], ['RMB', '右键']]) {
    const el = document.createElement('div');
    el.className = 'key mbtn used';
    el.textContent = label;
    keyEls[k] = el;
    mouse.append(el);
  }
  kb.append(keysCol, mouse);

  const rows = ACTIONS.map((a) => {
    const dt = document.createElement('dt');
    dt.textContent = a.dt;
    const dd = document.createElement('dd');
    dd.textContent = a.label;
    legend.append(dt, dd);
    return [dt, dd];
  });

  let idx = 0, t = 0;
  addTicker(kb, (dt) => {
    t += dt;
    const a = ACTIONS[idx];
    const len = a.seq ? 0.45 * a.keys.length + 0.3 : a.hold ? 1.5 : 1.1;
    for (const el of Object.values(keyEls)) el.classList.remove('lit');
    if (a.seq) {
      const k = a.keys[Math.min(a.keys.length - 1, Math.floor(t / 0.45))];
      keyEls[k].classList.add('lit');
    } else if (a.hold ? t < 1.2 : t < 0.35 || (t > 0.55 && t < 0.8)) {
      for (const k of a.keys) keyEls[k].classList.add('lit');
    }
    rows.forEach(([d1, d2], i) => { d1.classList.toggle('hl', i === idx); d2.classList.toggle('hl', i === idx); });
    if (t > len) { t = 0; idx = (idx + 1) % ACTIONS.length; }
  });
}

/* ================= 版本 ================= */

const VERSIONS = [
  ['v0.6.2', '新增进局加载页：点「出发」开新局或点「继续」读档后先落在黑底加载页——紫色方块扫描带自左向右循环掠过，中央偏下的进度条按真实准备进度推进（新局 5 步、读档 2 步），条首站着一只随进度走动的蓝色史莱姆；准备再快也至少停满 1 秒，进度走满后紫色脉冲一次才切入游戏，加载期间 BGM 不中断。开局准备拆成分步执行并预热首帧与 HUD 字形，进局不再卡一下。战士新增隐藏强化普攻「次元斩」：技能里带上「突刺」后，长按闪避 0.5 秒起判定（桌面 Shift / 右键，安卓闪避键），10 秒内打完 3 次突刺 + 3 次普攻 + 1 次跳跃即判定成功，此后 20 秒内的下一次普攻或重击打出——只消耗 30 体力，角色沿六芒星边跑边斩六刀，斩完回到原位停顿一拍后六刃一起结算（单刃 26 伤害 / 16 破韧，顺手劈开迷宫墙），期间除玩家外全场时缓到 0.22 倍、相机锁在起手点，并叠加浅蓝滤镜、把画面沿刃线错开；起手时 BGM 静音让位给配音，打完空一拍再按 1.4 秒渐入回来。天赋进度 HUD 改为可折叠：平时只占一行「天赋　已解锁 N / M」，点它才展开重手 / 远行 / 轻身 / 熟练 / 指引 / 以小博大（暴食）明细，新一局自动回到折叠态。安卓端接上鼠标后，攻击与技能朝向改为跟随鼠标指针（原来跟摇杆的移动方向），手指一落屏即交还摇杆。机甲人贴图整体下沉 5%、头顶昵称抬高，枪管不再压住名字'],
  ['v0.6.1', '浅水区的排雷棋盘从水域旁挪到圆形浅水区正中央：5×5 棋盘落在水中央，中心格就是场地圆心，四周被浅水环绕；棋盘格仍是干地（可走），站在上面仍受浅水减速（×0.82，跳跃 / 飞行免疫），棋盘外扩 2 格内的岩石 / 灌木 / 水塘一并抹平，免得树贴图盖住格子数字。史莱姆形态（拟态成史莱姆）的图鉴图标与场上形象统一重上色成浅蓝（保留明暗轮廓），史莱姆之躯头顶新增闪烁的绿色倒三角标记；boss 形态手绘本体现在跟随跳跃抬升与朝向。弹出面板（暂停 / 确认结算 / 虚空抉择 / 回归抉择 / 拟态图鉴）时 BGM 不再停播，改为压低到 35% 音量，关掉面板立刻恢复；结算仍切 5 号终曲，切后台才真正暂停。史莱姆形态加成补上「回蓝速度 +100%」。修复：已是史莱姆之躯后死亡复活面板里仍出现「使用史莱姆核心回归」选项；修复站在棋盘边缘时浅水 boss 房提前沉掉'],
  ['v0.6.0', '新增第二个 boss 房「浅水区」：击败过一次克苏鲁之眼后，远处会刷出一片半径 14 格的圆形浅水区（通行同平地，只对生物略微减速），旁边地面是一块 5×5、5 颗雷的扫雷棋盘——跳跃起跳与落地各揭示一次脚下格，数字为相邻雷数、0 格连锁翻开，第一下必定安全；踩雷扣 14 点腐蚀伤害并让本场理智（SAN）上限 -10（保底 30%）。排完雷「巨型腐化史莱姆」用 1.7 秒从水中浮出（420 血 / 30 盾 / 36 韧，等级为玩家 +5）：冲撞（预警 0.55 秒画冲刺走廊 → 冲出 0.5 秒）、弹跳砸击（预警 0.6 秒画落点圈 → 落地半径 52）、腐蚀水弹（预警 0.5 秒画 5 条射线 → 连射 5 发）三招循环，地上画的预警范围就是真正打到的范围；它经过处留下腐蚀粘液（每 0.5 秒扣 1.5 HP，护盾 / 飞行 / 跳跃免疫）。战斗期间 SAN 在 10 分钟内线性清零，归零即死且不能被意识回归符咒复活。击败后掉落新物品「史莱姆核心」（结算每个折算 400 积分）：死亡时若同时持有符咒与核心，可消耗 1 张符咒 + 1 个核心原地复活并化为「史莱姆之躯」——受伤 -10%、恢复改为回复 20% 最大生命与体力，并获得拟态：按 T 时停打开怪物图鉴（安卓右上角「拟态」），左 Ctrl（安卓「形态技」）施放当前形态技能，9 种形态各有加成与 4~6 秒冷却，吞噬过的怪才能幻化；累计吞噬 100 只解锁天赋【暴食】（经验翻倍）。BGM 新增 4 号复活曲与 5 号结算终曲（回主菜单不断，下一局才回到 1 号），复活 / 拒绝结算按角色性别播「继续前进」配音；大体积 boss 改用专属受击半径（克苏鲁之眼 36、腐化史莱姆 30），不再只有正中心吃得到伤害；修复 Windows 打包会把 build 目录里早已改名 / 删掉的旧 BGM 与贴图一起打进包'],
  ['v0.5.8', '修复安卓端战士 / 女剑客终极技能「I am atomic」的冷却显示：触屏技能按钮此前因冷却时长计算漏了该技能而始终显示可释放、不画冷却扇形也不显示剩余秒数；现补上该技能并重载一个带 lastCd 的版本，用每次释放实际定下的冷却时长当分母（满 MP 约 12 秒、只够门槛约 30 秒），桌面与安卓共用同一套 skillCooldownMax 逻辑一并修好'],
  ['v0.5.7', '战士 / 女剑客新增终极技能「I am atomic」：倾泻当前全部 MP，抹除以玩家为中心一屏范围的所有怪物（含克苏鲁之眼）；蓄力 4 秒并配 5 秒英文吟唱配音与逐词字幕，期间叠加全屏紫色滤镜、紫色网格扫描与脚下能量环；引爆有 0.12 秒全屏白闪（时长翻倍）、紫黑火球与蘑菇云、强震屏；范围内地形被烧成沙地（持久化到存档）并破坏迷宫墙；冷却 30 秒，按消耗 MP 比例减免（满 MP 约 12 秒）'],
  ['v0.5.6', '机甲人普攻固定为点射（「肘击」不再顶替普攻，只由技能键挥出）；修复带肘击的机甲人击败 boss 后仍劈不开迷宫墙'],
  ['v0.5.5', 'Windows 可执行文件换成多尺寸图标（16~256 px）：资源管理器、任务栏与文件属性都显示游戏图标'],
  ['v0.5.4', '新物品「意识回归符咒」：克苏鲁之眼掉落、可堆叠，死亡时可选确认回归（原地复活，生命 25%，护盾体力回满、短暂无敌，清空身边弹幕并推开怪物）或拒绝回归（直接结算，每张折算 500 积分）；确认回归会带上本轮永久诅咒「存在被克苏鲁余光注意！」，之后每隔 22~43 秒刷出双倍血量的精英怪，击杀积分翻倍；比岩石 / 灌木高的角色不再被其遮挡；BGM 改为默认循环 1 号曲、回血播一次 2 号曲、拒绝结束本轮播一次 3 号曲'],
  ['v0.5.3', '机甲人新技能「肘击」；设置里可勾选开局直接拥有天赋「世界指引」；修复安卓 / Linux 端不生成迷宫遗迹（雷达标不出遗迹、「寻路」无效）；修复画面外怪物陷进水 / 岩石里卡死；安卓迷宫地图挪到雷达左侧，不再被右下角按键遮挡'],
  ['v0.5.2', '战士 / 女剑客技能特效迁移自展示页（回旋斩、剑气、突刺、狂化）；剑气特效改用与实际判定一致的扇形；万葬改为两段伤害；安卓端新增「寻路」按键；迷宫地图进入即显示，路线仍由寻路开关控制'],
  ['v0.5.1', '画面外的怪物跨过岩石和灌木追击，离画面越远越快；天赋「以小博大」：击杀 10 个等级超过自己的怪物后，对更高等级敌人伤害变为 1.3 倍'],
  ['v0.5.0', 'Windows：迷宫遗迹与克苏鲁之眼；击杀 20 个入侵怪物获得天赋「世界指引」，按 G 寻路；万葬播放配音；出发界面改用角色像素图'],
  ['v0.4.0', '新职业机甲人（8 方向，弹匣与换弹，八选三技能：蜂群、磁力场、医疗包、喷气背包等）；新怪物机器人小兵；修复飞行 / 跳跃落在岩石上卡住'],
  ['v0.3.4', '安卓：自动索敌（可开关），攻击与技能朝向最近的敌人，闪避仍按摇杆方向'],
  ['v0.3.3', '安卓：摇杆不再被另一只手的触屏干扰，左半屏专管摇杆，底座跟随手指'],
  ['v0.3.2', '安卓：修复掉帧、摇杆卡方向、多指时面板点不动；Windows 包改用官方 Qt，BGM 正常播放'],
  ['v0.3.1', '安卓改用固定签名，此后的新版本可直接覆盖安装并保留存档'],
  ['v0.3.0', '首个安卓版本，加入触屏摇杆与按键'],
  ['v0.2.0', 'Linux 版：.deb 与 AppImage'],
  ['v0.1.0', 'Windows 可玩包'],
];

function createTimeline() {
  const ol = $('#timeline');
  for (const [v, text] of VERSIONS) {
    const li = document.createElement('li');
    li.className = 'reveal';
    li.innerHTML = `<a class="ver" href="https://github.com/${REPO}/releases/tag/${v}" target="_blank" rel="noopener">${v}</a><p></p>`;
    li.querySelector('p').textContent = text;
    ol.append(li);
  }
}

/* ================= 页面杂项 ================= */

function setupPage() {
  const nav = $('#nav');
  const onScroll = () => nav.classList.toggle('solid', window.scrollY > 40);
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  const revealIO = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (e.isIntersecting) {
        e.target.classList.add('in');
        revealIO.unobserve(e.target);
      }
    }
  }, { threshold: 0.12 });
  document.querySelectorAll('.reveal').forEach((el, i) => {
    el.style.transitionDelay = ((i % 6) * 0.07) + 's';
    revealIO.observe(el);
  });

  const steps = [...document.querySelectorAll('#loop li')];
  let step = 0;
  addTicker($('#loop'), (() => {
    let t = 0;
    return (dt) => {
      t += dt;
      if (t > 1.3) { t = 0; step = (step + 1) % steps.length; }
      steps.forEach((li, i) => li.classList.toggle('active', i === step));
    };
  })());

  fetch(`https://api.github.com/repos/${REPO}/releases/latest`)
    .then((r) => (r.ok ? r.json() : null))
    .then((j) => { if (j && j.tag_name) $('#latestTag').textContent = j.tag_name; })
    .catch(() => {});
}

(async function main() {
  createTimeline();
  setupPage();
  requestAnimationFrame(loop);
  try {
    await loadSprites();
  } catch (e) {
    console.error(e);
    return;
  }
  addTicker($('#top'), createBattle($('#battle')));
  createHeroShowcase();
  createBestiary();
  createMaze($('#mazeCanvas'));
  createTalents();
  createControls();
  document.querySelectorAll('.monster.reveal, .talent.reveal').forEach((el) => {
    new IntersectionObserver((es, io) => {
      if (es[0].isIntersecting) { el.classList.add('in'); io.disconnect(); }
    }, { threshold: 0.1 }).observe(el);
  });
})();
