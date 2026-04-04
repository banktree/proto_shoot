#pragma once
#include "raylib.h"
#include "bullet.h"
#include <vector>

class Player {
public:
    Player();

    // Returns true if a bomb was activated this frame
    void Update(float dt, std::vector<Bullet>& playerBullets);
    void Draw() const;
    void TakeDamage(int dmg);
    bool IsAlive() const { return hp > 0; }

    Vector3 pos;
    int     hp, maxHp;
    int     bombs, maxBombs;
    bool    usedBomb;   // set true the frame C is pressed; cleared each Update

private:
    float primaryTimer;
    float secondaryTimer;
    float invincibleTimer;

    static constexpr float SPEED          = 18.0f;
    static constexpr float PRIMARY_RATE   = 0.12f;
    static constexpr float SECONDARY_RATE = 0.35f;
    static constexpr float INVINCIBLE_TIME = 1.5f;
};
