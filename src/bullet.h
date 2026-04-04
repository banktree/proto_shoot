#pragma once
#include "raylib.h"
#include "raymath.h"

enum class BulletOwner { Player, Enemy };

struct Bullet {
    Vector3     pos;
    Vector3     vel;
    float       radius;
    int         damage;
    BulletOwner owner;
    bool        active;
    float       lifetime;
    Color       color;

    Bullet(Vector3 p, Vector3 v, float r, int dmg, BulletOwner o, Color c, float life = 3.0f)
        : pos(p), vel(v), radius(r), damage(dmg), owner(o),
          active(true), lifetime(life), color(c)
    {}

    void Update(float dt) {
        if (!active) return;
        pos = Vector3Add(pos, Vector3Scale(vel, dt));
        lifetime -= dt;
        if (lifetime <= 0.0f) active = false;
    }

    void Draw() const {
        if (!active) return;
        DrawSphere(pos, radius, color);
    }
};
