#pragma once
#include "raylib.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include <vector>
#include <random>

// Walls span the full Z width; player must be within the altitude gap to pass.
struct Wall {
    float x;
    float gapBottom;   // gap opens at this altitude
    float gapTop;      // gap closes at this altitude
    bool  passed;      // true once player crosses without hitting
};

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
    void DrawWalls();
    void DrawBuildings();
    void DrawBombEffects();
    void DrawHUD();

    void GenerateAhead();   // spawn walls/buildings ahead of player
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
    std::vector<Wall>       walls;
    std::vector<Building>   buildings;
    std::vector<BombEffect> bombEffects;

    float scrollSpeed;      // world units/sec (runtime adjustable)
    float worldGenX;        // furthest X we have generated terrain to

    int   score;
    int   wave;
    float enemySpawnTimer;
    float enemySpawnInterval;
    bool  gameOver;

    std::mt19937 rng;

    // ── Tunable constants ─────────────────────────────────────────────────────
    static constexpr float ARENA_Z_HALF    = 16.0f;
    static constexpr float WALL_HEIGHT     = 12.0f;
    static constexpr float WALL_THICKNESS  =  1.5f;
    static constexpr float SECTION_LEN     = 12.0f;
    static constexpr float GEN_LOOKAHEAD   = 70.0f;
    static constexpr float DESPAWN_BEHIND  = 35.0f;
    static constexpr float ENEMY_SPAWN_X   = 30.0f;  // visible at new camera angle
    static constexpr float BOMB_RADIUS     = 25.0f;

    // Scroll speed limits ([ / ] keys)
    static constexpr float SCROLL_DEFAULT  =  7.0f;
    static constexpr float SCROLL_MIN      =  3.0f;
    static constexpr float SCROLL_MAX      = 22.0f;
    static constexpr float SCROLL_STEP     =  1.5f;

    static constexpr int   SCREEN_W        = 1280;
    static constexpr int   SCREEN_H        = 720;
};
