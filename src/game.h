#pragma once
#include "raylib.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include <vector>
#include <random>

struct Obstacle {
    Vector3 pos;
    Vector3 halfSize;
    Color   color;
};

struct BombEffect {
    Vector3 pos;
    float   radius;
    float   maxRadius;
    float   timer;
    float   duration;
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
    void DrawObstacles();
    void DrawHUD();
    void DrawBombEffects();

    void SpawnEnemy();
    void CheckCollisions();
    void UpdateCamera();
    void Reset();
    void GenerateObstacles();
    void ApplyBomb();

    Camera3D camera;
    Player   player;

    std::vector<Enemy>      enemies;
    std::vector<Bullet>     playerBullets;
    std::vector<Bullet>     enemyBullets;
    std::vector<Obstacle>   obstacles;
    std::vector<BombEffect> bombEffects;

    int   score;
    int   wave;
    float enemySpawnTimer;
    float enemySpawnInterval;
    int   enemiesPerWave;
    int   enemiesSpawned;
    bool  gameOver;

    std::mt19937 rng;

    static constexpr float ARENA_SIZE = 45.0f;
    static constexpr float BOMB_RADIUS = 22.0f;
    static constexpr int   SCREEN_W   = 1280;
    static constexpr int   SCREEN_H   = 720;
};
