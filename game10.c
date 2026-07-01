#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#define MAX_ENEMIES 15
#define ADD_INTERVAL_FRAMES 360

#define MAX_ITEMS 6
#define ITEM_SPAWN_INTERVAL_FRAMES 420

#define PATCH_COUNT 7
#define PATCH_SPECIAL_CHANCE 18

typedef enum { ITEM_INVINCIBLE = 0, ITEM_SHRINK = 1, ITEM_LIFE = 2 } ItemType;

typedef struct {
    Rectangle rect;
    ItemType type;
    bool active;
    float speed;
} Item;

typedef enum { SURF_ICE = 0, SURF_MUD = 1 } SurfaceType;

typedef struct {
    Rectangle rect;
    SurfaceType type;
    bool active;
} SurfacePatch;

static float RandomXInRoad(int roadX, int roadW, int margin, int objW)
{
    int minX = roadX + margin;
    int maxX = roadX + roadW - margin - objW;
    return (float)GetRandomValue(minX, maxX);
}

static float RandomSpeed(float minV, float maxV)
{
    int a = (int)(minV * 10.0f);
    int b = (int)(maxV * 10.0f);
    return (float)GetRandomValue(a, b) / 10.0f;
}

static void SpawnEnemy(Rectangle *e, float *v,
                       int roadX, int roadW, int edgeWidth, int roadMargin,
                       float w, float h, int screenHeight,
                       float minV, float maxV)
{
    e->width = w;
    e->height = h;
    e->x = RandomXInRoad(roadX + edgeWidth, roadW - 2 * edgeWidth, roadMargin, (int)w);
    e->y = -h - (float)GetRandomValue(0, screenHeight);
    *v = RandomSpeed(minV, maxV);
}

static void SpawnItem(Item *it,
                      int roadX, int roadW, int edgeWidth, int roadMargin,
                      int screenHeight)
{
    float w = 34.0f;
    float h = 34.0f;

    it->rect.width = w;
    it->rect.height = h;
    it->rect.x = RandomXInRoad(roadX + edgeWidth, roadW - 2 * edgeWidth, roadMargin, (int)w);
    it->rect.y = -h - (float)GetRandomValue(0, screenHeight);

    it->type = (ItemType)GetRandomValue(0, 2);
    it->speed = RandomSpeed(2.0f, 4.5f);
    it->active = true;
}

static void RespawnPatch(SurfacePatch *p, int innerX, int innerW, float topY)
{
    int w = GetRandomValue(110, 200);
    if (w > innerW - 20) w = innerW - 20;

    int h = GetRandomValue(70, 140);
    int gap = GetRandomValue(220, 520);

    p->rect.width = (float)w;
    p->rect.height = (float)h;
    p->rect.x = (float)GetRandomValue(innerX, innerX + innerW - w);
    p->rect.y = topY - p->rect.height - (float)gap;

    int r = GetRandomValue(0, 99);
    if (r < PATCH_SPECIAL_CHANCE)
    {
        p->active = true;
        int t = GetRandomValue(0, 99);
        p->type = (t < 50) ? SURF_ICE : SURF_MUD;
    }
    else
    {
        p->active = false;
        p->type = SURF_ICE;
    }
}

static void DrawTextureInRect(Texture2D tex, Rectangle dst, Color tint)
{
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle d = { dst.x + dst.width * 0.5f, dst.y + dst.height * 0.5f, dst.width, dst.height };
    Vector2 origin = { dst.width * 0.5f, dst.height * 0.5f };
    DrawTexturePro(tex, src, d, origin, 0.0f, tint);
}

static void DrawTextureInRectRot(Texture2D tex, Rectangle dst, float rotDeg, Color tint)
{
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle d = { dst.x + dst.width * 0.5f, dst.y + dst.height * 0.5f, dst.width, dst.height };
    Vector2 origin = { dst.width * 0.5f, dst.height * 0.5f };
    DrawTexturePro(tex, src, d, origin, rotDeg, tint);
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "Car race");
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    const int roadWidth = 420;
    const int roadX = (screenWidth - roadWidth) / 2;
    const int roadMargin = 10;
    const int edgeWidth = 14;

    Rectangle road = { (float)roadX, 0.0f, (float)roadWidth, (float)screenHeight };

    const float playerBaseW = 50.0f;
    const float playerBaseH = 80.0f;
    const float playerBottom = (float)screenHeight - 30.0f;

    Rectangle player = { 0 };
    player.width = playerBaseW;
    player.height = playerBaseH;
    player.x = roadX + roadWidth / 2.0f - player.width / 2.0f;
    player.y = playerBottom - player.height;

    const float playerBaseSpeed = 6.0f;
    const float playerSpeedIncPerFrame = 0.0008f;
    const float playerSpeedMax = 14.0f;

    const float iceMoveMul = 1.6f;
    const float mudMoveMul = 0.55f;

    Rectangle enemies[MAX_ENEMIES];
    float enemyV[MAX_ENEMIES];
    const float enemyMinV = 2.0f;
    const float enemyMaxV = 6.0f;
    int enemiesActive = 1;

    Item items[MAX_ITEMS];
    for (int i = 0; i < MAX_ITEMS; i++) items[i].active = false;

    const int INVINCIBLE_FROM_DAMAGE = 90;
    const int INVINCIBLE_FROM_ITEM = 240;
    const int SHRINK_DURATION = 300;
    const float SHRINK_SCALE = 0.65f;

    int invincibleFrames = 0;
    int shrinkFrames = 0;

    int lives = 3;

    bool gameOver = false;
    unsigned long frameCount = 0;

    float score = 0.0f;
    const float scoreBasePerFrame = 1.0f;
    const float scoreIncPerFrame = 0.0005f;
    const float scoreMaxPerFrame = 3.0f;

    float laneOffset = 0.0f;
    const float dashW = 10.0f;
    const float dashH = 28.0f;
    const float dashPeriod = 52.0f;

    const float roadScrollFactor = 3.2f;
    float currentPlayerSpeed = playerBaseSpeed;

    const int innerX = roadX + edgeWidth;
    const int innerW = roadWidth - 2 * edgeWidth;

    SurfacePatch patches[PATCH_COUNT];

    Texture2D texPlayer = LoadTexture("assets/player.png");
    Texture2D texEnemy  = LoadTexture("assets/enemy.png");
    Texture2D texItemInv  = LoadTexture("assets/item_invincible.png");
    Texture2D texItemShr  = LoadTexture("assets/item_shrink.png");
    Texture2D texItemLife = LoadTexture("assets/item_life.png");

    bool hasPlayerTex = (texPlayer.id != 0);
    bool hasEnemyTex  = (texEnemy.id != 0);
    bool hasInvTex    = (texItemInv.id != 0);
    bool hasShrTex    = (texItemShr.id != 0);
    bool hasLifeTex   = (texItemLife.id != 0);

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i] = (Rectangle){ 0 };
        enemyV[i] = 0.0f;
    }

    SpawnEnemy(&enemies[0], &enemyV[0],
               roadX, roadWidth, edgeWidth, roadMargin,
               50, 80, screenHeight, enemyMinV, enemyMaxV);

    for (int i = 0; i < PATCH_COUNT; i++)
    {
        patches[i].rect = (Rectangle){ 0 };
        patches[i].type = (SurfaceType)GetRandomValue(0, 1);
        patches[i].active = false;
        float topY = -((float)i * 260.0f) - (float)GetRandomValue(0, 220);
        RespawnPatch(&patches[i], innerX, innerW, topY);
    }

    const Color invCol = YELLOW;
    const Color shrCol = LIME;
    const Color lifeCol = RED;

    while (!WindowShouldClose())
    {
        frameCount++;

        if (!gameOver)
        {
            if (invincibleFrames > 0) invincibleFrames--;
            if (shrinkFrames > 0) shrinkFrames--;

            currentPlayerSpeed = playerBaseSpeed + (float)frameCount * playerSpeedIncPerFrame;
            if (currentPlayerSpeed > playerSpeedMax) currentPlayerSpeed = playerSpeedMax;

            float targetW = (shrinkFrames > 0) ? (playerBaseW * SHRINK_SCALE) : playerBaseW;
            float targetH = (shrinkFrames > 0) ? (playerBaseH * SHRINK_SCALE) : playerBaseH;

            if (player.width != targetW || player.height != targetH)
            {
                float cx = player.x + player.width * 0.5f;
                player.width = targetW;
                player.height = targetH;
                player.x = cx - player.width * 0.5f;
                player.y = playerBottom - player.height;
            }

            float scrollDelta = currentPlayerSpeed * roadScrollFactor;

            laneOffset += scrollDelta;
            if (laneOffset > 1000000.0f) laneOffset = fmodf(laneOffset, dashPeriod);

            for (int i = 0; i < PATCH_COUNT; i++)
            {
                patches[i].rect.y += scrollDelta;
                if (patches[i].rect.y > (float)screenHeight)
                {
                    float topY = -(float)GetRandomValue(180, 520);
                    RespawnPatch(&patches[i], innerX, innerW, topY);
                }
            }

            float moveMul = 1.0f;
            for (int i = 0; i < PATCH_COUNT; i++)
            {
                if (!patches[i].active) continue;
                if (CheckCollisionRecs(player, patches[i].rect))
                {
                    if (patches[i].type == SURF_ICE) { moveMul = iceMoveMul; break; }
                    if (patches[i].type == SURF_MUD) { moveMul = mudMoveMul; }
                }
            }

            if (IsKeyDown(KEY_LEFT))  player.x -= currentPlayerSpeed * moveMul;
            if (IsKeyDown(KEY_RIGHT)) player.x += currentPlayerSpeed * moveMul;

            float leftLimit  = roadX + edgeWidth + roadMargin;
            float rightLimit = roadX + roadWidth - edgeWidth - roadMargin - player.width;

            if (player.x < leftLimit)  player.x = leftLimit;
            if (player.x > rightLimit) player.x = rightLimit;

            float scorePerFrame = scoreBasePerFrame + (float)frameCount * scoreIncPerFrame;
            if (scorePerFrame > scoreMaxPerFrame) scorePerFrame = scoreMaxPerFrame;
            score += scorePerFrame;

            if ((frameCount % ADD_INTERVAL_FRAMES == 0) && enemiesActive < MAX_ENEMIES)
            {
                SpawnEnemy(&enemies[enemiesActive], &enemyV[enemiesActive],
                           roadX, roadWidth, edgeWidth, roadMargin,
                           50, 80, screenHeight, enemyMinV, enemyMaxV);
                enemiesActive++;
            }

            if (frameCount % ITEM_SPAWN_INTERVAL_FRAMES == 0)
            {
                for (int i = 0; i < MAX_ITEMS; i++)
                {
                    if (!items[i].active)
                    {
                        SpawnItem(&items[i], roadX, roadWidth, edgeWidth, roadMargin, screenHeight);
                        break;
                    }
                }
            }

            for (int i = 0; i < enemiesActive; i++)
            {
                enemies[i].y += enemyV[i];

                if (enemies[i].y > screenHeight)
                {
                    enemies[i].y = -enemies[i].height - (float)GetRandomValue(0, 200);
                    enemies[i].x = RandomXInRoad(roadX + edgeWidth, roadWidth - 2 * edgeWidth, roadMargin, (int)enemies[i].width);
                }

                if (invincibleFrames == 0 && CheckCollisionRecs(player, enemies[i]))
                {
                    lives--;
                    if (lives <= 0) gameOver = true;
                    else
                    {
                        invincibleFrames = INVINCIBLE_FROM_DAMAGE;
                        player.x = roadX + roadWidth / 2.0f - player.width / 2.0f;
                        enemies[i].y = -enemies[i].height - (float)GetRandomValue(0, screenHeight);
                        enemies[i].x = RandomXInRoad(roadX + edgeWidth, roadWidth - 2 * edgeWidth, roadMargin, (int)enemies[i].width);
                    }
                    break;
                }
            }

            for (int i = 0; i < MAX_ITEMS; i++)
            {
                if (!items[i].active) continue;

                items[i].rect.y += items[i].speed + currentPlayerSpeed * 0.15f;

                if (items[i].rect.y > screenHeight)
                {
                    items[i].active = false;
                    continue;
                }

                if (CheckCollisionRecs(player, items[i].rect))
                {
                    if (items[i].type == ITEM_INVINCIBLE)
                    {
                        if (invincibleFrames < INVINCIBLE_FROM_ITEM) invincibleFrames = INVINCIBLE_FROM_ITEM;
                    }
                    else if (items[i].type == ITEM_SHRINK)
                    {
                        if (shrinkFrames < SHRINK_DURATION) shrinkFrames = SHRINK_DURATION;
                    }
                    else if (items[i].type == ITEM_LIFE)
                    {
                        if (lives < 9) lives++;
                    }

                    items[i].active = false;
                }
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawRectangle(0, 0, roadX, screenHeight, DARKGREEN);
        DrawRectangle(roadX + roadWidth, 0, screenWidth - (roadX + roadWidth), screenHeight, DARKGREEN);

        DrawRectangleRec(road, DARKGRAY);

        for (int i = 0; i < PATCH_COUNT; i++)
        {
            if (!patches[i].active) continue;
            if (patches[i].type == SURF_ICE) DrawRectangleRec(patches[i].rect, Fade(SKYBLUE, 0.38f));
            else DrawRectangleRec(patches[i].rect, Fade(BROWN, 0.38f));
        }

        DrawRectangle(roadX, 0, edgeWidth, screenHeight, GRAY);
        DrawRectangle(roadX + roadWidth - edgeWidth, 0, edgeWidth, screenHeight, GRAY);

        DrawLineEx((Vector2){ (float)(roadX + edgeWidth), 0 }, (Vector2){ (float)(roadX + edgeWidth), (float)screenHeight }, 2.0f, RAYWHITE);
        DrawLineEx((Vector2){ (float)(roadX + roadWidth - edgeWidth), 0 }, (Vector2){ (float)(roadX + roadWidth - edgeWidth), (float)screenHeight }, 2.0f, RAYWHITE);

        float laneX = (float)innerX + (float)innerW / 2.0f;
        float startY = -dashPeriod + fmodf(laneOffset, dashPeriod);
        for (float yy = startY; yy < (float)screenHeight; yy += dashPeriod)
        {
            DrawRectangle((int)(laneX - dashW * 0.5f), (int)yy, (int)dashW, (int)dashH, RAYWHITE);
        }

        Color playerTint = (invincibleFrames > 0 && !gameOver) ? SKYBLUE : WHITE;
        if (hasPlayerTex) DrawTextureInRect(texPlayer, player, playerTint);
        else DrawRectangleRec(player, (invincibleFrames > 0 && !gameOver) ? SKYBLUE : BLUE);

        for (int i = 0; i < enemiesActive; i++)
        {
            if (hasEnemyTex) DrawTextureInRectRot(texEnemy, enemies[i], 180.0f, WHITE);
            else DrawRectangleRec(enemies[i], RED);
        }

        for (int i = 0; i < MAX_ITEMS; i++)
        {
            if (!items[i].active) continue;

            if (items[i].type == ITEM_INVINCIBLE)
            {
                if (hasInvTex) DrawTextureInRect(texItemInv, items[i].rect, invCol);
                else DrawRectangleRec(items[i].rect, invCol);
            }
            else if (items[i].type == ITEM_SHRINK)
            {
                if (hasShrTex) DrawTextureInRect(texItemShr, items[i].rect, shrCol);
                else DrawRectangleRec(items[i].rect, shrCol);
            }
            else
            {
                if (hasLifeTex) DrawTextureInRect(texItemLife, items[i].rect, lifeCol);
                else DrawRectangleRec(items[i].rect, lifeCol);
            }
        }

        DrawText(TextFormat("Score: %d", (int)score), 20, 20, 22, YELLOW);
        DrawText(TextFormat("Lives: %d", lives), 20, 48, 20, RAYWHITE);

        if (gameOver)
        {
            const char *msg = "Game Over!";
            int fontSize = 50;
            int textW = MeasureText(msg, fontSize);
            DrawText(msg, screenWidth / 2 - textW / 2, screenHeight / 2 - fontSize, fontSize, RED);
            DrawText("Close the window to exit.", screenWidth / 2 - 160, screenHeight / 2 + 20, 20, RAYWHITE);
        }

        EndDrawing();
    }

    if (texPlayer.id) UnloadTexture(texPlayer);
    if (texEnemy.id) UnloadTexture(texEnemy);
    if (texItemInv.id) UnloadTexture(texItemInv);
    if (texItemShr.id) UnloadTexture(texItemShr);
    if (texItemLife.id) UnloadTexture(texItemLife);

    CloseWindow();
    return 0;
}