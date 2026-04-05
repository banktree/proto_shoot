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
    float       gravity;   // 0 = straight; negative = falls (used by ground bombs)
    bool        isBomb;    // parabolic ground-attack bomb

    Bullet(Vector3 p, Vector3 v, float r, int dmg, BulletOwner o, Color c,
           float life = 3.0f, float grav = 0.0f, bool bomb = false)
        : pos(p), vel(v), radius(r), damage(dmg), owner(o),
          active(true), lifetime(life), color(c),
          gravity(grav), isBomb(bomb)
    {}

    void Update(float dt) {
        if (!active) return;
        vel.y += gravity * dt;
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;
        lifetime -= dt;
        if (lifetime <= 0.0f) active = false;
        // Bombs are deactivated by CheckCollisions when y hits ground.
        // Clamp below ground so they don't fly into negative space.
        if (isBomb && pos.y < 0.05f) pos.y = 0.05f;
    }

    void Draw() const {
        if (!active) return;
        if (isBomb) {
            DrawSphere(pos, radius, color);
            // arc trail hint
            DrawSphereWires(pos, radius * 1.6f, 4, 4,
                Color{color.r, color.g, color.b, 60});
        } else {
            DrawSphere(pos, radius, color);
        }
    }
};
