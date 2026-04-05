#include "game.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

Game::Game()
    : score(0), currentStage(1),
      scrollSpeed(SCROLL_DEFAULT),
      worldGenX(0.0f),
      stageStartX(0.0f), stageClearTimer(0.0f),
      gameOver(false)
{
    InitWindow(SCREEN_W, SCREEN_H, "Proto Shoot — Zaxxon Style");
    SetTargetFPS(60);
    rng.seed(42);

    stageStartX = player.pos.x;
    worldGenX   = player.pos.x;
    GenerateAhead();
    BeginStage(currentStage);
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
    buildings.clear();
    bombEffects.clear();
    spawnQueue.clear();
    score           = 0;
    currentStage    = 1;
    scrollSpeed     = SCROLL_DEFAULT;
    stageStartX     = player.pos.x;
    worldGenX       = player.pos.x;
    stageClearTimer = 0.0f;
    gameOver        = false;
    GenerateAhead();
    BeginStage(currentStage);
    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stage system
// ─────────────────────────────────────────────────────────────────────────────

void Game::BeginStage(int stage) {
    spawnQueue.clear();

    // Use a deterministic RNG seeded per stage so layouts are reproducible
    std::mt19937 srng((unsigned)stage * 7919u + 13u);
    std::uniform_real_distribution<float> zDist(-ARENA_Z_HALF + 2.5f, ARENA_Z_HALF - 2.5f);
    std::uniform_real_distribution<float> yDist(2.0f, 7.5f);

    float len = STAGE_LENGTH;

    // ── Turrets (ground) — first 70% of stage ────────────────────────────────
    int numTurrets = 3 + (stage - 1) * 2;
    float tStep = len * 0.65f / numTurrets;
    for (int i = 0; i < numTurrets; ++i) {
        float relX = 40.0f + i * tStep;
        spawnQueue.push_back({relX, EnemyType::Turret, zDist(srng), 0.6f, false});
    }

    // ── Fighters (air) — spread across first 75% ─────────────────────────────
    int numFighters = 3 + (stage - 1) * 2;
    float fStep = len * 0.70f / numFighters;
    for (int i = 0; i < numFighters; ++i) {
        float relX = 20.0f + i * fStep;
        spawnQueue.push_back({relX, EnemyType::Fighter, zDist(srng), yDist(srng), false});
    }

    // ── Flankers from stage 2 ─────────────────────────────────────────────────
    if (stage >= 2) {
        int numFlankers = stage - 1;
        float flStep = len * 0.55f / numFlankers;
        for (int i = 0; i < numFlankers; ++i) {
            float relX = 60.0f + i * flStep;
            spawnQueue.push_back({relX, EnemyType::Flanker, zDist(srng), yDist(srng), false});
        }
    }

    // ── Boss — near end of stage ──────────────────────────────────────────────
    spawnQueue.push_back({len - 60.0f, EnemyType::Boss, 0.0f, 4.0f, false});

    // Sort by relX so triggers fire in order
    std::sort(spawnQueue.begin(), spawnQueue.end(),
              [](const SpawnTrigger& a, const SpawnTrigger& b){
                  return a.relX < b.relX;
              });
}

void Game::UpdateStage(float dt) {
    // During stage-clear countdown, just tick and then advance stage
    if (stageClearTimer > 0.0f) {
        stageClearTimer -= dt;
        if (stageClearTimer <= 0.0f) {
            stageClearTimer = 0.0f;
            currentStage++;
            stageStartX = player.pos.x;
            BeginStage(currentStage);
        }
        return;
    }

    float progress = player.pos.x - stageStartX;

    // Fire triggers as player advances through the stage
    for (auto& trig : spawnQueue) {
        if (!trig.fired && progress >= trig.relX) {
            trig.fired = true;
            // Boss spawns further ahead so player has room to react
            float spawnX = player.pos.x +
                           (trig.type == EnemyType::Boss ? ENEMY_SPAWN_X * 2.5f : ENEMY_SPAWN_X);
            enemies.emplace_back(Vector3{spawnX, trig.y, trig.z}, trig.type);
        }
    }

    // Stage clear: all triggers fired AND no boss alive
    bool allFired = true;
    for (const auto& t : spawnQueue)
        if (!t.fired) { allFired = false; break; }

    bool bossAlive = false;
    for (const auto& e : enemies)
        if (e.type == EnemyType::Boss && e.IsAlive()) { bossAlive = true; break; }

    if (allFired && !bossAlive && stageClearTimer == 0.0f) {
        stageClearTimer = STAGE_CLEAR_DELAY;
        enemies.clear();        // clear remaining non-boss enemies on stage clear
        enemyBullets.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Procedural terrain generation (buildings only)
// ─────────────────────────────────────────────────────────────────────────────

void Game::GenerateAhead() {
    std::uniform_real_distribution<float> zDist(-ARENA_Z_HALF + 2.5f, ARENA_Z_HALF - 2.5f);
    std::uniform_real_distribution<float> bwDist(1.0f, 3.5f);
    std::uniform_real_distribution<float> bhDist(0.8f, 3.2f);
    std::uniform_int_distribution<int>    numBldg(1, 3);

    float genTarget = player.pos.x + GEN_LOOKAHEAD;

    while (worldGenX < genTarget) {
        worldGenX += SECTION_LEN;
        int n = numBldg(rng);
        for (int i = 0; i < n; ++i) {
            float bw = bwDist(rng), bh = bhDist(rng), bd = bwDist(rng);
            Building b;
            b.pos      = {worldGenX + zDist(rng) * 0.25f, bh * 0.5f, zDist(rng)};
            b.halfSize = {bw * 0.5f, bh * 0.5f, bd * 0.5f};
            unsigned char cr = (unsigned char)(80 + (int)(bh * 18));
            unsigned char cg = (unsigned char)(70 + (int)(bw * 10));
            b.color = {cr, cg, 50, 255};
            buildings.push_back(b);
        }
    }

    float cutX = player.pos.x - DESPAWN_BEHIND;
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(),
        [cutX](const Building& b){ return b.pos.x + b.halfSize.x < cutX; }),
        buildings.end());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update
// ─────────────────────────────────────────────────────────────────────────────

void Game::Update(float dt) {
    player.Update(dt, playerBullets, scrollSpeed);
    if (player.usedBomb) ApplyBomb();

    GenerateAhead();
    UpdateStage(dt);

    // Update enemies
    for (auto& e : enemies)
        e.Update(dt, player.pos, enemyBullets);

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

    // Score dead enemies
    for (const auto& e : enemies)
        if (!e.IsAlive()) score += e.scoreValue;

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [&](const Enemy& e){
            return !e.IsAlive() || (e.pos.x < player.pos.x - DESPAWN_BEHIND);
        }), enemies.end());

    playerBullets.erase(std::remove_if(playerBullets.begin(), playerBullets.end(),
        [](const Bullet& b){ return !b.active; }), playerBullets.end());
    enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(),
        [](const Bullet& b){ return !b.active; }), enemyBullets.end());
    bombEffects.erase(std::remove_if(bombEffects.begin(), bombEffects.end(),
        [](const BombEffect& bf){ return !bf.active; }), bombEffects.end());

    // Stop scrolling during boss fight so player can maneuver in fixed arena
    bool bossOnScreen = false;
    for (const auto& e : enemies)
        if (e.type == EnemyType::Boss && e.IsAlive()) { bossOnScreen = true; break; }

    if (bossOnScreen)
        scrollSpeed = 0.0f;
    else
        scrollSpeed = IsKeyDown(KEY_SPACE) ? SCROLL_BOOST : SCROLL_DEFAULT;

    if (!player.IsAlive()) gameOver = true;

    UpdateCamera();
}

// ─────────────────────────────────────────────────────────────────────────────
//  C-bomb (screen-clearing)
// ─────────────────────────────────────────────────────────────────────────────

void Game::ApplyBomb() {
    for (auto& e : enemies)
        if (Vector3Distance(e.pos, player.pos) < BOMB_RADIUS) e.TakeDamage(999);
    for (auto& b : enemyBullets)
        if (Vector3Distance(b.pos, player.pos) < BOMB_RADIUS) b.active = false;

    BombEffect bf;
    bf.pos = player.pos; bf.radius = 0.0f; bf.maxRadius = BOMB_RADIUS;
    bf.duration = bf.timer = 0.6f; bf.active = true;
    bombEffects.push_back(bf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Collision detection
// ─────────────────────────────────────────────────────────────────────────────

void Game::CheckCollisions() {
    // ── Ground bomb hits ground → splash damage to nearby enemies ────────────
    for (auto& b : playerBullets) {
        if (!b.active || !b.isBomb) continue;
        if (b.pos.y <= 0.15f) {
            b.active = false;
            // Area damage (flat 2-D distance so altitude doesn't shield turrets)
            for (auto& e : enemies) {
                if (!e.IsAlive()) continue;
                float dx = b.pos.x - e.pos.x, dz = b.pos.z - e.pos.z;
                if (sqrtf(dx*dx + dz*dz) < BOMB_SPLASH)
                    e.TakeDamage(b.damage);
            }
            // Explosion visual
            BombEffect bf;
            bf.pos = {b.pos.x, 0.1f, b.pos.z};
            bf.radius = 0.0f; bf.maxRadius = BOMB_SPLASH;
            bf.duration = bf.timer = 0.55f; bf.active = true;
            bombEffects.push_back(bf);
        }
    }

    // ── Player missiles vs enemies ────────────────────────────────────────────
    for (auto& b : playerBullets) {
        if (!b.active || b.isBomb) continue;
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
        if (Vector3Distance(e.pos, player.pos) < e.collisionRadius + 0.8f)
            player.TakeDamage(1);
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
}

// ─────────────────────────────────────────────────────────────────────────────
//  Camera — isometric, Z fixed (Zaxxon angle)
// ─────────────────────────────────────────────────────────────────────────────

void Game::UpdateCamera() {
    const float D          = 32.0f;
    const float LOOK_AHEAD = 15.0f;
    // Z is fixed — camera does NOT follow player's left/right strafe
    camera.position   = {player.pos.x - D, D, D};
    camera.target     = {player.pos.x + LOOK_AHEAD, 0.0f, 0.0f};
    camera.up         = {0.0f, 1.0f, 0.0f};
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
    const float GW = 160.0f;
    DrawPlane({player.pos.x, 0.0f, 0.0f},
              {GW, (ARENA_Z_HALF + 6.0f) * 2.0f}, {28, 55, 28, 255});

    Color gc = {42, 72, 42, 255};
    float x0 = floorf((player.pos.x - 70.0f) / 5.0f) * 5.0f;
    float x1 = player.pos.x + 70.0f;
    for (float x = x0; x < x1; x += 5.0f)
        DrawLine3D({x, 0.02f, -ARENA_Z_HALF}, {x, 0.02f, ARENA_Z_HALF}, gc);
    for (float z = -ARENA_Z_HALF; z <= ARENA_Z_HALF; z += 5.0f)
        DrawLine3D({x0, 0.02f, z}, {x1, 0.02f, z}, gc);

    Color bc = {80, 80, 200, 180};
    DrawLine3D({x0, 0.05f, -ARENA_Z_HALF}, {x1, 0.05f, -ARENA_Z_HALF}, bc);
    DrawLine3D({x0, 0.05f,  ARENA_Z_HALF}, {x1, 0.05f,  ARENA_Z_HALF}, bc);
}

void Game::DrawBuildings() {
    for (const auto& b : buildings) {
        float w = b.halfSize.x * 2.0f, h = b.halfSize.y * 2.0f, d = b.halfSize.z * 2.0f;
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

    // ── Score / Stage (top-right) ─────────────────────────────────────────────
    bool boosting = IsKeyDown(KEY_SPACE);
    DrawText(TextFormat("SCORE %06d", score),    SCREEN_W - 210, 20, 20, WHITE);
    DrawText(TextFormat("STAGE  %d", currentStage), SCREEN_W - 210, 46, 18, ORANGE);
    bool bossOnHUD = false;
    for (const auto& e : enemies)
        if (e.type == EnemyType::Boss && e.IsAlive()) { bossOnHUD = true; break; }

    if (bossOnHUD)
        DrawText("BOSS FIGHT", SCREEN_W - 210, 70, 16, RED);
    else
        DrawText(boosting ? "BOOST!" : "NORMAL", SCREEN_W - 210, 70, 16,
                 boosting ? YELLOW : LIGHTGRAY);

    // ── Stage progress bar (top-right, below stage label) ────────────────────
    float stageProgress = Clamp((player.pos.x - stageStartX) / STAGE_LENGTH, 0.0f, 1.0f);
    DrawRectangle(SCREEN_W - 210, 90, 190, 10, DARKGRAY);
    DrawRectangle(SCREEN_W - 210, 90, (int)(190 * stageProgress), 10, ORANGE);
    DrawRectangleLines(SCREEN_W - 210, 90, 190, 10, GRAY);

    // ── Enemy HP bars (world → screen) ───────────────────────────────────────
    for (const auto& e : enemies) {
        if (!e.IsAlive()) continue;
        Vector2 sp = GetWorldToScreen({e.pos.x, e.pos.y + 2.5f, e.pos.z}, camera);
        if (sp.x < 0 || sp.x > SCREEN_W || sp.y < 0 || sp.y > SCREEN_H) continue;
        int bw = 36, bx = (int)sp.x - bw / 2, by = (int)sp.y - 6;
        DrawRectangle(bx, by, bw, 5, DARKGRAY);
        DrawRectangle(bx, by, (int)(bw * (float)e.hp / e.maxHp), 5, LIME);
    }

    // ══ Center dual gauge: [BOSS HP] | [ALTITUDE] ════════════════════════════
    const float ALT_MIN  = 0.5f, ALT_MAX = 10.0f;
    const float altRange = ALT_MAX - ALT_MIN;
    const int   GH = 200, GW = 22, GAP = 10;
    const int   pairW  = GW * 2 + GAP;
    const int   leftX  = SCREEN_W / 2 - pairW / 2;
    const int   rightX = leftX + GW + GAP;
    const int   gaugeY = SCREEN_H - GH - 44;

    // ── LEFT bar: Boss HP ─────────────────────────────────────────────────────
    DrawRectangle(leftX, gaugeY, GW, GH, DARKGRAY);
    const Enemy* boss = nullptr;
    for (const auto& e : enemies)
        if (e.type == EnemyType::Boss && e.IsAlive()) { boss = &e; break; }

    if (boss) {
        float hpRatio = (float)boss->hp / boss->maxHp;
        int   hpPx    = (int)(GH * hpRatio);
        Color bossCol = {(unsigned char)(255 * (1.0f - hpRatio)),
                         (unsigned char)(255 * hpRatio), 0, 255};
        DrawRectangle(leftX, gaugeY + GH - hpPx, GW, hpPx, bossCol);
        DrawText("BOSS",                      leftX,     gaugeY - 28, 13, PURPLE);
        DrawText(TextFormat("%d", boss->hp),   leftX + 1, gaugeY - 14, 13, bossCol);
    } else {
        DrawRectangle(leftX, gaugeY, GW, GH, {30, 0, 60, 255});
        DrawText("BOSS",           leftX - 2, gaugeY - 28, 12, PURPLE);
        DrawText("WAIT",           leftX - 2, gaugeY - 14, 12, DARKPURPLE);
    }
    DrawRectangleLines(leftX, gaugeY, GW, GH, PURPLE);
    DrawText("BOSS", leftX, gaugeY + GH + 5, 12, PURPLE);

    // ── RIGHT bar: Altitude meter ─────────────────────────────────────────────
    DrawRectangle(rightX, gaugeY, GW, GH, DARKGRAY);
    float playerR = Clamp((player.pos.y - ALT_MIN) / altRange, 0.0f, 1.0f);
    int   altFill = (int)(GH * playerR);
    DrawRectangle(rightX, gaugeY + GH - altFill, GW, altFill, SKYBLUE);

    for (int a = 1; a <= (int)ALT_MAX; ++a) {
        float r   = (a - ALT_MIN) / altRange;
        int   ty  = gaugeY + GH - (int)(GH * r);
        bool  maj = (a % 2 == 0);
        int   tL  = maj ? 7 : 4;
        Color tC  = maj ? WHITE : LIGHTGRAY;
        DrawLine(rightX - tL, ty, rightX,        ty, tC);
        DrawLine(rightX + GW, ty, rightX + GW + tL, ty, tC);
        if (maj)
            DrawText(TextFormat("%d", a), rightX + GW + tL + 2, ty - 7, 12, LIGHTGRAY);
    }
    DrawRectangleLines(rightX, gaugeY, GW, GH, WHITE);
    DrawText("ALT",                            rightX + 1, gaugeY + GH + 5,  12, SKYBLUE);
    DrawText(TextFormat("%.1f", player.pos.y), rightX - 6, gaugeY + GH + 18, 12, SKYBLUE);

    // ── Controls ──────────────────────────────────────────────────────────────
    DrawText("Arrows:Move/Alt  Z:Missile  X:Bomb(Ground)  C:ScreenBomb  SPC:Boost",
             12, SCREEN_H - 26, 13, LIGHTGRAY);

    // ── Stage Clear overlay ───────────────────────────────────────────────────
    if (stageClearTimer > 0.0f) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 140});
        const char* ct = TextFormat("STAGE %d CLEAR!", currentStage);
        int cw = MeasureText(ct, 56);
        DrawText(ct, SCREEN_W / 2 - cw / 2, SCREEN_H / 2 - 40, 56, GOLD);
        const char* nt = TextFormat("STAGE %d  INCOMING", currentStage + 1);
        int nw = MeasureText(nt, 26);
        DrawText(nt, SCREEN_W / 2 - nw / 2, SCREEN_H / 2 + 30, 26, ORANGE);
    }

    // ── Game Over overlay ─────────────────────────────────────────────────────
    if (gameOver) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, {0, 0, 0, 160});
        const char* goText = "GAME OVER";
        int tw = MeasureText(goText, 72);
        DrawText(goText, SCREEN_W / 2 - tw / 2, SCREEN_H / 2 - 70, 72, RED);
        const char* st = TextFormat("Final Score: %d  (Stage %d)", score, currentStage);
        int sw = MeasureText(st, 28);
        DrawText(st, SCREEN_W / 2 - sw / 2, SCREEN_H / 2 + 16, 28, WHITE);
        const char* rt = "Press  R  to restart";
        int rw = MeasureText(rt, 24);
        DrawText(rt, SCREEN_W / 2 - rw / 2, SCREEN_H / 2 + 58, 24, LIGHTGRAY);
    }
}
