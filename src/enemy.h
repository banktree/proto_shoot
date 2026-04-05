#pragma once
#include "raylib.h"
#include "bullet.h"
#include <vector>

// Fighter : aerial, advances in -X, tracks player Z/Y
// Turret  : stationary on ground, shoots at player
// Flanker : zigzags in Z while advancing in -X
// Boss    : large craft, high HP, multi-directional fire
enum class EnemyType { Fighter, Turret, Flanker, Boss };

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
