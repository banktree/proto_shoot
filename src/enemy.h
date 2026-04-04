#pragma once
#include "raylib.h"
#include "bullet.h"
#include <vector>

// Fighter  : aerial, advances in -X, tracks player Z
// Kamikaze : fast, beelines directly at player
// Turret   : stationary on ground, shoots at player
// Flanker  : zigzags in Z while advancing in -X
enum class EnemyType { Fighter, Kamikaze, Turret, Flanker };

class Enemy {
public:
    Enemy(Vector3 spawnPos, EnemyType type);

    void Update(float dt, Vector3 playerPos, std::vector<Bullet>& enemyBullets);
    void Draw() const;
    void TakeDamage(int dmg);
    bool IsAlive() const { return hp > 0; }

    Vector3   pos;
    int       hp, maxHp;
    EnemyType type;
    int       scoreValue;
    float     collisionRadius;

private:
    float shootTimer;
    float shootInterval;
    float xSpeed;        // advance speed in -X direction
    float zigzagTimer;
};
