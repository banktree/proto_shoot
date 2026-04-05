#include "game.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

Game::Game()
    : score(0), wave(1),
      scrollSpeed(SCROLL_DEFAULT),
      worldGenX(0.0f),
      enemySpawnTimer(2.0f), enemySpawnInterval(3.0f),
      gameOver(false)
{
    InitWindow(SCREEN_W, SCREEN_H, "Proto Shoot — Zaxxon Style");
    SetTargetFPS(60);
    rng.seed(42);

    worldGenX = player.pos.x;
    GenerateAhead();
    UpdateCamera();
}

Game::~Game() {
    CloseWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Run / Reset
// ─────────────────────────────────────────────────────────────────────────────

void Game::Run() {
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (!gameOver) Update(dt);
        else if (IsKeyPressed(KEY_R)) Reset();
        Draw();
    }
}

void Game::Reset() {
    player = Player();
    enemies.clear();
    playerBullets.clear();
    enemyBullets.clear();
    walls.clear();
    buildings.clear();
    bombEffects.clear();
    score       = 0;
    wave        = 1;
    scrollSpeed = SCROLL_DEFAULT;
    worldGenX   = player.pos.x;
    enemySpawnTimer    = 2.0f;
    enemySpawnInterval = 3.0f;
    gameOver = false;
    GenerateAhead();
    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Procedural terrain generation (Zaxxon-style walls + buildings)
// ─────────────────────────────────────────────────────────────────────────────

void Game::GenerateAhead() {
    std::uniform_real_distribution<float> zDist(-ARENA_Z_HALF + 2.5f, ARENA_Z_HALF - 2.5f);
    std::uniform_real_distribution<float> gapBotDist(1.5f, 6.0f);
    std::uniform_real_distribution<float> gapSizeDist(2.8f, 4.5f);
    std::uniform_real_distribution<float> bwDist(1.0f, 3.5f);
    std::uniform_real_distribution<float> bhDist(0.8f, 3.2f);
    std::uniform_int_distribution<int>    wallChance(0, 4);
    std::uniform_int_distribution<int>    numBldg(1, 3);

    float genTarget = player.pos.x + GEN_LOOKAHEAD;

    while (worldGenX < genTarget) {
        worldGenX += SECTION_LEN;

        // ── Wall (roughly every 4-5 sections) ────────────────────────────────
        if (wallChance(rng) == 0) {
            Wall w;
            w.x         = worldGenX;
            w.gapBottom = gapBotDist(rng);
            w.gapTop    = w.gapBottom + gapSizeDist(rng);
            if (w.gapTop > WALL_HEIGHT - 0.5f) w.gapTop = WALL_HEIGHT - 0.5f;
            w.passed = false;
            walls.push_back(w);
        }

        // ── Buildings (ground-level hazards / scenery) ────────────────────────
        int n = numBldg(rng);
        for (int i = 0; i < n; ++i) {
            float bw = bwDist(rng);
            float bh = bhDist(rng);
            float bd = bwDist(rng);
            Building b;
            b.pos      = {worldGenX + zDist(rng) * 0.25f, bh * 0.5f, zDist(rng)};
            b.halfSize = {bw * 0.5f, bh * 0.5f, bd * 0.5f};
            unsigned char cr = (unsigned char)(80 + (int)(bh * 18));
            unsigned char cg = (unsigned char)(70 + (int)(bw * 10));
            b.color = {cr, cg, 50, 255};
            buildings.push_back(b);
        }
    }

    // ── Cleanup old content behind the player ─────────────────────────────────
    float cutX = player.pos.x - DESPAWN_BEHIND;
    walls.erase(std::remove_if(walls.begin(), walls.end(),
        [cutX](const Wall& w) { return w.x < cutX; }), walls.end());
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(),
        [cutX](const Building& b) { return b.pos.x + b.halfSize.x < cutX; }), buildings.end());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void Game::Update(float dt) {
    player.Update(dt, playerBullets, scrollSpeed);

    if (player.usedBomb) ApplyBomb();

    GenerateAhead();

    // Enemy spawning
    enemySpawnTimer -= dt;
    if (enemySpawnTimer <= 0.0f) {
        SpawnEnemy();
        enemySpawnTimer = enemySpawnInterval;
    }

    // Update enemies
    for (auto& e : enemies) {
        e.Update(dt, player.pos, enemyBullets);
    }

    // Update bullets
    for (auto& b : playerBullets) b.Update(dt);
    for (auto& b : enemyBullets)  b.Update(dt);

    // Bomb effects
    for (auto& bf : bombEffects) {
        if (!bf.active) continue;
        bf.timer  -= dt;
        bf.radius  = bf.maxRadius * (1.0f - bf.timer / bf.duration);
        if (bf.timer <= 0.0f) bf.active = false;
    }

    CheckCollisions();

    // Score for enemies killed THIS frame
    for (const auto& e : enemies) {
        if (!e.IsAlive()) score += e.scoreValue;
    }

    // Remove dead enemies + enemies that flew past the player
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [&](const Enemy& e) {
            return !e.IsAlive() || (e.pos.x < player.pos.x - DESPAWN_BEHIND);
        }), enemies.end());

    playerBullets.erase(std::remove_if(playerBullets.begin(), playerBullets.end(),
        [](const Bullet& b) { return !b.active; }), playerBullets.end());
    enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(),
        [](const Bullet& b) { return !b.active; }), enemyBullets.end());
    bombEffects.erase(std::remove_if(bombEffects.begin(), bombEffects.end(),
        [](const BombEffect& bf) { return !bf.active; }), bombEffects.end());

    // Spacebar: hold to boost, release to return to default
    scrollSpeed = IsKeyDown(KEY_SPACE) ? SCROLL_BOOST : SCROLL_DEFAULT;

    // Wave: score threshold
    wave = 1 + (int)(score / 2000);

    if (!player.IsAlive()) gameOver = true;

    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enemy spawning
// ─────────────────────────────────────────────────────────────────────────────

void Game::SpawnEnemy() {
    std::uniform_real_distribution<float> zDist(-ARENA_Z_HALF + 2.0f, ARENA_Z_HALF - 2.0f);
    std::uniform_real_distribution<float> yDist(0.5f, 8.0f);
    std::uniform_real_distribution<float> typeDist(0.0f, 1.0f);

    float spawnX = player.pos.x + ENEMY_SPAWN_X;
    float spawnZ = zDist(rng);
    float r = typeDist(rng);

    EnemyType type;
    if      (wave >= 3 && r < 0.20f) type = EnemyType::Flanker;
    else if (wave >= 2 && r < 0.35f) type = EnemyType::Turret;
    else if (             r < 0.15f) type = EnemyType::Kamikaze;
    else                             type = EnemyType::Fighter;

    float spawnY = (type == EnemyType::Turret) ? 0.5f : yDist(rng);

    enemies.emplace_back(Vector3{spawnX, spawnY, spawnZ}, type);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bomb
// ─────────────────────────────────────────────────────────────────────────────

void Game::ApplyBomb() {
    for (auto& e : enemies) {
        if (Vector3Distance(e.pos, player.pos) < BOMB_RADIUS)
            e.TakeDamage(999);
    }
    for (auto& b : enemyBullets) {
        if (Vector3Distance(b.pos, player.pos) < BOMB_RADIUS)
            b.active = false;
    }
    BombEffect bf;
    bf.pos       = player.pos;
    bf.radius    = 0.0f;
    bf.maxRadius = BOMB_RADIUS;
    bf.duration  = bf.timer = 0.6f;
    bf.active    = true;
    bombEffects.push_back(bf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Collision detection
// ─────────────────────────────────────────────────────────────────────────────

void Game::CheckCollisions() {
    // ── Player bullets vs enemies ─────────────────────────────────────────────
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

    // ── Enemy bullets vs player ───────────────────────────────────────────────
    for (auto& b : enemyBullets) {
        if (!b.active) continue;
        if (Vector3Distance(b.pos, player.pos) < 0.9f + b.radius) {
            player.TakeDamage(b.damage);
            b.active = false;
        }
    }

    // ── Enemy body vs player (ram) ────────────────────────────────────────────
    for (auto& e : enemies) {
        if (!e.IsAlive()) continue;
        float minDist = e.collisionRadius + 0.8f;
        if (Vector3Distance(e.pos, player.pos) < minDist)
            player.TakeDamage(1);
    }

    // ── Player vs walls (core Zaxxon mechanic) ────────────────────────────────
    for (auto& w : walls) {
        float dx = fabsf(player.pos.x - w.x);
        if (dx < WALL_THICKNESS * 0.5f + 0.9f) {
            bool inGap = (player.pos.y >= w.gapBottom && player.pos.y <= w.gapTop);
            if (!inGap) {
                player.TakeDamage(1);
            } else if (!w.passed && player.pos.x >= w.x) {
                w.passed = true;
                score += 50;   // bonus for clean wall pass
            }
        }
    }

    // ── Player vs buildings ───────────────────────────────────────────────────
    for (const auto& bld : buildings) {
        BoundingBox bb = {
            {bld.pos.x - bld.halfSize.x, bld.pos.y - bld.halfSize.y, bld.pos.z - bld.halfSize.z},
            {bld.pos.x + bld.halfSize.x, bld.pos.y + bld.halfSize.y, bld.pos.z + bld.halfSize.z}
        };
        if (CheckCollisionBoxSphere(bb, player.pos, 0.8f))
            player.TakeDamage(1);
    }

    // ── Bullets vs walls ──────────────────────────────────────────────────────
    auto blockByWalls = [&](Bullet& b) {
        if (!b.active) return;
        for (const auto& w : walls) {
            if (fabsf(b.pos.x - w.x) < WALL_THICKNESS * 0.5f + b.radius) {
                bool inGap = (b.pos.y >= w.gapBottom && b.pos.y <= w.gapTop);
                if (!inGap) { b.active = false; return; }
            }
        }
    };
    for (auto& b : playerBullets) blockByWalls(b);
    for (auto& b : enemyBullets)  blockByWalls(b);

    // ── Player bullets vs buildings ───────────────────────────────────────────
    for (auto& b : playerBullets) {
        if (!b.active) continue;
        for (const auto& bld : buildings) {
            BoundingBox bb = {
                {bld.pos.x - bld.halfSize.x, bld.pos.y - bld.halfSize.y, bld.pos.z - bld.halfSize.z},
                {bld.pos.x + bld.halfSize.x, bld.pos.y + bld.halfSize.y, bld.pos.z + bld.halfSize.z}
            };
            if (CheckCollisionBoxSphere(bb, b.pos, b.radius)) {
                b.active = false;
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Camera — isometric, follows player X & Z (Zaxxon angle)
// ─────────────────────────────────────────────────────────────────────────────

void Game::UpdateCamera() {
    // Camera behind (-X) and above/right (+Z) the player so that:
    //   +X world (forward / scroll direction) → upper-right on screen  (Zaxxon)
    //   +Z world (right strafe)               → lower-right on screen
    //   +Y world (altitude)                   → up on screen
    const float D          = 32.0f;
    const float LOOK_AHEAD = 15.0f;   // more look-ahead for portrait (player lower-left)
    camera.position   = {player.pos.x - D, D, player.pos.z + D};
    camera.target     = {player.pos.x + LOOK_AHEAD, 0.0f, player.pos.z};
    camera.up         = {0.0f, 1.0f, 0.0f};
    // Portrait 720x1080 (2:3): fovy sets world-unit height; width = fovy*(720/1080)=fovy*0.67
    camera.fovy       = 36.0f;
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
        DrawBuildings();
        DrawWalls();
        DrawBombEffects();
        player.Draw();
        for (const auto& e : enemies)       e.Draw();
        for (const auto& b : playerBullets) b.Draw();
        for (const auto& b : enemyBullets)  b.Draw();
    EndMode3D();

    DrawHUD();
    EndDrawing();
}

void Game::DrawTerrain() {
    // Scrolling ground plane (wide enough to fill the view)
    const float GW = 160.0f;
    DrawPlane({player.pos.x, 0.0f, 0.0f},
              {GW, (ARENA_Z_HALF + 6.0f) * 2.0f}, {28, 55, 28, 255});

    // Grid lines
    Color gc = {42, 72, 42, 255};
    float x0 = floorf((player.pos.x - 70.0f) / 5.0f) * 5.0f;
    float x1 = player.pos.x + 70.0f;
    for (float x = x0; x < x1; x += 5.0f)
        DrawLine3D({x, 0.02f, -ARENA_Z_HALF}, {x, 0.02f, ARENA_Z_HALF}, gc);
    for (float z = -ARENA_Z_HALF; z <= ARENA_Z_HALF; z += 5.0f)
        DrawLine3D({x0, 0.02f, z}, {x1, 0.02f, z}, gc);

    // Side boundary lines
    Color bc = {80, 80, 200, 180};
    DrawLine3D({x0, 0.05f, -ARENA_Z_HALF}, {x1, 0.05f, -ARENA_Z_HALF}, bc);
    DrawLine3D({x0, 0.05f,  ARENA_Z_HALF}, {x1, 0.05f,  ARENA_Z_HALF}, bc);
}

void Game::DrawWalls() {
    Color wallCol = {110, 115, 125, 255};
    Color wireCol = { 60,  65,  75, 255};
    const float halfZ = ARENA_Z_HALF * 2.0f;

    for (const auto& w : walls) {
        // Bottom section: ground → gapBottom
        if (w.gapBottom > 0.05f) {
            float h   = w.gapBottom;
            Vector3 p = {w.x, h * 0.5f, 0.0f};
            DrawCube(p, WALL_THICKNESS, h, halfZ, wallCol);
            DrawCubeWires(p, WALL_THICKNESS, h, halfZ, wireCol);
        }
        // Top section: gapTop → WALL_HEIGHT
        float topH = WALL_HEIGHT - w.gapTop;
        if (topH > 0.05f) {
            Vector3 p = {w.x, w.gapTop + topH * 0.5f, 0.0f};
            DrawCube(p, WALL_THICKNESS, topH, halfZ, wallCol);
            DrawCubeWires(p, WALL_THICKNESS, topH, halfZ, wireCol);
        }
        // Gap highlight line (shows the opening altitude)
        Color gapCol = {200, 200, 80, 160};
        DrawLine3D({w.x, w.gapBottom, -ARENA_Z_HALF}, {w.x, w.gapBottom, ARENA_Z_HALF}, gapCol);
        DrawLine3D({w.x, w.gapTop,    -ARENA_Z_HALF}, {w.x, w.gapTop,    ARENA_Z_HALF}, gapCol);
    }
}

void Game::DrawBuildings() {
    for (const auto& b : buildings) {
        float w = b.halfSize.x * 2.0f;
        float h = b.halfSize.y * 2.0f;
        float d = b.halfSize.z * 2.0f;
        DrawCube(b.pos, w, h, d, b.color);
        Color wc = {(unsigned char)(b.color.r / 2),
                    (unsigned char)(b.color.g / 2),
                    (unsigned char)(b.color.b / 2), 255};
        DrawCubeWires(b.pos, w, h, d, wc);
    }
}

void Game::DrawBombEffects() {
    for (const auto& bf : bombEffects) {
        if (!bf.active) continue;
        float alpha = bf.timer / bf.duration;
        Color rc = {255, 220, 60, (unsigned char)(200 * alpha)};
        for (int ring = 0; ring < 3; ++ring) {
            float r = bf.radius - ring * 1.0f;
            if (r > 0.0f)
                DrawCircle3D({bf.pos.x, 0.5f + ring * 0.6f, bf.pos.z},
                             r, {1.0f, 0.0f, 0.0f}, 90.0f, rc);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  HUD
// ─────────────────────────────────────────────────────────────────────────────

void Game::DrawHUD() {
    // ── HP bar (top-left) ─────────────────────────────────────────────────────
    DrawRectangle(20, 20, 200, 22, DARKGRAY);
    int fw = (int)(200 * (float)player.hp / player.maxHp);
    Color hpCol = (player.hp > player.maxHp / 2) ? GREEN
                : (player.hp > 1)                ? YELLOW : RED;
    DrawRectangle(20, 20, fw, 22, hpCol);
    DrawRectangleLines(20, 20, 200, 22, WHITE);
    DrawText(TextFormat("HP  %d / %d", player.hp, player.maxHp), 28, 23, 15, WHITE);

    // ── Bomb stock ────────────────────────────────────────────────────────────
    DrawText("BOMB", 20, 50, 14, GRAY);
    for (int i = 0; i < player.maxBombs; ++i) {
        Color bc = (i < player.bombs) ? GOLD : DARKGRAY;
        DrawRectangle(60 + i * 22, 48, 18, 18, bc);
        DrawRectangleLines(60 + i * 22, 48, 18, 18, WHITE);
    }

    // ── Score / Wave (top-right) ──────────────────────────────────────────────
    bool boosting = IsKeyDown(KEY_SPACE);
    DrawText(TextFormat("SCORE %06d", score), SCREEN_W - 200, 20, 20, WHITE);
    DrawText(TextFormat("WAVE  %d",   wave),  SCREEN_W - 200, 46, 18, ORANGE);
    DrawText(boosting ? "BOOST!" : "NORMAL",  SCREEN_W - 200, 70, 16,
             boosting ? YELLOW : LIGHTGRAY);

    // ── Enemy HP bars (world → screen) ───────────────────────────────────────
    for (const auto& e : enemies) {
        if (!e.IsAlive()) continue;
        Vector2 sp = GetWorldToScreen({e.pos.x, e.pos.y + 2.5f, e.pos.z}, camera);
        if (sp.x < 0 || sp.x > SCREEN_W || sp.y < 0 || sp.y > SCREEN_H) continue;
        int bw = 36, bx = (int)sp.x - bw / 2, by = (int)sp.y - 6;
        DrawRectangle(bx, by, bw, 5, DARKGRAY);
        DrawRectangle(bx, by, (int)(bw * (float)e.hp / e.maxHp), 5, LIME);
    }

    // ══ Center dual gauge: [WALL GAP] | [ALTITUDE] ═══════════════════════════
    //   Both bars share the same altitude scale: ALT_MIN(0.5) → ALT_MAX(10.0)
    const float ALT_MIN  = 0.5f, ALT_MAX = 10.0f;
    const float altRange = ALT_MAX - ALT_MIN;
    const int   GH = 200;   // gauge height in pixels
    const int   GW = 22;    // gauge width
    const int   GAP = 10;   // gap between the two bars
    // Center the pair horizontally
    const int   pairW  = GW * 2 + GAP;
    const int   leftX  = SCREEN_W / 2 - pairW / 2;       // WALL gauge
    const int   rightX = leftX + GW + GAP;                // ALT gauge
    const int   gaugeY = SCREEN_H - GH - 44;

    // ── Find nearest upcoming wall ────────────────────────────────────────────
    const Wall* nextWall = nullptr;
    float       wallDist = 1e9f;
    for (const auto& w : walls) {
        float d = w.x - player.pos.x;
        if (d > 0.0f && d < wallDist) { wallDist = d; nextWall = &w; }
    }

    // ── LEFT bar: Wall gap gauge ──────────────────────────────────────────────
    DrawRectangle(leftX, gaugeY, GW, GH, DARKGRAY);

    if (nextWall) {
        // Danger (red) — bottom zone: ALT_MIN → gapBottom
        float botRatio = Clamp((nextWall->gapBottom - ALT_MIN) / altRange, 0.0f, 1.0f);
        int   botPx    = (int)(GH * botRatio);
        if (botPx > 0)
            DrawRectangle(leftX, gaugeY + GH - botPx, GW, botPx, RED);

        // Safe (green) — gap zone: gapBottom → gapTop
        float gapBotR = Clamp((nextWall->gapBottom - ALT_MIN) / altRange, 0.0f, 1.0f);
        float gapTopR = Clamp((nextWall->gapTop    - ALT_MIN) / altRange, 0.0f, 1.0f);
        int   gapY1   = gaugeY + GH - (int)(GH * gapTopR);
        int   gapY2   = gaugeY + GH - (int)(GH * gapBotR);
        if (gapY2 > gapY1)
            DrawRectangle(leftX, gapY1, GW, gapY2 - gapY1, {0, 200, 0, 255});

        // Danger (red) — top zone: gapTop → ALT_MAX
        float topRatio = Clamp((ALT_MAX - nextWall->gapTop) / altRange, 0.0f, 1.0f);
        int   topPx    = (int)(GH * topRatio);
        if (topPx > 0)
            DrawRectangle(leftX, gaugeY, GW, topPx, RED);

        // Distance label — colour changes as wall approaches
        Color dc = (wallDist < 12.0f) ? RED : (wallDist < 25.0f) ? YELLOW : WHITE;
        DrawText(TextFormat("WALL"), leftX + 1, gaugeY - 28, 13, dc);
        DrawText(TextFormat("%.0f", wallDist), leftX + 1, gaugeY - 14, 13, dc);
    } else {
        // No wall coming — all clear
        DrawRectangle(leftX, gaugeY, GW, GH, {0, 90, 0, 255});
        DrawText("CLEAR", leftX - 4, gaugeY - 14, 12, GREEN);
    }

    // Player altitude line on wall gauge
    float playerR    = Clamp((player.pos.y - ALT_MIN) / altRange, 0.0f, 1.0f);
    int   playerLineY = gaugeY + GH - (int)(GH * playerR);
    DrawRectangle(leftX - 4, playerLineY - 2, GW + 8, 4, WHITE);

    DrawRectangleLines(leftX, gaugeY, GW, GH, WHITE);
    DrawText("WALL", leftX, gaugeY + GH + 5, 12, GRAY);

    // ── RIGHT bar: Player altitude meter ─────────────────────────────────────
    DrawRectangle(rightX, gaugeY, GW, GH, DARKGRAY);

    // Altitude fill
    int altFill = (int)(GH * Clamp(playerR, 0.0f, 1.0f));
    DrawRectangle(rightX, gaugeY + GH - altFill, GW, altFill, SKYBLUE);

    // Tick marks: every 1 unit minor, every 2 units major with label
    for (int a = 1; a <= (int)ALT_MAX; ++a) {
        float r      = (a - ALT_MIN) / altRange;
        int   ty     = gaugeY + GH - (int)(GH * r);
        bool  major  = (a % 2 == 0);
        int   tLen   = major ? 7 : 4;
        Color tCol   = major ? WHITE : LIGHTGRAY;
        // ticks on BOTH sides
        DrawLine(rightX - tLen, ty, rightX,       ty, tCol);
        DrawLine(rightX + GW,   ty, rightX+GW+tLen, ty, tCol);
        if (major)
            DrawText(TextFormat("%d", a), rightX + GW + tLen + 2, ty - 7, 12, LIGHTGRAY);
    }

    DrawRectangleLines(rightX, gaugeY, GW, GH, WHITE);
    DrawText("ALT",                         rightX + 1,  gaugeY + GH + 5,  12, SKYBLUE);
    DrawText(TextFormat("%.1f", player.pos.y), rightX - 6, gaugeY + GH + 18, 12, SKYBLUE);

    // ── Controls (bottom-left) ────────────────────────────────────────────────
    DrawText("Arrows: Move/Alt   Z: Shot   X: Spread   C: Bomb   SPACE: Boost",
             20, SCREEN_H - 26, 15, LIGHTGRAY);

    // ── Game Over overlay ─────────────────────────────────────────────────────
    if (gameOver) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});
        const char* goText = "GAME OVER";
        int tw = MeasureText(goText, 72);
        DrawText(goText, SCREEN_W / 2 - tw / 2, SCREEN_H / 2 - 70, 72, RED);
        const char* st = TextFormat("Final Score: %d", score);
        int sw = MeasureText(st, 32);
        DrawText(st, SCREEN_W / 2 - sw / 2, SCREEN_H / 2 + 16, 32, WHITE);
        const char* rt = "Press  R  to restart";
        int rw = MeasureText(rt, 24);
        DrawText(rt, SCREEN_W / 2 - rw / 2, SCREEN_H / 2 + 62, 24, LIGHTGRAY);
    }
}
