#pragma once
#include "raylib.h"
#include "bullet.h"
#include <vector>

enum class EnemyType { Basic, Fast, Tank, Flanker };

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
    float speed;
    float zigzagTimer;
};
