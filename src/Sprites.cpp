#include "Sprites.h"

#include "Types.h"

#include <QPainter>

#include <algorithm>

bool SpriteAnim::load(const QString& path) {
    QImage image(path);
    if (image.isNull()) {
        return false;
    }
    image_ = image.convertToFormat(QImage::Format_ARGB32);
    frames_ = std::max(1, image_.width() / kFrame);
    dirs_ = std::max(1, image_.height() / kFrame);
    return true;
}

void SpriteAnim::draw(QPainter& painter, int frame, float x, float y, bool flip, float scale, float lift, const QColor& tint, int dir) const {
    if (!ok()) {
        return;
    }
    frame = std::clamp(frame, 0, frames_ - 1);
    dir = std::clamp(dir, 0, dirs_ - 1);
    const QImage* sourceImage = &image_;
    if (tint.isValid()) {
        const QRgb key = tint.rgba();
        if (tinted_.isNull() || tintKey_ != key) {
            tinted_ = image_;
            for (int py = 0; py < tinted_.height(); ++py) {
                auto* line = reinterpret_cast<QRgb*>(tinted_.scanLine(py));
                for (int px = 0; px < tinted_.width(); ++px) {
                    const int alpha = qAlpha(line[px]);
                    if (alpha == 0) {
                        continue;
                    }
                    line[px] = qRgba((qRed(line[px]) + tint.red()) / 2, (qGreen(line[px]) + tint.green()) / 2, (qBlue(line[px]) + tint.blue()) / 2, alpha);
                }
            }
            tintKey_ = key;
        }
        sourceImage = &tinted_;
    }
    const int size = int(kFrame * scale);
    painter.save();
    painter.translate(x, y - lift);
    painter.scale(flip ? -1.0 : 1.0, 1.0);
    painter.drawImage(QRect(-size / 2, int(-47 * scale), size, size), *sourceImage, QRect(frame * kFrame, dir * kFrame, kFrame, kFrame));
    painter.restore();
}

bool SpriteSet::load(const QString& assetDir) {
    auto loadHero = [&](const QString& folder, SpriteAnim& idle, SpriteAnim& run, SpriteAnim& attack, SpriteAnim& hurt, SpriteAnim& death) {
        return idle.load(assetDir + "/" + folder + "/idle.png")
            && run.load(assetDir + "/" + folder + "/run.png")
            && attack.load(assetDir + "/" + folder + "/attack.png")
            && hurt.load(assetDir + "/" + folder + "/hurt.png")
            && death.load(assetDir + "/" + folder + "/death.png");
    };
    const bool warrior = loadHero("hero_warrior", warriorIdle, warriorRun, warriorAttack, warriorHurt, warriorDeath);
    const bool sword = loadHero("hero_sword", swordIdle, swordRun, swordAttack, swordHurt, swordDeath);
    const bool mage = loadHero("hero_mage", mageIdle, mageRun, mageAttack, mageHurt, mageDeath);
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
    tiles = QImage(assetDir + "/tiles/tilemaps.png");
    return warrior && sword && mage && slime && skeleton && mushroom && flyer && !tiles.isNull();
}
