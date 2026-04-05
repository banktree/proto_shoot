#include "enemy.h"
#include "raymath.h"
#include <cmath>

Enemy::Enemy(Vector3 spawnPos, EnemyType t)
    : pos(spawnPos), type(t), shootTimer(0.8f), zigzagTimer(0.0f)
{
    switch (type) {
        case EnemyType::Fighter:
            hp = maxHp = 3;
            xSpeed = 12.0f;
            shootInterval = 1.6f;
            collisionRadius = 1.1f;
            scoreValue = 100;
            break;
        case EnemyType::Turret:
            hp = maxHp = 6;
            xSpeed = 0.0f;
            shootInterval = 1.1f;
            collisionRadius = 1.2f;
            scoreValue = 200;
            pos.y = 0.6f;
            break;
        case EnemyType::Flanker:
            hp = maxHp = 3;
            xSpeed = 10.0f;
            shootInterval = 1.3f;
            collisionRadius = 1.0f;
            scoreValue = 180;
            break;
        case EnemyType::Boss:
            hp = maxHp = 40;
            xSpeed = 3.5f;
            shootInterval = 0.45f;
            collisionRadius = 3.5f;
            scoreValue = 1500;
            break;
    }
}

void Enemy::Update(float dt, Vector3 playerPos, std::vector<Bullet>& enemyBullets) {
    // Movement
    switch (type) {
        case EnemyType::Fighter:
            pos.x -= xSpeed * dt;
            pos.z += Clamp(playerPos.z - pos.z, -5.0f, 5.0f) * dt;
            pos.y += Clamp(playerPos.y - pos.y, -3.0f, 3.0f) * dt;
            break;
        case EnemyType::Turret:
            break;
        case EnemyType::Flanker:
            zigzagTimer += dt;
            pos.x -= xSpeed * dt;
            pos.z += sinf(zigzagTimer * 2.5f) * 9.0f * dt;
            break;
        case EnemyType::Boss:
            pos.x -= xSpeed * dt;
            zigzagTimer += dt;
            pos.z += sinf(zigzagTimer * 1.0f) * 6.0f * dt;
            pos.y += Clamp(playerPos.y - pos.y, -2.0f, 2.0f) * 0.5f * dt;
            break;
    }

    // Shoot only while ahead of player
    shootTimer -= dt;
    if (shootTimer <= 0.0f && pos.x > playerPos.x) {
        shootTimer = shootInterval;

        Vector3 toPlayer = Vector3Subtract(playerPos, pos);
        float dist = Vector3Length(toPlayer);
        if (dist < 0.1f) return;
        Vector3 dir = Vector3Scale(toPlayer, 1.0f / dist);

        if (type == EnemyType::Boss) {
            // 5-way spread
            for (int i = -2; i <= 2; ++i) {
                Vector3 spread = {dir.x, dir.y, dir.z + i * 0.25f};
                float slen = Vector3Length(spread);
                if (slen > 0.001f) spread = Vector3Scale(spread, 1.0f / slen);
                Vector3 vel = Vector3Scale(spread, 20.0f);
                enemyBullets.emplace_back(pos, vel, 0.3f, 1, BulletOwner::Enemy, ORANGE, 3.0f);
            }
        } else if (type == EnemyType::Flanker) {
            Vector3 vel = Vector3Scale(dir, 18.0f);
            enemyBullets.emplace_back(pos, vel, 0.22f, 1, BulletOwner::Enemy, RED, 3.0f);
            for (int s : {-1, 1}) {
                Vector3 sv = {vel.x, vel.y, vel.z + s * 7.0f};
                enemyBullets.emplace_back(pos, sv, 0.22f, 1, BulletOwner::Enemy, ORANGE, 3.0f);
            }
        } else {
            Vector3 vel = Vector3Scale(dir, 18.0f);
            enemyBullets.emplace_back(pos, vel, 0.22f, 1, BulletOwner::Enemy, RED, 3.0f);
        }
    }
}

void Enemy::Draw() const {
    switch (type) {
        case EnemyType::Fighter:
            DrawCube(pos, 2.0f, 0.4f, 1.5f, RED);
            DrawCubeWires(pos, 2.0f, 0.4f, 1.5f, MAROON);
            DrawCube({pos.x + 0.2f, pos.y, pos.z}, 0.8f, 0.2f, 3.6f, RED);
            DrawSphere({pos.x + 0.7f, pos.y + 0.25f, pos.z}, 0.3f, Color{200, 50, 50, 255});
            break;

        case EnemyType::Turret: {
            DrawCube(pos, 1.8f, 0.5f, 1.8f, DARKGREEN);
            DrawCubeWires(pos, 1.8f, 0.5f, 1.8f, BLACK);
            DrawSphere({pos.x, pos.y + 0.4f, pos.z}, 0.5f, LIME);
            Vector3 barrel = {pos.x - 0.9f, pos.y + 0.4f, pos.z};
            DrawCylinder(barrel, 0.12f, 0.12f, 1.0f, 8, DARKGRAY);
            break;
        }

        case EnemyType::Flanker:
            DrawCube(pos, 1.8f, 0.3f, 2.0f, PURPLE);
            DrawCubeWires(pos, 1.8f, 0.3f, 2.0f, VIOLET);
            DrawCube({pos.x + 0.1f, pos.y, pos.z}, 0.6f, 0.15f, 3.8f, PURPLE);
            break;

        case EnemyType::Boss: {
            // Main hull
            DrawCube(pos, 5.0f, 1.2f, 4.0f, DARKPURPLE);
            DrawCubeWires(pos, 5.0f, 1.2f, 4.0f, PURPLE);
            // Side wings
            Vector3 wL = {pos.x, pos.y, pos.z - 3.8f};
            Vector3 wR = {pos.x, pos.y, pos.z + 3.8f};
            DrawCube(wL, 3.0f, 0.4f, 1.8f, DARKPURPLE);
            DrawCube(wR, 3.0f, 0.4f, 1.8f, DARKPURPLE);
            // Front cannon cluster
            for (int i = -1; i <= 1; ++i) {
                Vector3 cPos = {pos.x + 2.8f, pos.y, pos.z + i * 1.2f};
                DrawCylinder(cPos, 0.18f, 0.18f, 1.2f, 8, Color{180, 0, 0, 255});
            }
            // Cockpit dome
            DrawSphere({pos.x - 0.5f, pos.y + 0.8f, pos.z}, 0.8f, Color{100, 0, 200, 255});
            // HP glow: shifts red as boss is damaged
            float hpRatio = (float)hp / maxHp;
            Color glow = {(unsigned char)(255 * (1.0f - hpRatio)),
                          (unsigned char)(255 * hpRatio), 0, 180};
            DrawSphere({pos.x, pos.y, pos.z}, 0.4f, glow);
            break;
        }
    }
}

void Enemy::TakeDamage(int dmg) {
    hp -= dmg;
    if (hp < 0) hp = 0;
}
