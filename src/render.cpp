// World rendering and lighting.
#include "game.h"
#include <algorithm>

static RenderTexture2D lightRT;
static int lightW = 0, lightH = 0;

void initRender() {
    lightW = GetScreenWidth(); lightH = GetScreenHeight();
    lightRT = LoadRenderTexture(lightW, lightH);
}
void unloadRender() { UnloadRenderTexture(lightRT); }

static int visSeason() {
    int s = g->season();
    if (g->snowCover > 0.45f) return 3;
    if (s == 3 && g->snowCover < 0.2f) return 2;   // brown late-winter look before the snow comes
    return s;
}

static Color withA(Color c, float a) { c.a = (unsigned char)clampf(a * 255, 0, 255); return c; }

static void drawCrop(int x, int y, const Crop& c) {
    float px = x * TS, py = y * TS;
    if (c.dead) {
        for (int k = 0; k < 3; k++) DrawRectangle(px + 3 + k * 4, py + 7 + k % 2 * 2, 2, 6, Color{110, 84, 50, 255});
        return;
    }
    float gr = c.growth;
    Color green = Color{80, 160, 70, 255};
    int stage = gr < 0.25f ? 0 : gr < 0.6f ? 1 : gr < 1.f ? 2 : 3;
    for (int k = 0; k < 3; k++) {
        float cx = px + 3 + k * 5, base = py + 14;
        switch (c.type) {
            case CR_WHEAT: {
                int h = 3 + stage * 3;
                Color col = stage == 3 ? Color{226, 196, 92, 255} : (stage == 2 ? Color{170, 180, 80, 255} : green);
                DrawRectangle(cx, base - h, 1, h, col);
                DrawRectangle(cx + 2, base - h + 1, 1, h - 1, col);
                if (stage >= 2) DrawRectangle(cx - 0.5f, base - h - 2, 2, 3, stage == 3 ? Color{240, 210, 110, 255} : col);
                break;
            }
            case CR_POTATO: {
                float r = 1.5f + stage;
                DrawCircle(cx + 1, base - r, r, green);
                if (stage == 3) DrawRectangle(cx, base - r * 2, 2, 2, Color{240, 240, 255, 255});
                break;
            }
            case CR_CABBAGE: {
                float r = 1 + stage * 1.1f;
                DrawCircle(cx + 1, base - r, r, Color{110, 180, 100, 255});
                if (stage >= 2) DrawCircle(cx + 1, base - r, r * 0.6f, Color{170, 220, 150, 255});
                break;
            }
            case CR_TOMATO: {
                int h = 3 + stage * 2;
                DrawRectangle(cx + 1, base - h, 1, h, Color{70, 140, 60, 255});
                DrawCircle(cx + 1, base - h, 1.5f + stage * 0.5f, green);
                if (stage == 3) { DrawRectangle(cx - 1, base - h + 2, 2, 2, Color{220, 40, 30, 255}); DrawRectangle(cx + 2, base - h + 4, 2, 2, Color{230, 60, 40, 255}); }
                break;
            }
            case CR_SQUASH: {
                DrawCircle(cx + 1, base - 2, 1 + stage, green);
                if (stage >= 2) DrawEllipse(cx + 1, base - 1, 1.5f + stage * 0.5f, 1.3f + stage * 0.3f, stage == 3 ? Color{230, 140, 40, 255} : Color{140, 170, 70, 255});
                break;
            }
            case CR_GREENBEAN: {
                int h = 2 + stage * 2;
                DrawRectangle(cx, base - h, 3, h, green);
                if (stage == 3) { DrawRectangle(cx - 1, base - h + 1, 1, 3, Color{120, 200, 90, 255}); DrawRectangle(cx + 3, base - h + 2, 1, 3, Color{120, 200, 90, 255}); }
                break;
            }
            case CR_BUCKWHEAT: {
                int h = 2 + stage * 2;
                DrawRectangle(cx, base - h, 2, h, Color{140, 60, 60, 255});
                DrawCircle(cx + 1, base - h, 1 + stage * 0.6f, stage >= 2 ? Color{245, 235, 240, 255} : green);
                break;
            }
            case CR_MUSTARD: {
                int h = 2 + stage * 2;
                DrawRectangle(cx, base - h, 3, h, Color{100, 160, 70, 255});
                if (stage >= 2) DrawRectangle(cx - 1, base - h - 1, 4, 2, Color{240, 220, 60, 255});
                break;
            }
            case CR_BEANS: {
                int h = 3 + stage * 3;
                DrawRectangle(cx + 1, base - h - 1, 1, h + 1, Color{140, 110, 70, 255});
                DrawRectangle(cx, base - h + 2, 3, h - 2, green);
                if (stage == 3) { DrawRectangle(cx - 1, base - h + 3, 1, 3, Color{60, 120, 50, 255}); DrawRectangle(cx + 3, base - h + 5, 1, 3, Color{60, 120, 50, 255}); }
                break;
            }
        }
    }
    if (c.pest > 0.35f) {
        int n = (int)(c.pest * 6);
        for (int k = 0; k < n; k++) {
            float t = g->totalTime * 3 + k * 1.7f + x * 3;
            DrawRectangle(px + 8 + std::sin(t) * 6, py + 8 + std::cos(t * 1.3f) * 5, 1, 1, Color{30, 30, 20, 255});
        }
    }
}

static void drawFlame(float cx, float by, float scale, float seed) {
    float t = g->totalTime * 9 + seed;
    float h = (6 + std::sin(t) * 1.5f + std::sin(t * 2.3f) * 1) * scale;
    float w = 4 * scale;
    DrawTriangle({cx, by - h}, {cx - w, by}, {cx + w, by}, Color{230, 80, 30, 230});
    DrawTriangle({cx + std::sin(t * 1.7f) * 0.8f, by - h * 0.7f}, {cx - w * 0.6f, by}, {cx + w * 0.6f, by}, Color{255, 170, 50, 240});
    DrawTriangle({cx, by - h * 0.4f}, {cx - w * 0.3f, by}, {cx + w * 0.3f, by}, Color{255, 240, 160, 255});
}

struct Drawable { float y; int type; int a; };  // type 0 = object tile, 1 = entity, 2 = player

static void drawEntity(const Ent& e) {
    const SpeciesDef& s = SPECIES[e.kind];
    float scale = e.baby ? 0.65f : 1.f;
    float px = e.x * TS, py = e.y * TS;
    // shadow
    DrawEllipse(px, py, 6 * s.size * scale, 2.5f * scale, Color{0, 0, 0, 70});
    bool moving = std::fabs(std::sin(e.anim)) > 0.01f && (e.vx != 0 || e.vy != 0 || e.kind >= E_WORKER);
    int frame = e.sleeping ? 0 : ((int)(e.anim) % 2 + 1);
    if (!moving && e.kind != E_WORKER) frame = 0;
    if (e.kind == E_SHEEP && e.tame && e.produce < 0.5f) frame += 3;
    Rectangle src = entSrc(e.kind, frame, e.look);
    if (e.face < 0) src.width = -src.width;
    float w = 24 * scale, h = 24 * scale;
    float sz = (e.kind == E_BEAR || e.kind == E_BUFFALO) ? 1.25f : 1.f;
    w *= sz; h *= sz;
    Color tint = e.hurtFlash > 0 ? Color{255, 120, 120, 255} : WHITE;
    if (e.sleeping && e.kind == E_BEAR) tint = Color{200, 200, 220, 255};
    DrawTexturePro(atlas, src, Rectangle{px - w / 2, py - h + 2 * scale, w, h}, {0, 0}, 0, tint);
    if (e.kind == E_RAIDER && e.torch && e.state >= 1) {
        DrawRectangle(px + e.face * 6, py - 16, 1, 8, Color{110, 80, 50, 255});
        drawFlame(px + e.face * 6 + 0.5f, py - 16, 0.5f, e.id);
    }
    if (e.sleeping) DrawText("z", px + 4, py - h - 2 + std::sin(g->totalTime * 2) * 2, 8, Color{220, 220, 255, 220});
    // markers
    if (e.tame && e.kind < E_WORKER) { DrawRectangle(px - 1, py - h - 1, 3, 2, Color{255, 120, 160, 255}); DrawRectangle(px, py - h + 1, 1, 1, Color{255, 120, 160, 255}); }
    if (e.kind == E_VILLAGER && e.state == 1) {
        float bob = std::sin(g->totalTime * 3) * 1.5f;
        DrawCircle(px, py - 28 + bob, 4, Color{240, 200, 60, 255});
        DrawText("$", px - 2, py - 32 + bob, 8, Color{120, 80, 20, 255});
    }
    Vector2 wm = GetScreenToWorld2D(GetMousePosition(), g->cam);
    bool hovered = dist(wm.x / TS, wm.y / TS, e.x, e.y - 0.5f) < 1.2f;
    if (e.kind == E_WORKER && !e.name.empty() && (hovered || IsKeyDown(KEY_LEFT_ALT))) {
        const char* txt = TextFormat("%s (%s)", e.name.c_str(), JOB_NAMES[e.job]);
        int tw = MeasureText(txt, 5);
        DrawText(txt, px - tw / 2, py - 30, 5, Color{230, 240, 255, 230});
    }
    if (e.carry.id && e.carry.n > 0 && e.kind == E_WORKER) {
        drawItemIcon(e.carry.id, px - 4, py - 22, 8);
    }
    if (e.hp < e.maxhp && e.kind != E_FROG && e.kind != E_SPIDER) {
        float bw = 14;
        DrawRectangle(px - bw / 2, py - h - 4, bw, 2, Color{40, 10, 10, 200});
        DrawRectangle(px - bw / 2, py - h - 4, bw * clampf(e.hp / e.maxhp, 0, 1), 2, e.kind == E_RAIDER || e.kind == E_WOLF ? RED : GREEN);
    }
}

static void drawPlayer() {
    Player& p = g->pl;
    float px = p.x * TS, py = p.y * TS;
    float lift = 0;
    if (p.riding >= 0) { Ent* h = entById(p.riding); if (h) { drawEntity(*h); lift = 9; } }
    else DrawEllipse(px, py, 5, 2.2f, Color{0, 0, 0, 70});
    int frame = p.sleeping ? 0 : (((int)p.anim) % 2 + 1);
    bool moving = IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
    if (!moving || p.riding >= 0) frame = 0;
    Rectangle src = entSrc(E_KCOUNT, frame, 0);
    if (p.face < 0) src.width = -src.width;
    Color tint = p.hurtFlash > 0 ? Color{255, 120, 120, 255} : WHITE;
    if (p.sleeping) { DrawText("z z", px + 2, py - 30 + std::sin(g->totalTime * 2) * 2, 8, Color{220, 220, 255, 220}); return; }
    DrawTexturePro(atlas, src, Rectangle{px - 12, py - 22 - lift, 24, 24}, {0, 0}, 0, tint);
    // worn clothing tint overlays
    if (p.wear[W_BODY].id) DrawRectangle(px - 4, py - 11 - lift, 8, 6, withA(ITEMS[p.wear[W_BODY].id].c1, 0.85f));
    if (p.wear[W_HEAD].id) DrawRectangle(px - 3, py - 19 - lift, 7, 2, withA(ITEMS[p.wear[W_HEAD].id].c1, 0.95f));
    // held item
    Stack& h = p.inv[p.sel];
    if (h.id) {
        float swing = p.useAnim > 0 ? (p.useAnim / 0.2f) * 50.f : 0;
        float hx = px + p.face * 5, hy = py - 9 - lift;
        Rectangle isrc = itemSrc(h.id);
        if (ITEMS[h.id].placeObj >= 0) return;
        if (p.face < 0) isrc.width = -isrc.width;
        DrawTexturePro(atlas, isrc, Rectangle{hx, hy, 10, 10}, {5, 8}, p.face * swing, WHITE);
        if (h.id == I_TORCH) drawFlame(hx + p.face * 3, hy - 5, 0.45f, 3);
    }
}

static void drawFishing() {
    Player& p = g->pl;
    if (!p.fishing) return;
    float bx = p.bobX * TS + 8, by = p.bobY * TS + 8 + (p.biteTimer > 0 ? std::sin(g->totalTime * 30) * 1.5f : std::sin(g->totalTime * 3) * 0.5f);
    DrawLineEx({p.x * TS + p.face * 8, p.y * TS - 18}, {bx, by}, 0.4f, Color{230, 230, 230, 200});
    DrawCircle(bx, by, 1.5f, p.biteTimer > 0 ? Color{255, 230, 60, 255} : Color{230, 50, 40, 255});
}

void renderWorld() {
    Player& p = g->pl;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (sw != lightW || sh != lightH) { unloadRender(); initRender(); }
    Vector2 shake{0, 0};
    if (g->shake > 0) { shake = {rndr(-2, 2) * g->shake * 4, rndr(-2, 2) * g->shake * 4}; g->shake -= GetFrameTime(); }
    g->cam.zoom = g->zoom;
    g->cam.offset = {sw / 2.f + shake.x, sh / 2.f + shake.y};
    g->cam.target = {std::round(p.x * TS * g->zoom) / g->zoom, std::round(p.y * TS * g->zoom) / g->zoom};
    g->cam.rotation = 0;

    Vector2 tl = GetScreenToWorld2D({0, 0}, g->cam), br = GetScreenToWorld2D({(float)sw, (float)sh}, g->cam);
    int x0 = std::max(0, (int)(tl.x / TS) - 1), y0 = std::max(0, (int)(tl.y / TS) - 1);
    int x1 = std::min(WW - 1, (int)(br.x / TS) + 1), y1 = std::min(WH - 1, (int)(br.y / TS) + 3);
    int vs = visSeason();

    BeginMode2D(g->cam);
    // ---- ground
    int waterFrame = (int)(g->totalTime * 1.5f);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = IDX(x, y);
            uint8_t gr = g->ground[i];
            int var = (int)(hash32(i) & 3);
            if (gr == G_WATER || gr == G_DEEP) var = (waterFrame + (int)(hash32(i) % 4)) & 3;
            int s = vs;
            if (gr == G_GREENHOUSE || gr == G_FLOOR || gr == G_VILLAGE) s = std::min(s, 2);
            DrawTexturePro(atlas, groundSrc(gr, s, var), Rectangle{(float)x * TS, (float)y * TS, TS, TS}, {0, 0}, 0, WHITE);
            // soft edge: darker bank where land meets water above
            if ((gr == G_WATER || gr == G_DEEP) && y > 0 && !isWater(x, y - 1)) {
                DrawRectangle(x * TS, y * TS, TS, 3, Color{92, 70, 48, 255});
                DrawRectangle(x * TS, y * TS + 3, TS, 1, Color{20, 40, 70, 120});
            }
            if ((gr == G_WATER) && ((x > 0 && !isWater(x - 1, y)) || (x < WW - 1 && !isWater(x + 1, y)))) {
                if (!isWater(x - 1, y)) DrawRectangle(x * TS, y * TS, 1, TS, Color{200, 230, 255, 90});
                if (!isWater(x + 1, y)) DrawRectangle(x * TS + TS - 1, y * TS, 1, TS, Color{200, 230, 255, 90});
            }
            if ((gr == G_TILLED || gr == G_GREENHOUSE) && g->moist[i] > 0.4f) DrawRectangle(x * TS, y * TS, TS, TS, Color{20, 10, 0, (unsigned char)(g->moist[i] * 60)});
            if ((gr == G_TILLED || gr == G_GREENHOUSE) && !g->crops[i].type && soilMin(i) < 0.3f) DrawRectangle(x * TS + 1, y * TS + 1, TS - 2, TS - 2, Color{200, 180, 120, 50});   // tired, pale soil
            if (vs == 2 && (gr == G_GRASS || gr == G_FOREST || gr == G_DIRT)) {
                bool under = false;
                for (int k = -1; k <= 1 && !under; k++)
                    for (int j = -1; j <= 1; j++) {
                        if (!inb(x + j, y + k)) continue;
                        uint8_t o = g->tiles[IDX(x + j, y + k)].obj;
                        if (isDeciduous(o)) { under = true; break; }
                    }
                if (under)
                    for (int k = 0; k < 6; k++) {
                        uint32_t h = hash32(i * 13 + k);
                        Color lc = k % 3 == 0 ? Color{200, 90, 40, 255} : k % 3 == 1 ? Color{220, 160, 50, 255} : Color{160, 110, 50, 255};
                        DrawRectangle(x * TS + (h % 15), y * TS + ((h >> 8) % 15), 2, 1, lc);
                    }
            }
            if (g->litter[i] > 0.15f)
                for (int k = 0; k < (int)(g->litter[i] * 6); k++) { uint32_t h = hash32(i * 31 + k); DrawRectangle(x * TS + (h % 14) + 1, y * TS + ((h >> 8) % 14) + 1, 1, 1, Color{235, 230, 215, 220}); }
            if (g->crops[i].type) drawCrop(x, y, g->crops[i]);
        }

    // ---- flat objects, then y-sorted tall things
    static std::vector<Drawable> list;
    list.clear();
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (!o) continue;
            bool flat = o == O_MANURE || o == O_EGG || o == O_ASH || o == O_BRANCH || o == O_FLINT || o == O_CLAY || o == O_POND || o == O_SPRINKLER;
            if (flat) DrawTexturePro(atlas, objSrc(o, vs, g->tiles[IDX(x, y)].var), Rectangle{(float)x * TS, (float)y * TS - TS, TS, TS * 2}, {0, 0}, 0, WHITE);
            else list.push_back({(float)y + 0.99f, 0, IDX(x, y)});
        }
    for (int y = std::max(0, y0 - 8); y <= std::min(WH - 1, y1 + 8); y++)
        for (int x = std::max(0, x0 - 8); x <= std::min(WW - 1, x1 + 8); x++) {
            int i = IDX(x, y);
            uint8_t o = g->tiles[i].obj;
            if (!o || o == O_BATTERY || !isPowerObj(o)) continue;
            int b = powerLink(i);
            if (b < 0) continue;
            Vector2 a{x * TS + 8.f, y * TS + 12.f}, c{(b % WW) * TS + 8.f, (b / WW) * TS + 10.f};
            Vector2 mid{(a.x + c.x) / 2, (a.y + c.y) / 2 + 3};   // slight sag
            Color wire = poweredAt(x, y) ? Color{40, 34, 30, 220} : Color{120, 40, 30, 220};
            DrawLineEx(a, mid, 0.7f, wire);
            DrawLineEx(mid, c, 0.7f, wire);
        }
    for (int y = std::max(0, y0 - 8); y <= std::min(WH - 1, y1 + 8); y++)
        for (int x = std::max(0, x0 - 8); x <= std::min(WW - 1, x1 + 8); x++) {
            int i = IDX(x, y);
            uint8_t o = g->tiles[i].obj;
            if (o != O_SPRINKLER && o != O_DRIP) continue;
            int s = waterLink(i);
            if (s < 0) continue;
            Color pipe = irrigating(x, y) ? Color{70, 140, 220, 230} : Color{60, 80, 110, 200};
            DrawLineEx({x * TS + 8.f, y * TS + 14.f}, {(s % WW) * TS + 8.f, (s / WW) * TS + 14.f}, 1.2f, pipe);
        }
    for (size_t k = 0; k < g->ents.size(); k++) {
        const Ent& e = g->ents[k];
        if (!e.alive || e.id == p.riding || e.hidden) continue;
        if (e.x < x0 - 1 || e.x > x1 + 1 || e.y < y0 - 1 || e.y > y1 + 2) continue;
        list.push_back({e.y, 1, (int)k});
    }
    list.push_back({p.y, 2, 0});
    std::sort(list.begin(), list.end(), [](const Drawable& a, const Drawable& b) { return a.y < b.y; });
    for (auto& d : list) {
        if (d.type == 0) {
            int x = d.a % WW, y = d.a / WW;
            Tile& t = g->tiles[d.a];
            Color tint = WHITE;
            if (OBJS[t.obj].tall && p.y < y + 0.6f && p.y > y - 1.6f && std::fabs(p.x - (x + 0.5f)) < 1.2f) tint.a = 120;
            int var = t.var;
            if (t.obj == O_BUSH && t.var >= 4) var = 4;
            if (t.obj == O_TENT) var = t.var & 1;
            if (t.obj == O_BANNER) var = t.var;
            DrawTexturePro(atlas, objSrc(t.obj, (t.obj == O_TENT || t.obj == O_BANNER) ? 1 : vs, var), Rectangle{(float)x * TS, (float)y * TS - TS, TS, TS * 2}, {0, 0}, 0, tint);
            float cx = x * TS + 8, by = y * TS + 13;
            if (t.obj == O_CAMPFIRE) drawFlame(cx, by, 0.9f, x * 7.f);
            else if (t.obj == O_BONFIRE) drawFlame(cx, by - 1, 1.4f, x * 7.f);
            else if (t.obj == O_TORCHPOST) drawFlame(cx, y * TS - 2, 0.5f, x * 3.f);
            else if (t.obj == O_LAMP && poweredAt(x, y) && sunLevel() < 0.5f) DrawRectangle(x * TS + 5, y * TS - 10, 6, 6, Color{255, 255, 220, 255});
            else if (t.obj == O_WHEEL) {
                float a = g->totalTime * 60;
                for (int k = 0; k < 4; k++) {
                    float ang = (a + k * 45) * DEG2RAD;
                    DrawLineEx({cx + std::cos(ang) * 7, y * TS + 5 + std::sin(ang) * 7}, {cx - std::cos(ang) * 7, y * TS + 5 - std::sin(ang) * 7}, 1, Color{80, 54, 32, 255});
                }
            }
            if (isFruitPlant(t.obj) && g->fruitState[d.a] == 1) {   // ripe fruit
                Color fc = fruitColor(d.a);
                bool tree = isFruitTree(t.obj);
                for (int k = 0; k < (tree ? 7 : 5); k++) {
                    uint32_t h = hash32(d.a * 7 + k);
                    float fx = x * TS + 3 + (h % 10), fy = tree ? y * TS - 12 + ((h >> 8) % 12) : y * TS + 4 + ((h >> 8) % 7);
                    DrawRectangle(fx, fy, 2, 2, fc);
                }
            }
            if (t.var & VAR_GUARD && (isTreeObj(t.obj) || t.obj == O_SAPLING)) {   // wire mesh around the trunk
                DrawRectangleLines(x * TS + 4, y * TS + 6, 8, 8, Color{190, 190, 200, 230});
                DrawLine(x * TS + 4, y * TS + 10, x * TS + 12, y * TS + 10, Color{190, 190, 200, 160});
            }
            if ((t.obj == O_SPRINKLER || t.obj == O_DRIP || t.obj == O_WATERFALL) && (t.var & VAR_TIMER)) DrawCircle(x * TS + 13, y * TS + 3, 1.5f, Color{120, 220, 140, 255});
            if (t.obj == O_BEEHIVE && (t.var & VAR_BEES) && sunLevel() > 0.3f && g->season() != 3 && airTemp() > 8)
                for (int k = 0; k < 6; k++) {   // buzzing bees
                    float a = g->totalTime * (2.5f + k * 0.37f) + k * 1.9f;
                    DrawRectangle(x * TS + 8 + std::cos(a) * (6 + k * 2.5f), y * TS + 2 + std::sin(a * 1.3f) * (4 + k), 1, 1, k % 2 ? Color{250, 210, 50, 255} : Color{40, 30, 20, 255});
                }
            if (t.obj == O_BIRDHOUSE && sunLevel() > 0.3f && g->season() != 3)
                for (int k = 0; k < 2; k++) {
                    float a = g->totalTime * (1.3f + k * 0.4f) + d.a;
                    float bx = x * TS + 8 + std::cos(a) * (14 + k * 6), by2 = y * TS - 14 + std::sin(a * 1.7f) * 8;
                    float flap = std::sin(g->totalTime * 14 + k) > 0 ? 1 : 0;
                    DrawLine(bx - 2, by2 - flap, bx, by2, Color{60, 50, 50, 255}); DrawLine(bx, by2, bx + 2, by2 - flap, Color{60, 50, 50, 255});
                }
            if (isDeciduous(t.obj) && t.hp < OBJS[t.obj].hp)   // stripped bark
                DrawRectangle(x * TS + 6, y * TS + 8, 4, 3, Color{235, 215, 170, 255});
            if (t.obj == O_WATERFALL && irrigating(x, y)) {
                float ph = g->totalTime * 30;
                for (int k = 0; k < 3; k++) {
                    float sx = x * TS + 5 + k * 3;
                    DrawRectangle(sx, y * TS - 10, 2, 22, Color{150, 205, 250, 200});
                    DrawRectangle(sx, y * TS - 10 + std::fmod(ph + k * 7, 20.f), 2, 3, Color{240, 250, 255, 255});
                }
                if (rnd() < 0.4f) g->parts.push_back({x + 0.5f + rndr(-0.3f, 0.3f), y + 0.75f, rndr(-0.6f, 0.6f), rndr(-1.5f, -0.5f), 0.4f, 0.4f, Color{220, 240, 255, 200}, 1.5f, 0});
            }
            if (lineBroken(t)) {
                if (leaking(x, y))
                    for (int k = 0; k < 2; k++)
                        g->parts.push_back({x + 0.5f + rndr(-0.1f, 0.1f), y + 0.6f, rndr(-0.8f, 0.8f), rndr(-5.f, -3.5f), 0.6f, 0.6f, Color{150, 210, 255, 230}, 2.f, 0});
                DrawRectangle(x * TS + 6, y * TS + 4, 4, 1, Color{230, 60, 50, 255});
                DrawRectangle(x * TS + 7, y * TS + 3, 2, 3, Color{230, 60, 50, 255});
            }
            if ((t.obj == O_SPRINKLER || t.obj == O_DRIP) && irrigating(x, y) && rnd() < 0.5f) {
                float a = rndr(0, 6.283f), r = t.obj == O_SPRINKLER ? 2.5f : 0.6f;
                g->parts.push_back({x + 0.5f, y + 0.4f, std::cos(a) * r, std::sin(a) * r - (t.obj == O_SPRINKLER ? 1.f : 0.f), 0.5f, 0.5f, Color{140, 200, 255, 200}, 1.5f, 3});
            }
            if (t.obj == O_BATTERY) {
                float f = clampf(t.timer / 10.f, 0, 1);
                DrawRectangle(x * TS + 4, y * TS + 8, 8, 2, Color{20, 30, 20, 255});
                DrawRectangle(x * TS + 4, y * TS + 8, 8 * f, 2, f > 0.25f ? Color{90, 230, 120, 255} : Color{240, 90, 60, 255});
            }
            if (OBJS[t.obj].structure && t.hp < OBJS[t.obj].hp && t.obj != O_DAM) {
                DrawRectangle(x * TS + 2, y * TS - 18, 12, 2, Color{40, 10, 10, 200});
                DrawRectangle(x * TS + 2, y * TS - 18, 12.f * t.hp / OBJS[t.obj].hp, 2, ORANGE);
            }
        } else if (d.type == 1) drawEntity(g->ents[d.a]);
        else drawPlayer();
    }

    drawFishing();
    // ---- fire
    for (int i : g->fireTiles) {
        int x = i % WW, y = i / WW;
        if (x < x0 || x > x1 || y < y0 || y > y1) continue;
        float f = g->fire[i];
        for (int k = 0; k < 3; k++) drawFlame(x * TS + 3 + k * 5, y * TS + 14, 0.5f + f * 0.8f, i * 1.3f + k * 2.1f);
    }
    // ---- particles
    for (auto& pt : g->parts) {
        float a = clampf(pt.life / pt.max, 0, 1);
        Color c = pt.c; c.a = (unsigned char)(c.a * a);
        DrawRectangle(pt.x * TS - pt.size / 2, pt.y * TS - pt.size / 2, pt.size, pt.size, c);
    }
    // ---- cursor & placement preview
    if (g->panel == P_NONE) {
        Vector2 wm = GetScreenToWorld2D(GetMousePosition(), g->cam);
        int tx = (int)std::floor(wm.x / TS), ty = (int)std::floor(wm.y / TS);
        if (inb(tx, ty)) {
            Stack& h = p.inv[p.sel];
            float d = dist(p.x, p.y, tx + 0.5f, ty + 0.5f);
            bool placeable = h.id && (ITEMS[h.id].placeObj >= 0 || ITEMS[h.id].placeGround >= 0);
            if (placeable && d < 4.5f) {
                Rectangle src = itemSrc(h.id);
                bool tall = src.height > 16;
                DrawTexturePro(atlas, src, Rectangle{(float)tx * TS, (float)ty * TS - (tall ? TS : 0), TS, tall ? TS * 2.f : TS}, {0, 0}, 0, Color{255, 255, 255, 140});
            }
            Color c = d < (placeable ? 4.5f : 2.3f) ? Color{255, 255, 255, 160} : Color{255, 255, 255, 50};
            DrawRectangleLinesEx(Rectangle{(float)tx * TS, (float)ty * TS, TS, TS}, 0.5f, c);
        }
    }
    // ---- floating text
    for (auto& f : g->floats) {
        int fs = 6;
        int tw = MeasureText(f.text.c_str(), fs);
        Color c = f.c; c.a = (unsigned char)(255 * clampf(f.life, 0, 1));
        DrawText(f.text.c_str(), f.x * TS - tw / 2 + 0.5f, f.y * TS - 10 + 0.5f, fs, Color{0, 0, 0, c.a});
        DrawText(f.text.c_str(), f.x * TS - tw / 2, f.y * TS - 10, fs, c);
    }
    EndMode2D();

    // ---- weather (screen space)
    float t = g->totalTime;
    if (g->weather == WX_RAIN || g->weather == WX_STORM) {
        int n = g->weather == WX_STORM ? 420 : 260;
        for (int k = 0; k < n; k++) {
            float sx = std::fmod(hash32(k) % 2000 + t * 120 - g->cam.target.x * g->zoom * 0.0f, (float)sw + 100) - 50;
            float sy = std::fmod(hash32(k * 7) % 2000 + t * 900, (float)sh + 40) - 20;
            DrawLineEx({sx, sy}, {sx - 4, sy + 16}, 1.5f, Color{170, 190, 230, 120});
        }
    }
    if (g->weather == WX_SNOW || g->weather == WX_BLIZZARD) {
        int n = g->weather == WX_BLIZZARD ? 700 : 300;
        float wind = g->weather == WX_BLIZZARD ? 400 : 30;
        for (int k = 0; k < n; k++) {
            float sx = std::fmod(hash32(k) % 3000 + t * wind + std::sin(t + k) * 20, (float)sw + 40) - 20;
            float sy = std::fmod(hash32(k * 13) % 3000 + t * (60 + k % 5 * 15), (float)sh + 20) - 10;
            float s = 2 + k % 3;
            DrawRectangle(sx, sy, s, s, Color{245, 248, 255, 220});
        }
        if (g->weather == WX_BLIZZARD) DrawRectangle(0, 0, sw, sh, Color{220, 230, 245, 110});
    }
    if (g->weather == WX_FOG) DrawRectangle(0, 0, sw, sh, Color{200, 205, 210, 90});

    // ---- lighting
    float dl = sunLevel();
    Color night{38, 46, 86, 255}, dusk{255, 170, 130, 255}, day{255, 255, 255, 255};
    Color amb;
    if (dl >= 1) amb = day;
    else if (dl > 0.5f) { float k = (dl - 0.5f) / 0.5f; amb = Color{(unsigned char)(dusk.r + (day.r - dusk.r) * k), (unsigned char)(dusk.g + (day.g - dusk.g) * k), (unsigned char)(dusk.b + (day.b - dusk.b) * k), 255}; }
    else { float k = dl / 0.5f; amb = Color{(unsigned char)(night.r + (dusk.r - night.r) * k), (unsigned char)(night.g + (dusk.g - night.g) * k), (unsigned char)(night.b + (dusk.b - night.b) * k), 255}; }
    float wmul = 1;
    switch (g->weather) { case WX_CLOUDY: wmul = 0.88f; break; case WX_RAIN: wmul = 0.75f; break; case WX_STORM: wmul = 0.6f; break; case WX_BLIZZARD: wmul = 0.8f; break; case WX_FOG: wmul = 0.85f; break; default: break; }
    if (g->season() == 3) { amb.r = (unsigned char)(amb.r * 0.92f); amb.g = (unsigned char)(amb.g * 0.96f); }
    amb = Color{(unsigned char)(amb.r * wmul), (unsigned char)(amb.g * wmul), (unsigned char)(amb.b * wmul), 255};
    if (g->flash > 0) amb = Color{(unsigned char)std::min(255.f, amb.r + 255 * g->flash), (unsigned char)std::min(255.f, amb.g + 255 * g->flash), (unsigned char)std::min(255.f, amb.b + 255 * g->flash), 255};

    if (amb.r < 250 || amb.g < 250 || amb.b < 250) {
        BeginTextureMode(lightRT);
        ClearBackground(amb);
        BeginBlendMode(BLEND_ADDITIVE);
        auto light = [&](float wx, float wy, float radiusTiles, Color c) {
            Vector2 sp = GetWorldToScreen2D({wx * TS, wy * TS}, g->cam);
            float r = radiusTiles * TS * g->zoom;
            if (sp.x < -r || sp.y < -r || sp.x > sw + r || sp.y > sh + r) return;
            DrawCircleGradient((int)sp.x, (int)sp.y, r, c, Color{0, 0, 0, 255});
        };
        float flick = 1 + std::sin(t * 13) * 0.04f + std::sin(t * 7.3f) * 0.03f;
        light(p.x, p.y - 0.4f, 2.6f, Color{90, 90, 100, 255});
        if (p.inv[p.sel].id == I_TORCH) light(p.x, p.y - 0.6f, 6.5f * flick, Color{255, 170, 90, 255});
        for (int y = std::max(0, y0 - 8); y <= std::min(WH - 1, y1 + 8); y++)
            for (int x = std::max(0, x0 - 10); x <= std::min(WW - 1, x1 + 10); x++) {
                uint8_t o = g->tiles[IDX(x, y)].obj;
                if (!o || OBJS[o].light <= 0) continue;
                if (o == O_LAMP && (!poweredAt(x, y) || sunLevel() >= 0.3f)) continue;
                if (o == O_HEATER && (!poweredAt(x, y) || airTemp() >= 16)) continue;
                bool fireish = o == O_CAMPFIRE || o == O_TORCHPOST || o == O_BONFIRE || o == O_KILN;
                light(x + 0.5f, y + 0.3f, OBJS[o].light * (fireish ? flick : 1), OBJS[o].lightCol);
            }
        int fl = 0;
        for (int i : g->fireTiles) {
            if (fl++ > 120) break;
            light(i % WW + 0.5f, i / WW + 0.5f, 3.5f * flick, Color{255, 130, 60, 255});
        }
        for (auto& e : g->ents) if (e.alive && e.kind == E_RAIDER && e.torch && e.state >= 1) light(e.x, e.y - 1, 5 * flick, Color{255, 150, 80, 255});
        EndBlendMode();
        EndTextureMode();
        BeginBlendMode(BLEND_MULTIPLIED);
        DrawTextureRec(lightRT.texture, Rectangle{0, 0, (float)lightW, -(float)lightH}, {0, 0}, WHITE);
        EndBlendMode();
    }
    // vignette
    for (int k = 0; k < 6; k++) DrawRectangleLinesEx(Rectangle{(float)k * 6, (float)k * 6, (float)sw - k * 12, (float)sh - k * 12}, 6, Color{0, 0, 0, (unsigned char)(40 - k * 6)});
    if (p.hurtFlash > 0) DrawRectangle(0, 0, sw, sh, Color{200, 0, 0, (unsigned char)(p.hurtFlash * 200)});
    if (p.body < 35) DrawRectangle(0, 0, sw, sh, Color{180, 220, 255, (unsigned char)clampf((35 - p.body) * 50, 0, 120)});
}
