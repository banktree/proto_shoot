#pragma once
#include "raylib.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include <vector>
#include <random>

// Ground-level scenery buildings
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

// One scripted spawn event within a stage.
// Fires when the player's X progress into the stage reaches relX.
struct SpawnTrigger {
    float     relX;   // units from stage start that triggers this spawn
    EnemyType type;
    float     z, y;
    bool      fired;
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

    void GenerateAhead();          // spawn buildings ahead of player
    void BeginStage(int stage);    // fill spawnQueue for the stage
    void UpdateStage(float dt);    // fire triggers, detect stage clear
    void CheckCollisions();
    void UpdateCamera();
    void Reset();
    void ApplyBomb();

    Camera3D camera;
    Player   player;

    std::vector<Enemy>        enemies;
    std::vector<Bullet>       playerBullets;
    std::vector<Bullet>       enemyBullets;
    std::vector<Building>     buildings;
    std::vector<BombEffect>   bombEffects;
    std::vector<SpawnTrigger> spawnQueue;

    float scrollSpeed;     // units/sec auto-scroll
    float worldGenX;       // furthest X that terrain has been generated to

    int   score;
    int   currentStage;    // 1-based
    float stageStartX;     // player.pos.x when the current stage began
    float stageClearTimer; // >0 while "STAGE CLEAR" screen is shown
    bool  gameOver;

    std::mt19937 rng;

    // ── Tunable constants ─────────────────────────────────────────────────────
    static constexpr float ARENA_Z_HALF     = 12.0f;
    static constexpr float STAGE_LENGTH     = 400.0f;  // X units per stage
    static constexpr float STAGE_CLEAR_DELAY =  3.0f;  // seconds of "STAGE CLEAR"
    static constexpr float BOMB_SPLASH      =   7.0f;  // ground-bomb explosion radius
    static constexpr float SECTION_LEN      =  12.0f;
    static constexpr float GEN_LOOKAHEAD    =  70.0f;
    static constexpr float DESPAWN_BEHIND   =  35.0f;
    static constexpr float ENEMY_SPAWN_X    =  30.0f;
    static constexpr float BOMB_RADIUS      =  25.0f;  // C-bomb screen clear

    static constexpr float SCROLL_DEFAULT   =   7.0f;
    static constexpr float SCROLL_BOOST     =  18.0f;

    static constexpr int   SCREEN_W         =  720;
    static constexpr int   SCREEN_H         = 1080;
};
