#include "game.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

Game::Game()
    : score(0), wave(1),
      enemySpawnTimer(1.5f), enemySpawnInterval(2.0f),
      enemiesPerWave(10), enemiesSpawned(0),
      gameOver(false)
{
    InitWindow(SCREEN_W, SCREEN_H, "Proto Shoot — Isometric Aerial Combat");
    SetTargetFPS(60);

    rng.seed(12345);

    UpdateCamera();
    GenerateObstacles();
}

Game::~Game() {
    CloseWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main loop
// ─────────────────────────────────────────────────────────────────────────────

void Game::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (!gameOver) {
            Update(dt);
        } else {
            if (IsKeyPressed(KEY_R)) Reset();
        }
        Draw();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Reset
// ─────────────────────────────────────────────────────────────────────────────

void Game::Reset() {
    player = Player();
    enemies.clear();
    playerBullets.clear();
    enemyBullets.clear();
    bombEffects.clear();
    score = 0;
    wave  = 1;
    enemySpawnTimer    = 1.5f;
    enemySpawnInterval = 2.0f;
    enemiesPerWave     = 10;
    enemiesSpawned     = 0;
    gameOver           = false;
    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Obstacle generation
// ─────────────────────────────────────────────────────────────────────────────

void Game::GenerateObstacles() {
    std::uniform_real_distribution<float> posDist(-38.0f, 38.0f);
    std::uniform_real_distribution<float> widthDist(1.5f, 4.5f);
    std::uniform_real_distribution<float> heightDist(2.5f, 7.0f);

    for (int i = 0; i < 25; ++i) {
        float x = posDist(rng);
        float z = posDist(rng);

        // Clear zone around player start
        if (fabsf(x) < 9.0f && fabsf(z) < 9.0f) continue;

        float w = widthDist(rng);
        float h = heightDist(rng);
        float d = widthDist(rng);

        Obstacle obs;
        obs.pos      = {x, h * 0.5f, z};
        obs.halfSize = {w * 0.5f, h * 0.5f, d * 0.5f};

        // Colour variation: sandy browns / greys
        unsigned char r = (unsigned char)(90 + (int)(h * 12));
        unsigned char g = (unsigned char)(70 + (int)(w * 8));
        unsigned char b = (unsigned char)(50);
        obs.color = {r, g, b, 255};

        obstacles.push_back(obs);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void Game::Update(float dt) {
    // Player
    player.Update(dt, playerBullets);

    // Bomb activation
    if (player.usedBomb) ApplyBomb();

    // Obstacle push-back for player (simple sphere-vs-AABB)
    for (const auto& obs : obstacles) {
        float nearX = Clamp(player.pos.x, obs.pos.x - obs.halfSize.x, obs.pos.x + obs.halfSize.x);
        float nearZ = Clamp(player.pos.z, obs.pos.z - obs.halfSize.z, obs.pos.z + obs.halfSize.z);
        float dx = player.pos.x - nearX;
        float dz = player.pos.z - nearZ;
        const float playerR = 1.1f;
        if (dx * dx + dz * dz < playerR * playerR) {
            float dist = sqrtf(dx * dx + dz * dz);
            Vector3 push = (dist < 0.001f)
                ? Vector3{1.0f, 0.0f, 0.0f}
                : Vector3{dx / dist, 0.0f, dz / dist};
            player.pos.x = nearX + push.x * playerR;
            player.pos.z = nearZ + push.z * playerR;
        }
    }

    // Enemy spawning
    enemySpawnTimer -= dt;
    if (enemySpawnTimer <= 0.0f && enemiesSpawned < enemiesPerWave) {
        SpawnEnemy();
        enemySpawnTimer = enemySpawnInterval;
    }

    // Wave clear check
    if (enemiesSpawned >= enemiesPerWave && enemies.empty()) {
        wave++;
        enemiesPerWave     = 8 + wave * 4;
        enemiesSpawned     = 0;
        enemySpawnInterval = std::max(0.7f, 2.0f - wave * 0.1f);
        player.bombs       = std::min(player.bombs + 1, player.maxBombs);
    }

    // Enemies
    for (auto& e : enemies) {
        e.Update(dt, player.pos, enemyBullets);
    }

    // Bullets
    for (auto& b : playerBullets) b.Update(dt);
    for (auto& b : enemyBullets)  b.Update(dt);

    // Bomb effects
    for (auto& bf : bombEffects) {
        if (!bf.active) continue;
        bf.timer -= dt;
        bf.radius = bf.maxRadius * (1.0f - bf.timer / bf.duration);
        if (bf.timer <= 0.0f) bf.active = false;
    }

    CheckCollisions();

    // Tally score for enemies killed this frame, then remove them
    for (const auto& e : enemies) {
        if (!e.IsAlive()) score += e.scoreValue;
    }
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                       [](const Enemy& e) { return !e.IsAlive(); }),
        enemies.end());

    playerBullets.erase(
        std::remove_if(playerBullets.begin(), playerBullets.end(),
                       [](const Bullet& b) { return !b.active; }),
        playerBullets.end());

    enemyBullets.erase(
        std::remove_if(enemyBullets.begin(), enemyBullets.end(),
                       [](const Bullet& b) { return !b.active; }),
        enemyBullets.end());

    bombEffects.erase(
        std::remove_if(bombEffects.begin(), bombEffects.end(),
                       [](const BombEffect& bf) { return !bf.active; }),
        bombEffects.end());

    if (!player.IsAlive()) gameOver = true;

    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enemy spawning
// ─────────────────────────────────────────────────────────────────────────────

void Game::SpawnEnemy() {
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> typeDist(0.0f, 1.0f);

    float   angle     = angleDist(rng);
    float   spawnDist = ARENA_SIZE * 0.85f;
    Vector3 spawnPos  = {
        Clamp(player.pos.x + cosf(angle) * spawnDist, -ARENA_SIZE, ARENA_SIZE),
        1.0f,
        Clamp(player.pos.z + sinf(angle) * spawnDist, -ARENA_SIZE, ARENA_SIZE)
    };

    float r = typeDist(rng);
    EnemyType type;
    if      (wave >= 3 && r < 0.20f) type = EnemyType::Tank;
    else if (wave >= 2 && r < 0.50f) type = EnemyType::Flanker;
    else if (             r < 0.70f) type = EnemyType::Basic;
    else                             type = EnemyType::Fast;

    enemies.emplace_back(spawnPos, type);
    enemiesSpawned++;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bomb
// ─────────────────────────────────────────────────────────────────────────────

void Game::ApplyBomb() {
    // Damage enemies in radius
    for (auto& e : enemies) {
        if (Vector3Distance(e.pos, player.pos) < BOMB_RADIUS) {
            e.TakeDamage(999);
        }
    }
    // Clear enemy bullets in radius
    for (auto& b : enemyBullets) {
        if (Vector3Distance(b.pos, player.pos) < BOMB_RADIUS) {
            b.active = false;
        }
    }
    // Visual shockwave
    BombEffect bf;
    bf.pos       = player.pos;
    bf.radius    = 0.0f;
    bf.maxRadius = BOMB_RADIUS;
    bf.duration  = 0.6f;
    bf.timer     = bf.duration;
    bf.active    = true;
    bombEffects.push_back(bf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Collision detection
// ─────────────────────────────────────────────────────────────────────────────

void Game::CheckCollisions() {
    // Player bullets vs enemies
    for (auto& b : playerBullets) {
        if (!b.active) continue;
        for (auto& e : enemies) {
            if (!e.IsAlive()) continue;
            if (Vector3Distance(b.pos, e.pos) < e.collisionRadius + b.radius) {
                e.TakeDamage(b.damage);
                b.active = false;
                break;
            }
        }
    }

    // Enemy bullets vs player
    for (auto& b : enemyBullets) {
        if (!b.active) continue;
        if (Vector3Distance(b.pos, player.pos) < 0.9f + b.radius) {
            player.TakeDamage(b.damage);
            b.active = false;
        }
    }

    // Enemy body vs player (ram)
    for (auto& e : enemies) {
        if (!e.IsAlive()) continue;
        float dist = Vector3Distance(e.pos, player.pos);
        float minDist = e.collisionRadius + 0.8f;
        if (dist < minDist) {
            player.TakeDamage(1);
            Vector3 dir = (dist < 0.001f)
                ? Vector3{1.0f, 0.0f, 0.0f}
                : Vector3Normalize(Vector3Subtract(e.pos, player.pos));
            e.pos = Vector3Add(player.pos, Vector3Scale(dir, minDist + 0.05f));
        }
    }

    // Bullets vs obstacles
    auto blocksBullet = [&](Bullet& b) {
        if (!b.active) return;
        for (const auto& obs : obstacles) {
            BoundingBox bb = {
                {obs.pos.x - obs.halfSize.x, obs.pos.y - obs.halfSize.y, obs.pos.z - obs.halfSize.z},
                {obs.pos.x + obs.halfSize.x, obs.pos.y + obs.halfSize.y, obs.pos.z + obs.halfSize.z}
            };
            if (CheckCollisionBoxSphere(bb, b.pos, b.radius)) {
                b.active = false;
                return;
            }
        }
    };

    for (auto& b : playerBullets) blocksBullet(b);
    for (auto& b : enemyBullets)  blocksBullet(b);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Camera — classic isometric angle
// ─────────────────────────────────────────────────────────────────────────────

void Game::UpdateCamera() {
    // Equal offset on X/Y/Z gives true isometric elevation (~35.26°)
    const float D = 32.0f;
    camera.position  = {player.pos.x + D, D, player.pos.z + D};
    camera.target    = {player.pos.x,     0.0f, player.pos.z};
    camera.up        = {0.0f, 1.0f, 0.0f};
    camera.fovy      = 28.0f;                 // ortho view height in world units
    camera.projection = CAMERA_ORTHOGRAPHIC;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Draw
// ─────────────────────────────────────────────────────────────────────────────

void Game::Draw() {
    BeginDrawing();
    ClearBackground({8, 16, 36, 255});

    BeginMode3D(camera);

        DrawTerrain();
        DrawObstacles();

        // Bomb shockwave rings
        DrawBombEffects();

        // Entities
        player.Draw();
        for (const auto& e : enemies)       e.Draw();
        for (const auto& b : playerBullets) b.Draw();
        for (const auto& b : enemyBullets)  b.Draw();

        // Arena boundary markers
        Color boundCol = {80, 80, 200, 180};
        DrawLine3D({-ARENA_SIZE, 0.05f, -ARENA_SIZE}, { ARENA_SIZE, 0.05f, -ARENA_SIZE}, boundCol);
        DrawLine3D({ ARENA_SIZE, 0.05f, -ARENA_SIZE}, { ARENA_SIZE, 0.05f,  ARENA_SIZE}, boundCol);
        DrawLine3D({ ARENA_SIZE, 0.05f,  ARENA_SIZE}, {-ARENA_SIZE, 0.05f,  ARENA_SIZE}, boundCol);
        DrawLine3D({-ARENA_SIZE, 0.05f,  ARENA_SIZE}, {-ARENA_SIZE, 0.05f, -ARENA_SIZE}, boundCol);

    EndMode3D();

    DrawHUD();
    EndDrawing();
}

void Game::DrawTerrain() {
    // Ground plane
    DrawPlane({0.0f, 0.0f, 0.0f}, {ARENA_SIZE * 2.0f, ARENA_SIZE * 2.0f}, {28, 55, 28, 255});

    // Grid
    Color gridCol = {42, 72, 42, 255};
    for (int i = -(int)ARENA_SIZE; i <= (int)ARENA_SIZE; i += 5) {
        DrawLine3D({(float)i, 0.02f, -ARENA_SIZE}, {(float)i, 0.02f, ARENA_SIZE}, gridCol);
        DrawLine3D({-ARENA_SIZE, 0.02f, (float)i}, {ARENA_SIZE, 0.02f, (float)i}, gridCol);
    }
}

void Game::DrawObstacles() {
    for (const auto& obs : obstacles) {
        float w = obs.halfSize.x * 2.0f;
        float h = obs.halfSize.y * 2.0f;
        float d = obs.halfSize.z * 2.0f;
        DrawCube(obs.pos, w, h, d, obs.color);
        Color wireCol = {(unsigned char)(obs.color.r / 2),
                         (unsigned char)(obs.color.g / 2),
                         (unsigned char)(obs.color.b / 2), 255};
        DrawCubeWires(obs.pos, w, h, d, wireCol);
    }
}

void Game::DrawBombEffects() {
    for (const auto& bf : bombEffects) {
        if (!bf.active) continue;
        float alpha = bf.timer / bf.duration;
        Color ringCol = {255, 200, 50, (unsigned char)(200 * alpha)};
        // Draw as stacked rings
        for (int ring = 0; ring < 3; ++ring) {
            float r = bf.radius - ring * 0.8f;
            if (r > 0.0f) {
                DrawCircle3D({bf.pos.x, 0.3f + ring * 0.4f, bf.pos.z},
                             r, {1.0f, 0.0f, 0.0f}, 90.0f, ringCol);
            }
        }
    }
}

void Game::DrawHUD() {
    // ── HP bar ─────────────────────────────────────────────────────────────────
    const int barW = 200, barH = 22;
    DrawRectangle(20, 20, barW, barH, DARKGRAY);
    int filledW = (int)(barW * (float)player.hp / player.maxHp);
    Color hpCol = (player.hp > player.maxHp / 2) ? GREEN
                : (player.hp > 1)                ? YELLOW
                                                 : RED;
    DrawRectangle(20, 20, filledW, barH, hpCol);
    DrawRectangleLines(20, 20, barW, barH, WHITE);
    DrawText(TextFormat("HP  %d / %d", player.hp, player.maxHp), 28, 23, 15, WHITE);

    // ── Bombs ──────────────────────────────────────────────────────────────────
    DrawText("BOMB", 20, 50, 14, GRAY);
    for (int i = 0; i < player.maxBombs; ++i) {
        Color bombCol = (i < player.bombs) ? GOLD : DARKGRAY;
        DrawRectangle(60 + i * 22, 48, 18, 18, bombCol);
        DrawRectangleLines(60 + i * 22, 48, 18, 18, WHITE);
    }

    // ── Score / Wave / Enemies ────────────────────────────────────────────────
    DrawText(TextFormat("SCORE  %06d", score),               SCREEN_W - 220, 20, 20, WHITE);
    DrawText(TextFormat("WAVE   %d",   wave),                SCREEN_W - 220, 46, 18, ORANGE);
    int remaining = (enemiesPerWave - enemiesSpawned) + (int)enemies.size();
    DrawText(TextFormat("ENEMIES %d",  remaining),           SCREEN_W - 220, 70, 18, RED);

    // ── Enemy HP bars (world → screen) ────────────────────────────────────────
    for (const auto& e : enemies) {
        if (!e.IsAlive()) continue;
        Vector3 aboveEnemy = {e.pos.x, e.pos.y + 2.5f, e.pos.z};
        Vector2 sp = GetWorldToScreen(aboveEnemy, camera);
        int bw = 40;
        int bx = (int)sp.x - bw / 2;
        int by = (int)sp.y - 6;
        DrawRectangle(bx, by, bw, 5, DARKGRAY);
        DrawRectangle(bx, by, (int)(bw * (float)e.hp / e.maxHp), 5, LIME);
    }

    // ── Controls hint ──────────────────────────────────────────────────────────
    DrawText("WASD Move   Z Primary   X Spread   C Bomb", 20, SCREEN_H - 26, 15, LIGHTGRAY);

    // ── Game Over overlay ──────────────────────────────────────────────────────
    if (gameOver) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});
        const char* goText = "GAME OVER";
        int tw = MeasureText(goText, 72);
        DrawText(goText, SCREEN_W / 2 - tw / 2, SCREEN_H / 2 - 70, 72, RED);

        const char* scoreText = TextFormat("Final Score: %d", score);
        int sw = MeasureText(scoreText, 32);
        DrawText(scoreText, SCREEN_W / 2 - sw / 2, SCREEN_H / 2 + 16, 32, WHITE);

        const char* restartText = "Press  R  to restart";
        int rw = MeasureText(restartText, 24);
        DrawText(restartText, SCREEN_W / 2 - rw / 2, SCREEN_H / 2 + 62, 24, LIGHTGRAY);
    }
}
