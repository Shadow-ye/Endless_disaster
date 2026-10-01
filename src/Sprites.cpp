#include "Sprites.h"

#include "Types.h"

#include <QPainter>

#include <algorithm>

bool SpriteAnim::load(const QString& path, int size) {
    QImage image(path);
    if (image.isNull()) {
        return false;
    }
    // 预乘格式是 QPainter 的快速路径，非预乘每次绘制都要逐像素转换
    image_ = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    tinted_.clear();
    size_ = size;
    frames_ = std::max(1, image_.width() / size_);
    dirs_ = std::max(1, image_.height() / size_);
    return true;
}

void SpriteAnim::draw(QPainter& painter, int frame, float x, float y, bool flip, float scale, float lift, const QColor& tint, int dir, bool recolor) const {
    if (!ok()) {
        return;
    }
    frame = std::clamp(frame, 0, frames_ - 1);
    dir = std::clamp(dir, 0, dirs_ - 1);
    const QImage* sourceImage = &image_;
    if (tint.isValid()) {
        const QRgb key = tint.rgba();
        QHash<QRgb, QImage>& cache = recolor ? recolored_ : tinted_;
        auto it = cache.find(key);
        if (it == cache.end()) {
            QImage tinted = image_.convertToFormat(QImage::Format_ARGB32);
            for (int py = 0; py < tinted.height(); ++py) {
                auto* line = reinterpret_cast<QRgb*>(tinted.scanLine(py));
                for (int px = 0; px < tinted.width(); ++px) {
                    const int alpha = qAlpha(line[px]);
                    if (alpha == 0) {
                        continue;
                    }
                    int r = 0;
                    int g = 0;
                    int b = 0;
                    if (recolor) {
                        // 重上色：按原像素明度铺目标色，暗部保留轮廓，亮部就是目标色
                        const int lum = (qRed(line[px]) * 299 + qGreen(line[px]) * 587 + qBlue(line[px]) * 114) / 1000;
                        const int k = 90 + lum * 165 / 255;
                        r = tint.red() * k / 255;
                        g = tint.green() * k / 255;
                        b = tint.blue() * k / 255;
                    } else {
                        r = (qRed(line[px]) + tint.red()) / 2;
                        g = (qGreen(line[px]) + tint.green()) / 2;
                        b = (qBlue(line[px]) + tint.blue()) / 2;
                    }
                    line[px] = qRgba(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), alpha);
                }
            }
            it = cache.insert(key, tinted.convertToFormat(QImage::Format_ARGB32_Premultiplied));
        }
        sourceImage = &it.value();
    }
    const int size = int(size_ * scale);
    painter.save();
    painter.translate(x, y - lift);
    painter.scale(flip ? -1.0 : 1.0, 1.0);
    painter.drawImage(QRect(-size / 2, int(-(size_ - 17) * scale), size, size), *sourceImage, QRect(frame * size_, dir * size_, size_, size_));
    painter.restore();
}

bool SpriteSet::load(const QString& assetDir) {
    auto loadHero = [&](const QString& folder, SpriteAnim& idle, SpriteAnim& run, SpriteAnim& attack, SpriteAnim& hurt, SpriteAnim& death, int size = kFrame) {
        return idle.load(assetDir + "/" + folder + "/idle.png", size)
            && run.load(assetDir + "/" + folder + "/run.png", size)
            && attack.load(assetDir + "/" + folder + "/attack.png", size)
            && hurt.load(assetDir + "/" + folder + "/hurt.png", size)
            && death.load(assetDir + "/" + folder + "/death.png", size);
    };
    const bool warrior = loadHero("hero_warrior", warriorIdle, warriorRun, warriorAttack, warriorHurt, warriorDeath);
    const bool sword = loadHero("hero_sword", swordIdle, swordRun, swordAttack, swordHurt, swordDeath);
    const bool mage = loadHero("hero_mage", mageIdle, mageRun, mageAttack, mageHurt, mageDeath);
    // 步枪横向较长，64 像素帧放不下
    const bool robot = loadHero("hero_robot", robotIdle, robotRun, robotAttack, robotHurt, robotDeath, 96);
    const bool slime = slimeIdle.load(assetDir + "/slime/idle.png")
        && slimeWalk.load(assetDir + "/slime/walk.png")
        && slimeDeath.load(assetDir + "/slime/death.png");
    const bool skeleton = skeletonIdle.load(assetDir + "/skeleton/idle.png")
        && skeletonWalk.load(assetDir + "/skeleton/walk.png")
        && skeletonAttack.load(assetDir + "/skeleton/attack.png")
        && skeletonDefense.load(assetDir + "/skeleton/defense.png")
        && skeletonHurt.load(assetDir + "/skeleton/hurt.png")
        && skeletonDeath.load(assetDir + "/skeleton/death.png");
    const bool mushroom = mushroomIdle.load(assetDir + "/mushroom/idle.png")
        && mushroomJump.load(assetDir + "/mushroom/jump.png")
        && mushroomDeath.load(assetDir + "/mushroom/death.png");
    const bool flyer = flyerIdle.load(assetDir + "/flyer/idle.png")
        && flyerFly.load(assetDir + "/flyer/fly.png")
        && flyerAttack.load(assetDir + "/flyer/attack.png")
        && flyerHurt.load(assetDir + "/flyer/hurt.png")
        && flyerDeath.load(assetDir + "/flyer/death.png");
    const bool killbot = killbotWalk.load(assetDir + "/killbot/walk.png")
        && killbotAttack.load(assetDir + "/killbot/attack.png")
        && killbotDeath.load(assetDir + "/killbot/death.png");
    drone = QImage(assetDir + "/drone/drone.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    tiles = QImage(assetDir + "/tiles/tilemaps.png").convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return warrior && sword && mage && robot && slime && skeleton && mushroom && flyer && killbot && !tiles.isNull();
}
