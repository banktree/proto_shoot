#include "player.h"
#include "raymath.h"
#include <cmath>

Player::Player()
    : pos({0.0f, 3.0f, 0.0f}),
      hp(5), maxHp(5),
      bombs(3), maxBombs(3),
      usedBomb(false),
      primaryTimer(0.0f),
      secondaryTimer(0.0f),
      invincibleTimer(0.0f)
{}

void Player::Update(float dt, std::vector<Bullet>& playerBullets, float scrollSpeed) {
    usedBomb = false;

    // ── Auto-scroll forward (+X) ──────────────────────────────────────────────
    pos.x += scrollSpeed * dt;

    // ── Arrow keys ────────────────────────────────────────────────────────────
    // UP / DOWN  → altitude
    if (IsKeyDown(KEY_UP))    pos.y += CLIMB_SPEED  * dt;
    if (IsKeyDown(KEY_DOWN))  pos.y -= CLIMB_SPEED  * dt;
    // LEFT / RIGHT → strafe (Z axis)
    if (IsKeyDown(KEY_LEFT))  pos.z -= STRAFE_SPEED * dt;
    if (IsKeyDown(KEY_RIGHT)) pos.z += STRAFE_SPEED * dt;

    pos.y = Clamp(pos.y, MIN_ALT, MAX_ALT);
    pos.z = Clamp(pos.z, -Z_BOUND, Z_BOUND);

    // ── Timers ────────────────────────────────────────────────────────────────
    if (primaryTimer    > 0.0f) primaryTimer    -= dt;
    if (secondaryTimer  > 0.0f) secondaryTimer  -= dt;
    if (invincibleTimer > 0.0f) invincibleTimer -= dt;

    // ── Z: Primary fire — straight forward ───────────────────────────────────
    if (IsKeyDown(KEY_Z) && primaryTimer <= 0.0f) {
        primaryTimer = PRIMARY_RATE;
        Vector3 bPos = {pos.x + 1.5f, pos.y, pos.z};
        playerBullets.emplace_back(bPos, Vector3{55.0f, 0.0f, 0.0f},
                                   0.2f, 1, BulletOwner::Player, YELLOW, 1.5f);
    }

    // ── X: Spread shot — forward + Z fan ─────────────────────────────────────
    if (IsKeyDown(KEY_X) && secondaryTimer <= 0.0f) {
        secondaryTimer = SECONDARY_RATE;
        const float zVels[] = {-10.0f, 0.0f, 10.0f};
        for (float zv : zVels) {
            playerBullets.emplace_back(pos, Vector3{45.0f, 0.0f, zv},
                                       0.22f, 1, BulletOwner::Player, ORANGE, 1.2f);
        }
    }

    // ── C: Bomb ───────────────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_C) && bombs > 0) {
        bombs--;
        usedBomb = true;
    }
}

void Player::Draw() const {
    // ── Ground shadow — altitude indicator (Zaxxon style) ─────────────────────
    float altRatio    = (pos.y - 0.5f) / 9.5f;           // 0 = ground, 1 = ceiling
    float shadowR     = Lerp(1.5f, 0.4f, altRatio);
    unsigned char sha = (unsigned char)Lerp(180.0f, 30.0f, altRatio);
    DrawCircle3D({pos.x, 0.05f, pos.z}, shadowR,
                 {1.0f, 0.0f, 0.0f}, 90.0f, {0, 0, 0, sha});

    // ── Flicker during invincibility ──────────────────────────────────────────
    if (invincibleTimer > 0.0f && ((int)(GetTime() * 12) % 2) == 0) return;

    Color body = BLUE;

    // Fuselage (elongated along +X = travel direction)
    DrawCube(pos, 2.2f, 0.35f, 1.4f, body);
    DrawCubeWires(pos, 2.2f, 0.35f, 1.4f, DARKBLUE);

    // Wings (span Z)
    Vector3 wingPos = {pos.x - 0.2f, pos.y, pos.z};
    DrawCube(wingPos, 0.8f, 0.18f, 4.2f, body);
    DrawCubeWires(wingPos, 0.8f, 0.18f, 4.2f, DARKBLUE);

    // Tail fin
    Vector3 tailPos = {pos.x - 0.8f, pos.y + 0.3f, pos.z};
    DrawCube(tailPos, 0.6f, 0.6f, 0.2f, body);

    // Cockpit
    DrawSphere({pos.x + 0.6f, pos.y + 0.28f, pos.z}, 0.38f, SKYBLUE);

    // Engine exhaust glow
    DrawSphere({pos.x - 1.1f, pos.y, pos.z}, 0.28f, (Color){255, 140, 0, 220});
}

void Player::TakeDamage(int dmg) {
    if (invincibleTimer > 0.0f) return;
    hp -= dmg;
    if (hp < 0) hp = 0;
    invincibleTimer = INVINCIBLE_TIME;
}
