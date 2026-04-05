#pragma once
#include "raylib.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include <vector>
#include <random>

// Ground-level scenery / hazard buildings
struct Building {
    Vector3 pos;
    Vector3 halfSize;
    Color   color;
};

struct BombEffect {
    Vector3 pos;
    float   radius, maxRadius;
    float   timer, duration;
    bool    active;
};

class Game {
public:
    Game();
    ~Game();
    void Run();

private:
    void Update(float dt);
    void Draw();
    void DrawTerrain();
    void DrawBuildings();
    void DrawBombEffects();
    void DrawHUD();

    void GenerateAhead();   // spawn buildings ahead of player
    void SpawnEnemy();
    void CheckCollisions();
    void UpdateCamera();
    void Reset();
    void ApplyBomb();

    Camera3D camera;
    Player   player;

    std::vector<Enemy>      enemies;
    std::vector<Bullet>     playerBullets;
    std::vector<Bullet>     enemyBullets;
    std::vector<Building>   buildings;
    std::vector<BombEffect> bombEffects;

    float scrollSpeed;      // world units/sec (runtime adjustable)
    float worldGenX;        // furthest X we have generated terrain to

    int   score;
    int   wave;
    float enemySpawnTimer;
    float enemySpawnInterval;
    bool  gameOver;
    bool  bossAlive;       // true while a Boss enemy is on screen
    int   lastBossWave;    // wave at which the last boss was spawned

    std::mt19937 rng;

    // ── Tunable constants ─────────────────────────────────────────────────────
    static constexpr float ARENA_Z_HALF    = 12.0f;  // narrowed for portrait
    static constexpr float SECTION_LEN     = 12.0f;
    static constexpr float GEN_LOOKAHEAD   = 70.0f;
    static constexpr float DESPAWN_BEHIND  = 35.0f;
    static constexpr float ENEMY_SPAWN_X   = 30.0f;  // visible at new camera angle
    static constexpr float BOMB_RADIUS     = 25.0f;

    // Scroll speed: default + spacebar boost
    static constexpr float SCROLL_DEFAULT  =  7.0f;
    static constexpr float SCROLL_BOOST    = 18.0f;

    static constexpr int   SCREEN_W        =  720;   // portrait
    static constexpr int   SCREEN_H        = 1080;
};
