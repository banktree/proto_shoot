#include "player.h"
#include "raymath.h"
#include <cmath>

static constexpr float ARENA_BOUND = 44.0f;

Player::Player()
    : pos({0.0f, 1.0f, 0.0f}),
      hp(5), maxHp(5),
      bombs(3), maxBombs(3),
      usedBomb(false),
      primaryTimer(0.0f),
      secondaryTimer(0.0f),
      invincibleTimer(0.0f)
{}

void Player::Update(float dt, std::vector<Bullet>& playerBullets) {
    usedBomb = false;

    // ── Movement ────────────────────────────────────────────────────────────
    Vector3 dir = {0.0f, 0.0f, 0.0f};
    if (IsKeyDown(KEY_W)) dir.z -= 1.0f;
    if (IsKeyDown(KEY_S)) dir.z += 1.0f;
    if (IsKeyDown(KEY_A)) dir.x -= 1.0f;
    if (IsKeyDown(KEY_D)) dir.x += 1.0f;

    float len = Vector3Length(dir);
    if (len > 0.0f) {
        dir = Vector3Scale(dir, SPEED * dt / len);
        pos = Vector3Add(pos, dir);
    }

    pos.x = Clamp(pos.x, -ARENA_BOUND, ARENA_BOUND);
    pos.z = Clamp(pos.z, -ARENA_BOUND, ARENA_BOUND);

    // ── Timers ───────────────────────────────────────────────────────────────
    if (primaryTimer   > 0.0f) primaryTimer   -= dt;
    if (secondaryTimer > 0.0f) secondaryTimer -= dt;
    if (invincibleTimer > 0.0f) invincibleTimer -= dt;

    // ── Z: Primary fire — straight shot ──────────────────────────────────────
    if (IsKeyDown(KEY_Z) && primaryTimer <= 0.0f) {
        primaryTimer = PRIMARY_RATE;
        Vector3 bPos = {pos.x, pos.y, pos.z - 1.0f};
        Vector3 bVel = {0.0f, 0.0f, -40.0f};
        playerBullets.emplace_back(bPos, bVel, 0.2f, 1, BulletOwner::Player, YELLOW);
    }

    // ── X: Secondary fire — spread shot ──────────────────────────────────────
    if (IsKeyDown(KEY_X) && secondaryTimer <= 0.0f) {
        secondaryTimer = SECONDARY_RATE;
        const float angles[] = {-20.0f, -10.0f, 0.0f, 10.0f, 20.0f};
        for (float angle : angles) {
            float rad = angle * DEG2RAD;
            Vector3 bVel = {sinf(rad) * 30.0f, 0.0f, -cosf(rad) * 30.0f};
            playerBullets.emplace_back(pos, bVel, 0.22f, 1, BulletOwner::Player, ORANGE);
        }
    }

    // ── C: Bomb ───────────────────────────────────────────────────────────────
    if (IsKeyPressed(KEY_C) && bombs > 0) {
        bombs--;
        usedBomb = true;
    }
}

void Player::Draw() const {
    // Flicker during invincibility frames
    bool show = true;
    if (invincibleTimer > 0.0f) {
        show = ((int)(GetTime() * 12) % 2) == 0;
    }
    if (!show) return;

    Color bodyCol = BLUE;

    // Fuselage
    DrawCube(pos, 1.4f, 0.35f, 2.2f, bodyCol);
    DrawCubeWires(pos, 1.4f, 0.35f, 2.2f, DARKBLUE);

    // Wings
    Vector3 wingPos = {pos.x, pos.y, pos.z + 0.3f};
    DrawCube(wingPos, 4.2f, 0.18f, 1.0f, bodyCol);
    DrawCubeWires(wingPos, 4.2f, 0.18f, 1.0f, DARKBLUE);

    // Tail fins
    Vector3 tailPos = {pos.x, pos.y + 0.25f, pos.z + 0.9f};
    DrawCube(tailPos, 1.6f, 0.5f, 0.4f, bodyCol);
    DrawCubeWires(tailPos, 1.6f, 0.5f, 0.4f, DARKBLUE);

    // Cockpit
    Vector3 cockpitPos = {pos.x, pos.y + 0.28f, pos.z - 0.4f};
    DrawSphere(cockpitPos, 0.38f, SKYBLUE);

    // Engine glow
    Vector3 enginePos = {pos.x, pos.y, pos.z + 1.1f};
    DrawSphere(enginePos, 0.25f, (Color){255, 140, 0, 200});
}

void Player::TakeDamage(int dmg) {
    if (invincibleTimer > 0.0f) return;
    hp -= dmg;
    if (hp < 0) hp = 0;
    invincibleTimer = INVINCIBLE_TIME;
}
