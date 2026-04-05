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
        case EnemyType::Kamikaze:
            hp = maxHp = 1;
            xSpeed = 24.0f;   // beelines; speed used as vector magnitude
            shootInterval = 99.0f;
            collisionRadius = 0.8f;
            scoreValue = 150;
            break;
        case EnemyType::Turret:
            hp = maxHp = 6;
            xSpeed = 0.0f;
            shootInterval = 1.1f;
            collisionRadius = 1.2f;
            scoreValue = 200;
            pos.y = 0.5f;     // sit on the ground
            break;
        case EnemyType::Flanker:
            hp = maxHp = 3;
            xSpeed = 10.0f;
            shootInterval = 1.3f;
            collisionRadius = 1.0f;
            scoreValue = 180;
            break;
    }
}

void Enemy::Update(float dt, Vector3 playerPos, std::vector<Bullet>& enemyBullets) {
    // ── Movement ──────────────────────────────────────────────────────────────
    switch (type) {
        case EnemyType::Fighter:
            // Advance in -X; slow Z tracking
            pos.x -= xSpeed * dt;
            pos.z += Clamp(playerPos.z - pos.z, -5.0f, 5.0f) * dt;
            break;

        case EnemyType::Kamikaze: {
            // Directly toward player at full speed
            Vector3 dir = Vector3Subtract(playerPos, pos);
            float dist = Vector3Length(dir);
            if (dist > 0.1f)
                pos = Vector3Add(pos, Vector3Scale(Vector3Scale(dir, 1.0f / dist), xSpeed * dt));
            break;
        }

        case EnemyType::Turret:
            // Stationary; slight Z rotation isn't modelled — just holds position
            break;

        case EnemyType::Flanker:
            // Zigzag in Z while advancing
            zigzagTimer += dt;
            pos.x -= xSpeed * dt;
            pos.z += sinf(zigzagTimer * 2.5f) * 9.0f * dt;
            break;
    }

    // ── Shooting — only while ahead of the player ────────────────────────────
    shootTimer -= dt;
    if (shootTimer <= 0.0f && pos.x > playerPos.x) {
        shootTimer = shootInterval;

        Vector3 dir = Vector3Subtract(playerPos, pos);
        float dist = Vector3Length(dir);
        if (dist > 0.1f) {
            Vector3 vel = Vector3Scale(Vector3Scale(dir, 1.0f / dist), 18.0f);
            enemyBullets.emplace_back(pos, vel, 0.22f, 1, BulletOwner::Enemy, RED, 3.0f);

            // Flanker fires a 3-way Z spread
            if (type == EnemyType::Flanker) {
                for (int s : {-1, 1}) {
                    Vector3 sv = {vel.x, vel.y, vel.z + s * 7.0f};
                    enemyBullets.emplace_back(pos, sv, 0.22f, 1, BulletOwner::Enemy, ORANGE, 3.0f);
                }
            }
        }
    }
}

void Enemy::Draw() const {
    switch (type) {
        case EnemyType::Fighter:
            // Fuselage facing -X (coming at player)
            DrawCube(pos, 2.0f, 0.4f, 1.5f, RED);
            DrawCubeWires(pos, 2.0f, 0.4f, 1.5f, MAROON);
            // Wings spanning Z
            DrawCube({pos.x + 0.2f, pos.y, pos.z}, 0.8f, 0.2f, 3.6f, RED);
            DrawSphere({pos.x + 0.7f, pos.y + 0.25f, pos.z}, 0.3f, Color{200, 50, 50, 255});
            break;

        case EnemyType::Kamikaze:
            DrawCube(pos, 1.5f, 0.3f, 1.0f, ORANGE);
            DrawCubeWires(pos, 1.5f, 0.3f, 1.0f, RED);
            // Engine trail suggestion
            DrawSphere({pos.x + 0.8f, pos.y, pos.z}, 0.25f, Color{255, 100, 0, 180});
            break;

        case EnemyType::Turret: {
            // Base block
            DrawCube(pos, 1.8f, 0.5f, 1.8f, DARKGREEN);
            DrawCubeWires(pos, 1.8f, 0.5f, 1.8f, BLACK);
            // Dome
            DrawSphere({pos.x, pos.y + 0.4f, pos.z}, 0.5f, LIME);
            // Barrel pointing in -X
            Vector3 barrel = {pos.x - 0.9f, pos.y + 0.4f, pos.z};
            DrawCylinder(barrel, 0.12f, 0.12f, 1.0f, 8, DARKGRAY);
            break;
        }

        case EnemyType::Flanker:
            DrawCube(pos, 1.8f, 0.3f, 2.0f, PURPLE);
            DrawCubeWires(pos, 1.8f, 0.3f, 2.0f, VIOLET);
            DrawCube({pos.x + 0.1f, pos.y, pos.z}, 0.6f, 0.15f, 3.8f, PURPLE);
            break;
    }
}

void Enemy::TakeDamage(int dmg) {
    hp -= dmg;
    if (hp < 0) hp = 0;
}
