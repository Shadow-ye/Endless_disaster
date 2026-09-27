#pragma once

#include <QColor>
#include <QHash>
#include <QImage>
#include <QString>

class QPainter;

class SpriteAnim {
public:
    bool load(const QString& path);
    bool ok() const { return !image_.isNull() && frames_ > 0; }
    int frames() const { return frames_; }
    int dirs() const { return dirs_; }
    void draw(QPainter& painter, int frame, float x, float y, bool flip, float scale = 1.f, float lift = 0.f, const QColor& tint = QColor(), int dir = 0) const;

private:
    QImage image_;
    int frames_ = 0;
    int dirs_ = 1;
    mutable QHash<QRgb, QImage> tinted_;
};

class SpriteSet {
public:
    bool load(const QString& assetDir);

    SpriteAnim warriorIdle;
    SpriteAnim warriorRun;
    SpriteAnim warriorAttack;
    SpriteAnim warriorHurt;
    SpriteAnim warriorDeath;
    SpriteAnim swordIdle;
    SpriteAnim swordRun;
    SpriteAnim swordAttack;
    SpriteAnim swordHurt;
    SpriteAnim swordDeath;
    SpriteAnim mageIdle;
    SpriteAnim mageRun;
    SpriteAnim mageAttack;
    SpriteAnim mageHurt;
    SpriteAnim mageDeath;
    SpriteAnim slimeIdle;
    SpriteAnim slimeWalk;
    SpriteAnim slimeDeath;
    SpriteAnim skeletonIdle;
    SpriteAnim skeletonWalk;
    SpriteAnim skeletonAttack;
    SpriteAnim skeletonDefense;
    SpriteAnim skeletonHurt;
    SpriteAnim skeletonDeath;
    SpriteAnim mushroomIdle;
    SpriteAnim mushroomJump;
    SpriteAnim mushroomDeath;
    SpriteAnim flyerIdle;
    SpriteAnim flyerFly;
    SpriteAnim flyerAttack;
    SpriteAnim flyerHurt;
    SpriteAnim flyerDeath;
    QImage tiles;
};
