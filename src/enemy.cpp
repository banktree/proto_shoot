#include "enemy.h"
#include "raymath.h"
#include <cmath>

Enemy::Enemy(Vector3 spawnPos, EnemyType t)
    : pos(spawnPos), type(t), shootTimer(0.5f), zigzagTimer(0.0f)
{
    switch (type) {
        case EnemyType::Basic:
            hp = maxHp = 3;
            speed = 6.0f;
            shootInterval = 1.6f;
            collisionRadius = 1.0f;
            scoreValue = 100;
            break;
        case EnemyType::Fast:
            hp = maxHp = 1;
            speed = 15.0f;
            shootInterval = 2.5f;
            collisionRadius = 0.7f;
            scoreValue = 150;
            break;
        case EnemyType::Tank:
            hp = maxHp = 10;
            speed = 3.5f;
            shootInterval = 1.0f;
            collisionRadius = 1.8f;
            scoreValue = 400;
            break;
        case EnemyType::Flanker:
            hp = maxHp = 3;
            speed = 9.0f;
            shootInterval = 1.2f;
            collisionRadius = 0.9f;
            scoreValue = 200;
            break;
    }
}

void Enemy::Update(float dt, Vector3 playerPos, std::vector<Bullet>& enemyBullets) {
    Vector3 toPlayer = Vector3Subtract(playerPos, pos);
    float dist = Vector3Length(toPlayer);

    // ── Movement patterns ─────────────────────────────────────────────────────
    Vector3 moveDir = {0.0f, 0.0f, 0.0f};

    if (dist > 0.1f) {
        Vector3 normalDir = Vector3Normalize(toPlayer);

        switch (type) {
            case EnemyType::Basic:
            case EnemyType::Fast:
            case EnemyType::Tank:
                moveDir = normalDir;
                break;

            case EnemyType::Flanker: {
                // Zigzag perpendicular to chase direction
                zigzagTimer += dt;
                Vector3 perp = {-normalDir.z, 0.0f, normalDir.x};
                float zigzag = sinf(zigzagTimer * 3.5f) * 0.8f;
                moveDir = Vector3Add(normalDir, Vector3Scale(perp, zigzag));
                float mlen = Vector3Length(moveDir);
                if (mlen > 0.001f) moveDir = Vector3Scale(moveDir, 1.0f / mlen);
                break;
            }
        }
    }

    pos = Vector3Add(pos, Vector3Scale(moveDir, speed * dt));

    // ── Shooting ──────────────────────────────────────────────────────────────
    shootTimer -= dt;
    if (shootTimer <= 0.0f && dist > 0.1f) {
        shootTimer = shootInterval;

        Vector3 dir = Vector3Normalize(toPlayer);
        Vector3 vel = Vector3Scale(dir, 14.0f);
        enemyBullets.emplace_back(pos, vel, 0.22f, 1, BulletOwner::Enemy, RED);

        // Tank fires a 3-way spread
        if (type == EnemyType::Tank) {
            for (int s : {-1, 1}) {
                float ang = s * 12.0f * DEG2RAD;
                float cs = cosf(ang), sn = sinf(ang);
                Vector3 spreadVel = {
                    vel.x * cs - vel.z * sn,
                    0.0f,
                    vel.x * sn + vel.z * cs
                };
                enemyBullets.emplace_back(pos, spreadVel, 0.22f, 1, BulletOwner::Enemy, ORANGE);
            }
        }
    }
}

void Enemy::Draw() const {
    switch (type) {
        case EnemyType::Basic:
            DrawCube(pos, 1.6f, 0.45f, 1.6f, RED);
            DrawCubeWires(pos, 1.6f, 0.45f, 1.6f, MAROON);
            // Small "wing" nubs
            {
                Vector3 w = {pos.x, pos.y, pos.z};
                DrawCube(w, 3.0f, 0.2f, 0.7f, (Color){200, 0, 0, 255});
            }
            break;

        case EnemyType::Fast:
            DrawCube(pos, 0.9f, 0.28f, 1.8f, ORANGE);
            DrawCubeWires(pos, 0.9f, 0.28f, 1.8f, RED);
            break;

        case EnemyType::Tank:
            DrawCube(pos, 2.8f, 0.8f, 2.8f, DARKGREEN);
            DrawCubeWires(pos, 2.8f, 0.8f, 2.8f, BLACK);
            // Turret
            {
                Vector3 turret = {pos.x, pos.y + 0.7f, pos.z};
                DrawCylinder(turret, 0.5f, 0.5f, 0.5f, 8, LIME);
            }
            break;

        case EnemyType::Flanker:
            DrawCube(pos, 1.2f, 0.35f, 2.0f, PURPLE);
            DrawCubeWires(pos, 1.2f, 0.35f, 2.0f, VIOLET);
            {
                Vector3 w = {pos.x, pos.y, pos.z + 0.2f};
                DrawCube(w, 3.2f, 0.15f, 0.7f, (Color){150, 0, 200, 255});
            }
            break;
    }

    // HP bar floating above enemy — drawn via GetWorldToScreen in HUD
}

void Enemy::TakeDamage(int dmg) {
    hp -= dmg;
    if (hp < 0) hp = 0;
}
