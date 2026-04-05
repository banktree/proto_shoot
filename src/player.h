#pragma once
#include "raylib.h"
#include "bullet.h"
#include <vector>

class Player {
public:
    Player();

    // scrollSpeed: units/sec auto-advance in +X (set by Game each frame)
    void Update(float dt, std::vector<Bullet>& playerBullets, float scrollSpeed);
    void Draw() const;
    void TakeDamage(int dmg);
    bool IsAlive() const { return hp > 0; }

    Vector3 pos;
    int     hp, maxHp;
    int     bombs, maxBombs;
    bool    usedBomb;   // true the frame C is pressed; cleared each Update

private:
    float primaryTimer;
    float secondaryTimer;
    float invincibleTimer;

    static constexpr float CLIMB_SPEED     =  7.0f;
    static constexpr float STRAFE_SPEED    = 12.0f;
    static constexpr float MIN_ALT         =  0.5f;
    static constexpr float MAX_ALT         = 10.0f;
    static constexpr float Z_BOUND         = 11.0f;  // portrait: narrower play field
    static constexpr float PRIMARY_RATE    =  0.15f;
    static constexpr float SECONDARY_RATE  =  0.35f;
    static constexpr float INVINCIBLE_TIME =  1.5f;
};
