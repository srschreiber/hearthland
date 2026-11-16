// Procedural pixel-art atlas. Everything is drawn in code at startup.
#include "game.h"
#include <cstring>

Texture2D atlas;
static const int AW = 1024, AH = 1024;
static Color* PX = nullptr;
static int CX = 0, CY = 0, CW = 16, CH = 16;

// layout
static const int GROUND_Y = 0;                 // 16x16 cells, (g*4+season)*4+var
static const int OBJ_Y = 64;                    // 16x32 cells, (obj*4+season)*4+var
static const int ITEM_Y = OBJ_Y + 32 * 20;      // 16x16 cells
static const int ENT_Y = ITEM_Y + 16 * 3;       // 24x24 cells, ((kind*6+frame)*4+look)
static const int ENT_KINDS = E_KCOUNT + 1;      // +1 for the player

static Color rgb(int r, int g, int b, int a = 255) { return Color{(unsigned char)clampf(r, 0, 255), (unsigned char)clampf(g, 0, 255), (unsigned char)clampf(b, 0, 255), (unsigned char)a}; }
static Color mul(Color c, float f) { return rgb(int(c.r * f), int(c.g * f), int(c.b * f), c.a); }
static Color mix(Color a, Color b, float t) { return rgb(int(a.r + (b.r - a.r) * t), int(a.g + (b.g - a.g) * t), int(a.b + (b.b - a.b) * t), int(a.a + (b.a - a.a) * t)); }
static float hn(int x, int y, int s) { return (hash32(uint32_t(x * 7919 + y * 104729 + s * 15485863)) & 0xFFFF) / 65535.f; }

static void cell(int ox, int oy, int w, int h) { CX = ox; CY = oy; CW = w; CH = h; }
static void P(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || c.a == 0) return;
    PX[(CY + y) * AW + CX + x] = c;
}
static Color G(int x, int y) {
    if (x < 0 || y < 0 || x >= CW || y >= CH) return BLANK;
    return PX[(CY + y) * AW + CX + x];
}
static void rect(int x, int y, int w, int h, Color c, bool shade = true) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            float f = shade ? 1.12f - 0.28f * (h > 1 ? j / float(h - 1) : 0) : 1.f;
            P(x + i, y + j, mul(c, f));
        }
}
static void ell(float cx, float cy, float rx, float ry, Color c, bool shade = true) {
    for (int y = int(cy - ry - 1); y <= int(cy + ry + 1); y++)
        for (int x = int(cx - rx - 1); x <= int(cx + rx + 1); x++) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy > 1) continue;
            float f = shade ? 1.f + 0.22f * (-dx * 0.4f - dy * 0.7f) : 1.f;
            P(x, y, mul(c, f));
        }
}
static void line(int x0, int y0, int x1, int y1, Color c) {
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        P(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void outline(Color oc = Color{24, 18, 22, 255}) {
    std::vector<uint8_t> solid(CW * CH);
    for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) solid[y * CW + x] = G(x, y).a > 0;
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            if (solid[y * CW + x]) continue;
            bool adj = (x > 0 && solid[y * CW + x - 1]) || (x < CW - 1 && solid[y * CW + x + 1]) || (y > 0 && solid[(y - 1) * CW + x]) || (y < CH - 1 && solid[(y + 1) * CW + x]);
            if (adj) P(x, y, oc);
        }
}
static void speckle(int x, int y, int w, int h, Color c, float density, int seed) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (hn(x + i, y + j, seed) < density) P(x + i, y + j, c);
}

// ---------------------------------------------------------------- ground
static Color grassCol(int s) {
    switch (s) { case 0: return rgb(96, 168, 70); case 1: return rgb(74, 146, 58); case 2: return rgb(158, 148, 66); default: return rgb(226, 234, 242); }
}
static void drawGround(int gr, int s, int v) {
    int seed = gr * 31 + v * 7 + 1;
    auto fillNoise = [&](Color a, Color b, float amt) {
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            float n = hn(x, y, seed);
            P(x, y, mix(a, b, n < amt ? 1.f : 0.f));
        }
    };
    bool snow = s == 3;
    switch (gr) {
        case G_DEEP: {
            Color c = snow ? rgb(70, 110, 150) : rgb(36, 74, 140);
            fillNoise(c, mul(c, 0.9f), 0.3f);
            for (int k = 0; k < 3; k++) { int x = (int)(hn(k, v, 3) * 12), y = (int)(hn(k, v, 4) * 15); rect(x, y, 3, 1, mul(c, 1.25f), false); }
            break;
        }
        case G_WATER: {
            Color c = snow ? rgb(170, 205, 230) : rgb(62, 128, 200);
            fillNoise(c, mul(c, 1.06f), 0.3f);
            for (int k = 0; k < 4; k++) {
                int x = (int)(hn(k, 0, 5) * 13), y = ((int)(hn(k, 0, 6) * 16) + v * 2) % 16;
                rect(x, y, 3, 1, snow ? rgb(230, 245, 255) : rgb(140, 200, 245), false);
            }
            break;
        }
        case G_SAND: fillNoise(rgb(222, 202, 144), rgb(206, 184, 126), 0.25f); speckle(0, 0, 16, 16, rgb(240, 225, 175), 0.05f, seed + 2); break;
        case G_GRASS: {
            Color c = grassCol(s);
            fillNoise(c, mul(c, snow ? 0.96f : 0.9f), 0.35f);
            for (int k = 0; k < 5; k++) {
                int x = (int)(hn(k, v, 8) * 15), y = (int)(hn(k, v, 9) * 14) + 1;
                Color t = snow ? rgb(120, 140, 110) : mul(c, 1.2f);
                if (!snow || k < 2) { P(x, y, t); P(x + 1, y - 1, t); }
            }
            if (s == 0 && v == 2) { P(4, 5, rgb(255, 240, 120)); P(11, 10, rgb(240, 160, 200)); }
            break;
        }
        case G_FOREST: {
            Color c = snow ? rgb(205, 214, 222) : (s == 2 ? rgb(120, 100, 52) : rgb(62, 112, 52));
            fillNoise(c, mul(c, 0.82f), 0.35f);
            Color leaf = s == 2 ? rgb(190, 110, 40) : (snow ? rgb(150, 160, 150) : rgb(92, 76, 44));
            speckle(0, 0, 16, 16, leaf, 0.07f, seed + 3);
            break;
        }
        case G_DIRT: fillNoise(snow ? rgb(200, 200, 205) : rgb(132, 98, 64), snow ? rgb(170, 160, 150) : rgb(116, 84, 54), 0.35f); speckle(0, 0, 16, 16, rgb(150, 140, 130), 0.03f, seed + 4); break;
        case G_STONE: fillNoise(snow ? rgb(205, 210, 220) : rgb(120, 120, 128), snow ? rgb(180, 185, 195) : rgb(104, 104, 112), 0.4f); line(2, 4 + v, 7, 6 + v, rgb(90, 90, 98)); break;
        case G_TILLED: case G_GREENHOUSE: {
            Color c = rgb(98, 66, 42);
            if (snow && gr == G_TILLED) c = rgb(200, 196, 196);
            fillNoise(c, mul(c, 0.86f), 0.4f);
            for (int y = 2; y < 16; y += 4) rect(0, y, 16, 1, mul(c, 0.7f), false);
            if (gr == G_GREENHOUSE) {
                for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { Color o = G(x, y); P(x, y, mix(o, rgb(180, 230, 240), 0.18f)); }
                rect(0, 0, 16, 1, rgb(220, 240, 245), false); rect(0, 0, 1, 16, rgb(220, 240, 245), false);
                P(3, 3, rgb(255, 255, 255)); P(4, 2, rgb(255, 255, 255));
            }
            break;
        }
        case G_ROAD: {
            fillNoise(rgb(150, 136, 112), rgb(130, 118, 98), 0.4f);
            for (int y = 0; y < 16; y += 4)
                for (int x = (y / 4) % 2 * 2; x < 16; x += 5) { rect(x, y, 4, 3, rgb(160, 152, 140)); P(x + 3, y + 2, rgb(110, 100, 90)); }
            if (snow) speckle(0, 0, 16, 16, rgb(230, 235, 240), 0.3f, seed);
            break;
        }
        case G_BURNT: fillNoise(rgb(52, 46, 42), rgb(38, 34, 32), 0.4f); speckle(0, 0, 16, 16, rgb(90, 84, 78), 0.06f, seed + 5); break;
        case G_FLOOR:
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
                Color c = rgb(156, 108, 62);
                if (y % 4 == 3) c = rgb(110, 74, 42);
                else if (hn(x, y, seed) < 0.2f) c = rgb(146, 100, 58);
                if ((x + (y / 4) * 5) % 16 == 0) c = rgb(110, 74, 42);
                P(x, y, c);
            }
            break;
        case G_VILLAGE: fillNoise(snow ? rgb(220, 220, 225) : rgb(176, 148, 104), snow ? rgb(200, 200, 205) : rgb(160, 132, 92), 0.35f); break;
        default: fillNoise(MAGENTA, PURPLE, 0.5f);
    }
}

// ---------------------------------------------------------------- objects (16x32, base on bottom 16 rows)
static void tree(int kind, int s, int v) {
    Color trunk = (kind == O_BIRCH || kind == O_ASPEN) ? rgb(225, 222, 210) : kind == O_MAPLE ? rgb(120, 100, 90) : rgb(110, 74, 44);
    bool small = kind == O_SERVICEBERRY || kind == O_MTNASH || kind == O_APRICOT;
    rect(6, 20, small ? 3 : 4, 11, trunk);
    if (kind == O_BIRCH || kind == O_ASPEN) { P(7, 23, rgb(40, 40, 40)); P(8, 27, rgb(40, 40, 40)); P(6, 25, rgb(40, 40, 40)); }
    P(5, 30, mul(trunk, 0.8f)); P(10, 30, mul(trunk, 0.8f));
    int lean = (v % 2) ? 1 : 0;
    if (kind == O_PINE) {
        Color c = rgb(36, 96, 62);
        for (int k = 0; k < 4; k++) {
            int w = 4 + k * 2, y = 4 + k * 4;
            for (int j = 0; j < 6; j++) {
                int ww = w * (j + 1) / 6;
                rect(8 - ww + lean, y + j, ww * 2, 1, mul(c, 1.1f - j * 0.05f - k * 0.04f), false);
            }
        }
        if (s == 3) for (int k = 0; k < 4; k++) { int w = 3 + k * 2; rect(8 - w + lean, 5 + k * 4, w * 2 - 1, 1, rgb(240, 245, 250), false); }
        return;
    }
    Color leaf;
    if (kind == O_OAK) leaf = s == 0 ? rgb(110, 180, 70) : s == 1 ? rgb(70, 140, 56) : s == 2 ? (v % 2 ? rgb(200, 90, 40) : rgb(214, 140, 50)) : rgb(0, 0, 0, 0);
    else if (kind == O_MAPLE) leaf = s == 0 ? rgb(120, 180, 80) : s == 1 ? rgb(80, 150, 60) : s == 2 ? (v % 2 ? rgb(210, 40, 40) : rgb(230, 70, 40)) : rgb(0, 0, 0, 0);
    else if (kind == O_MTNASH) leaf = s == 0 ? rgb(130, 190, 90) : s == 1 ? rgb(90, 160, 70) : s == 2 ? rgb(230, 120, 40) : rgb(0, 0, 0, 0);
    else if (kind == O_ASPEN) leaf = s == 0 ? rgb(160, 205, 100) : s == 1 ? rgb(120, 180, 80) : s == 2 ? rgb(245, 205, 50) : rgb(0, 0, 0, 0);
    else if (kind == O_SERVICEBERRY) leaf = s == 0 ? rgb(245, 245, 250) : s == 1 ? rgb(90, 150, 70) : s == 2 ? rgb(190, 60, 70) : rgb(0, 0, 0, 0);
    else if (kind == O_APPLE) leaf = s == 0 ? rgb(250, 215, 225) : s == 1 ? rgb(80, 150, 60) : s == 2 ? rgb(170, 160, 60) : rgb(0, 0, 0, 0);
    else if (kind == O_PEAR) leaf = s == 0 ? rgb(250, 250, 250) : s == 1 ? rgb(70, 140, 60) : s == 2 ? rgb(200, 90, 50) : rgb(0, 0, 0, 0);
    else if (kind == O_APRICOT) leaf = s == 0 ? rgb(250, 180, 200) : s == 1 ? rgb(90, 155, 65) : s == 2 ? rgb(220, 160, 60) : rgb(0, 0, 0, 0);
    else leaf = s == 0 ? rgb(150, 196, 90) : s == 1 ? rgb(110, 170, 70) : s == 2 ? rgb(230, 190, 60) : rgb(0, 0, 0, 0);
    if (s == 3) {
        // bare branches with snow
        line(8, 20, 3 + lean, 9, trunk); line(8, 18, 13 + lean, 8, trunk); line(8, 20, 8, 5, trunk);
        line(5, 13, 2, 10, trunk); line(11, 12, 14, 11, trunk);
        P(3 + lean, 8, rgb(240, 245, 250)); P(13 + lean, 7, rgb(240, 245, 250)); P(8, 4, rgb(240, 245, 250));
        return;
    }
    ell(8 + lean, 12, 7.5f, 7.5f, mul(leaf, 0.85f));
    ell(5 + lean, 10, 4.5f, 4.5f, leaf);
    ell(11 + lean, 9, 4.5f, 4.5f, leaf);
    ell(8 + lean, 6, 4.5f, 4, mul(leaf, 1.1f));
    speckle(1, 2, 14, 16, mul(leaf, 1.25f), 0.06f, v + kind);
    if (kind == O_MTNASH && (s == 1 || s == 2)) for (int k = 0; k < 6; k++) { int x = 3 + (int)(hn(k, v, 7) * 10), y = 6 + (int)(hn(k, v, 8) * 10); P(x, y, rgb(230, 40, 30)); P(x + 1, y, rgb(200, 30, 30)); P(x, y + 1, rgb(200, 30, 30)); }
    if (kind == O_SERVICEBERRY && s == 1) for (int k = 0; k < 6; k++) { int x = 3 + (int)(hn(k, v, 5) * 10), y = 6 + (int)(hn(k, v, 6) * 10); P(x, y, rgb(90, 40, 100)); }
    if (kind == O_ASPEN) speckle(1, 2, 14, 16, mul(leaf, 1.35f), 0.08f, v + 99);   // shimmering leaves
    if (s == 0 && kind == O_OAK && v == 1) { P(4, 8, rgb(255, 220, 230)); P(11, 6, rgb(255, 220, 230)); P(9, 12, rgb(255, 220, 230)); }
}

static void drawObj(int o, int s, int v) {
    bool snow = s == 3;
    Color wood = rgb(150, 104, 60), dark = rgb(90, 60, 36), stone = rgb(138, 138, 146);
    switch (o) {
        case O_OAK: case O_PINE: case O_BIRCH: case O_MAPLE: case O_MTNASH: case O_ASPEN: case O_SERVICEBERRY:
        case O_APPLE: case O_PEAR: case O_APRICOT: tree(o, s, v); break;
        case O_GRAPEVINE:
            rect(2, 16, 2, 15, wood); rect(12, 16, 2, 15, wood); line(2, 18, 13, 18, rgb(120, 120, 120)); line(2, 23, 13, 23, rgb(120, 120, 120));
            line(8, 30, 8, 18, rgb(100, 70, 50)); line(8, 22, 3, 18, rgb(100, 70, 50)); line(8, 21, 13, 18, rgb(100, 70, 50));
            if (s != 3) for (int k = 0; k < 9; k++) ell(3 + (k % 5) * 2.5f, 17 + (k / 5) * 5, 1.8f, 1.5f, s == 2 ? rgb(200, 170, 60) : rgb(80, 150, 60));
            break;
        case O_BLACKBERRY: case O_RASPBERRY: {
            Color c = s == 3 ? rgb(110, 80, 70) : s == 2 ? rgb(150, 90, 60) : (o == O_BLACKBERRY ? rgb(50, 110, 50) : rgb(80, 150, 70));
            for (int k = 0; k < 6; k++) line(3 + k * 2, 31, 2 + k * 2 + (k % 3), 20 + k % 2 * 2, mul(o == O_BLACKBERRY ? rgb(120, 50, 60) : rgb(150, 90, 70), 1.f));
            if (s != 3) { ell(8, 25, 6.5f, 4.5f, c); ell(6, 23, 3, 2.5f, mul(c, 1.15f)); }
            if (o == O_BLACKBERRY) speckle(2, 21, 12, 9, rgb(200, 190, 190), 0.05f, v);   // thorns
            break;
        }
        case O_WILDPOTATO: for (int k = 0; k < 4; k++) ell(4 + k * 3, 26 - (k % 2) * 2, 2.5f, 2, s == 3 ? rgb(120, 110, 90) : rgb(90, 150, 70)); if (s <= 1) { P(6, 23, WHITE); P(10, 22, rgb(220, 200, 240)); } break;
        case O_MINE:
            ell(8, 26, 7.5f, 5, rgb(120, 116, 112));
            ell(8, 27, 4.5f, 4, rgb(20, 16, 14), false);
            rect(2, 18, 2, 12, rgb(110, 76, 44)); rect(12, 18, 2, 12, rgb(110, 76, 44)); rect(2, 17, 12, 2, rgb(130, 90, 54));
            line(5, 31, 11, 31, rgb(90, 90, 96)); line(6, 30, 6, 31, rgb(90, 90, 96)); line(10, 30, 10, 31, rgb(90, 90, 96));
            break;
        case O_BIRDHOUSE:
            rect(7, 18, 2, 13, rgb(110, 76, 44));
            rect(4, 10, 8, 8, rgb(170, 120, 70));
            for (int k = 0; k < 5; k++) rect(8 - k - 1, 5 + k, (k + 1) * 2, 1, rgb(140, 60, 50), false);
            ell(8, 13, 1.5f, 1.5f, rgb(30, 20, 15), false);
            break;
        case O_BEEHIVE:
            rect(4, 27, 2, 4, rgb(90, 60, 36)); rect(10, 27, 2, 4, rgb(90, 60, 36));
            rect(3, 17, 10, 4, rgb(235, 220, 170)); rect(3, 21, 10, 3, rgb(220, 200, 150)); rect(3, 24, 10, 3, rgb(235, 220, 170));
            rect(2, 15, 12, 2, rgb(150, 110, 70)); rect(6, 25, 4, 1, rgb(40, 30, 20), false);
            break;
        case O_IRONORE: ell(8, 26, 6.5f, 4.5f, rgb(110, 104, 100)); P(5, 25, rgb(170, 80, 50)); P(6, 26, rgb(150, 70, 40)); P(10, 24, rgb(170, 80, 50)); P(9, 27, rgb(150, 70, 40)); P(11, 26, rgb(170, 80, 50)); break;
        case O_QUARTZ: ell(8, 27, 6.5f, 4, rgb(120, 120, 128)); line(6, 26, 5, 21, rgb(240, 240, 255)); line(8, 26, 9, 20, rgb(225, 230, 250)); line(10, 27, 12, 23, rgb(240, 240, 255)); P(9, 21, WHITE); break;
        case O_FLOWERS: {
            static const Color pal[5] = {rgb(250, 200, 50), rgb(150, 110, 220), rgb(240, 120, 160), rgb(250, 250, 250), rgb(230, 90, 60)};   // balsamroot, lupine, ...
            for (int k = 0; k < 12; k++) {
                int x = 2 + (int)(hn(k, v, 1) * 12), y = 21 + (int)(hn(k, v, 2) * 9);
                line(x, y + 2, x, y, s == 3 ? rgb(130, 110, 80) : rgb(70, 140, 60));
                if (s <= 1) { P(x, y - 1, pal[(k + v) % 5]); P(x + 1, y - 1, mul(pal[(k + v) % 5], 0.85f)); }
                else if (s == 2) P(x, y - 1, rgb(170, 120, 70));
            }
            break;
        }
        case O_WATERFALL:
            ell(8, 28, 7.5f, 3, rgb(60, 120, 190), false);
            ell(4, 18, 3.5f, 4.5f, rgb(128, 128, 136)); ell(11, 15, 4, 6, rgb(118, 118, 126)); ell(8, 22, 5, 3.5f, rgb(140, 140, 148));
            ell(12, 9, 2.5f, 2, rgb(90, 140, 70)); P(3, 13, rgb(90, 140, 70));
            break;
        case O_SAPLING: line(8, 30, 8, 22, rgb(110, 80, 50)); ell(8, 22, 3, 2.5f, s == 3 ? rgb(140, 120, 90) : rgb(110, 180, 80)); break;
        case O_STUMP: rect(5, 25, 6, 5, rgb(120, 82, 50)); ell(8, 25, 3, 1.4f, rgb(190, 150, 100), false); P(8, 25, rgb(140, 100, 60)); break;
        case O_BUSH: {
            Color c = s == 2 ? rgb(150, 120, 50) : (snow ? rgb(120, 110, 100) : rgb(60, 130, 60));
            ell(8, 25, 6.5f, 5, c);
            ell(6, 23, 3.5f, 3, mul(c, 1.15f));
            if ((v & 4) == 0 && !snow) for (int k = 0; k < 5; k++) { int x = 4 + (int)(hn(k, v, 1) * 8), y = 21 + (int)(hn(k, v, 2) * 7); P(x, y, rgb(200, 40, 70)); P(x + 1, y, rgb(150, 20, 50)); }
            if (snow) rect(4, 20, 8, 1, rgb(240, 245, 250), false);
            break;
        }
        case O_ROCK: ell(8, 26, 6.5f, 4.5f, snow ? rgb(170, 175, 185) : stone); ell(6, 24, 3, 2, mul(stone, 1.2f)); if (snow) rect(4, 21, 7, 1, rgb(240, 245, 250), false); break;
        case O_ORE: ell(8, 26, 6.5f, 4.5f, rgb(110, 110, 118)); P(5, 25, rgb(70, 200, 150)); P(6, 25, rgb(70, 200, 150)); P(10, 27, rgb(220, 130, 60)); P(9, 23, rgb(70, 200, 150)); P(11, 25, rgb(220, 130, 60)); break;
        case O_CLAY: ell(8, 27, 6, 3, rgb(176, 104, 76)); ell(7, 26, 3, 1.5f, rgb(200, 130, 100)); break;
        case O_TALLGRASS: {
            Color c = s == 0 ? rgb(110, 190, 80) : s == 1 ? rgb(90, 170, 60) : s == 2 ? rgb(190, 170, 80) : rgb(170, 160, 120);
            for (int k = 0; k < 7; k++) { int x = 2 + k * 2; int h = 5 + (int)(hn(k, v, 3) * 5); line(x, 30, x + (k % 2 ? 1 : -1), 30 - h, mul(c, 0.85f + k % 3 * 0.1f)); }
            break;
        }
        case O_REEDS: for (int k = 0; k < 5; k++) { int x = 3 + k * 2; line(x, 30, x, 18 + k % 3, rgb(90, 140, 70)); rect(x, 17 + k % 3, 1, 3, rgb(110, 76, 50), false); } break;
        case O_BRANCH: line(3, 28, 13, 25, rgb(120, 86, 50)); line(8, 27, 10, 29, rgb(120, 86, 50)); break;
        case O_FLINT: ell(8, 28, 2.5f, 1.8f, rgb(60, 60, 80)); P(7, 27, rgb(140, 140, 170)); break;
        case O_MANURE: ell(8, 28, 3.5f, 2, rgb(100, 70, 36)); ell(8, 26.5f, 2, 1.5f, rgb(110, 80, 42)); break;
        case O_EGG: ell(8, 28, 2, 2.5f, rgb(245, 238, 220)); break;
        case O_ASH: ell(8, 28, 6, 2.5f, rgb(70, 66, 64), false); speckle(3, 26, 10, 4, rgb(120, 115, 110), 0.3f, v); break;
        case O_CAMPFIRE:
            for (int k = 0; k < 8; k++) { float a = k * 0.785f; ell(8 + std::cos(a) * 5, 27 + std::sin(a) * 2.5f, 1.6f, 1.3f, stone); }
            line(5, 28, 11, 25, dark); line(5, 25, 11, 28, dark);
            break;
        case O_TORCHPOST: rect(7, 16, 2, 15, wood); rect(6, 14, 4, 3, dark); break;
        case O_WALL:
            rect(0, 8, 16, 24, wood);
            for (int y = 8; y < 32; y += 4) rect(0, y, 16, 1, dark, false);
            rect(0, 4, 16, 5, rgb(176, 128, 80));
            if (snow) rect(0, 3, 16, 2, rgb(240, 245, 250), false);
            break;
        case O_STONEWALL:
            rect(0, 8, 16, 24, stone);
            for (int y = 8; y < 32; y += 4) { rect(0, y, 16, 1, rgb(90, 90, 98), false); for (int x = (y / 4 % 2) * 4; x < 16; x += 8) rect(x, y, 1, 4, rgb(90, 90, 98), false); }
            rect(0, 4, 16, 5, rgb(170, 170, 178));
            if (snow) rect(0, 3, 16, 2, rgb(240, 245, 250), false);
            break;
        case O_DOOR:
            rect(0, 4, 16, 5, rgb(176, 128, 80));
            rect(0, 8, 2, 24, dark); rect(14, 8, 2, 24, dark);
            rect(2, 10, 12, 22, rgb(130, 86, 48));
            for (int x = 4; x < 14; x += 3) rect(x, 10, 1, 22, dark, false);
            P(11, 21, rgb(220, 200, 80));
            break;
        case O_BED: rect(1, 18, 14, 13, dark); rect(2, 19, 12, 4, rgb(240, 236, 220)); rect(2, 23, 12, 7, rgb(170, 50, 50)); rect(2, 23, 12, 1, rgb(200, 80, 80), false); break;
        case O_CHEST: rect(2, 20, 12, 10, wood); rect(2, 20, 12, 3, mul(wood, 1.2f)); rect(2, 24, 12, 1, dark, false); rect(7, 23, 2, 3, rgb(220, 190, 80)); break;
        case O_GRANARY:
            rect(3, 26, 2, 5, dark); rect(11, 26, 2, 5, dark);
            rect(2, 14, 12, 13, wood);
            for (int y = 14; y < 27; y += 3) rect(2, y, 12, 1, dark, false);
            for (int k = 0; k < 8; k++) rect(8 - k - 1, 6 + k, (k + 1) * 2, 1, mix(rgb(210, 180, 90), rgb(170, 140, 60), k / 8.f), false);
            if (snow) for (int k = 0; k < 4; k++) rect(8 - k - 1, 6 + k, (k + 1) * 2, 1, rgb(240, 245, 250), false);
            break;
        case O_CELLAR: ell(8, 25, 7.5f, 6, snow ? rgb(220, 225, 230) : rgb(100, 140, 70)); rect(4, 22, 8, 9, stone); rect(5, 23, 6, 8, rgb(110, 70, 40)); break;
        case O_WORKBENCH: rect(1, 21, 14, 4, rgb(176, 128, 80)); rect(2, 25, 2, 6, dark); rect(12, 25, 2, 6, dark); rect(4, 19, 4, 2, stone); line(9, 20, 13, 18, dark); break;
        case O_KILN: ell(8, 22, 7, 9, rgb(176, 90, 66)); for (int y = 15; y < 31; y += 3) rect(2, y, 12, 1, rgb(140, 70, 50), false); ell(8, 27, 3, 3, rgb(40, 20, 16), false); ell(8, 28, 2, 1.5f, rgb(255, 150, 60), false); rect(6, 10, 4, 4, rgb(150, 76, 56)); break;
        case O_LOOM: rect(2, 16, 2, 15, wood); rect(12, 16, 2, 15, wood); rect(2, 16, 12, 2, dark); rect(2, 26, 12, 2, dark); for (int x = 4; x < 12; x += 2) line(x, 18, x, 26, rgb(220, 210, 240)); rect(4, 21, 8, 3, rgb(200, 190, 230)); break;
        case O_TANNER: line(2, 31, 4, 16, wood); line(14, 31, 12, 16, wood); rect(3, 16, 10, 1, dark); rect(4, 17, 8, 10, rgb(170, 130, 90)); P(4, 17, dark); P(11, 17, dark); break;
        case O_DRYRACK: line(2, 31, 2, 18, wood); line(14, 31, 14, 18, wood); rect(2, 18, 13, 1, dark); for (int x = 4; x < 14; x += 3) rect(x, 19, 2, 6, rgb(150, 60, 50)); break;
        case O_WELL: ell(8, 27, 6.5f, 3.5f, stone); ell(8, 27, 4, 2, rgb(30, 50, 90), false); rect(2, 14, 2, 13, wood); rect(12, 14, 2, 13, wood); for (int k = 0; k < 5; k++) rect(8 - k - 3, 10 + k, (k + 3) * 2, 1, rgb(150, 70, 50), false); line(8, 15, 8, 23, rgb(200, 200, 200)); break;
        case O_FENCE: rect(1, 20, 2, 11, wood); rect(13, 20, 2, 11, wood); rect(0, 22, 16, 2, mul(wood, 1.1f)); rect(0, 27, 16, 2, mul(wood, 1.1f)); if (snow) { P(1, 19, WHITE); P(2, 19, WHITE); P(13, 19, WHITE); P(14, 19, WHITE); } break;
        case O_GATE: rect(0, 18, 2, 13, dark); rect(14, 18, 2, 13, dark); rect(2, 21, 12, 2, wood); rect(2, 27, 12, 2, wood); line(2, 28, 13, 22, wood); break;
        case O_TROUGH: rect(1, 24, 14, 6, wood); rect(2, 24, 12, 2, rgb(210, 190, 110)); rect(2, 30, 2, 1, dark); rect(12, 30, 2, 1, dark); break;
        case O_KENNEL: rect(2, 20, 12, 11, rgb(170, 110, 60)); for (int k = 0; k < 6; k++) rect(8 - k - 2, 14 + k, (k + 2) * 2, 1, rgb(160, 50, 40), false); ell(8, 27, 3, 4, rgb(30, 20, 20), false); break;
        case O_COOP: rect(2, 20, 12, 9, rgb(200, 170, 110)); for (int k = 0; k < 6; k++) rect(8 - k - 2, 14 + k, (k + 2) * 2, 1, rgb(120, 80, 50), false); rect(6, 24, 3, 4, rgb(40, 30, 20), false); line(9, 28, 14, 31, wood); rect(3, 29, 2, 2, dark); break;
        case O_POND: ell(8, 26, 7.5f, 4.5f, rgb(110, 90, 60), false); ell(8, 26, 6.5f, 3.5f, snow ? rgb(190, 220, 240) : rgb(60, 130, 190), false); if (!snow) { ell(5, 26, 1.5f, 1, rgb(80, 160, 70), false); P(10, 25, rgb(160, 210, 240)); } break;
        case O_LEAFPILE: ell(8, 27, 6.5f, 4, rgb(180, 110, 50)); speckle(2, 23, 12, 7, rgb(210, 150, 60), 0.3f, v); speckle(2, 23, 12, 7, rgb(140, 80, 40), 0.15f, v + 1); break;
        case O_COMPOST: rect(2, 20, 12, 11, wood); for (int x = 2; x < 14; x += 3) rect(x, 20, 1, 11, dark, false); rect(3, 20, 10, 2, rgb(70, 50, 30), false); break;
        case O_DAM: rect(0, 18, 16, 13, rgb(120, 116, 112)); for (int y = 18; y < 31; y += 3) rect(0, y, 16, 1, rgb(90, 88, 86), false); rect(0, 18, 16, 3, wood); rect(0, 30, 16, 2, rgb(150, 200, 240), false); break;
        case O_WHEEL: ell(8, 21, 7.5f, 9, wood, false); ell(8, 21, 5.5f, 7, rgb(0, 0, 0, 0), false);
            for (int y = 12; y <= 30; y++) for (int x = 1; x <= 15; x++) { float dx = (x + 0.5f - 8) / 7.5f, dy = (y + 0.5f - 21) / 9; float d = dx * dx + dy * dy; if (d > 1) continue; if (d > 0.55f) P(x, y, mul(wood, 0.9f + (x + y) % 3 * 0.1f)); }
            line(8, 12, 8, 30, dark); line(1, 21, 15, 21, dark); line(3, 15, 13, 27, dark); line(13, 15, 3, 27, dark); ell(8, 21, 1.5f, 1.5f, rgb(220, 130, 70)); break;
        case O_SOLAR: rect(7, 24, 2, 7, rgb(150, 150, 160)); for (int y = 0; y < 9; y++) rect(1 + y / 3, 14 + y, 14, 1, mix(rgb(50, 80, 160), rgb(90, 140, 220), y / 9.f), false); for (int x = 4; x < 16; x += 4) line(x, 14, x + 2, 22, rgb(170, 190, 230)); rect(1, 18, 15, 1, rgb(170, 190, 230), false); break;
        case O_BATTERY: rect(2, 20, 12, 10, rgb(70, 74, 84)); rect(2, 20, 12, 2, rgb(100, 104, 114)); rect(4, 18, 2, 2, rgb(220, 130, 70)); rect(10, 18, 2, 2, rgb(220, 130, 70)); rect(5, 24, 6, 2, rgb(90, 230, 120)); break;
        case O_LAMP: rect(7, 12, 2, 19, rgb(70, 70, 80)); rect(4, 30, 8, 1, rgb(70, 70, 80)); rect(5, 6, 6, 6, rgb(255, 240, 180)); rect(4, 5, 8, 1, rgb(70, 70, 80)); rect(4, 12, 8, 1, rgb(70, 70, 80)); break;
        case O_HEATER: rect(2, 20, 12, 11, rgb(170, 84, 60)); for (int y = 20; y < 31; y += 3) rect(2, y, 12, 1, rgb(130, 60, 44), false); rect(4, 23, 8, 4, rgb(255, 120, 50)); rect(5, 24, 6, 2, rgb(255, 200, 90)); break;
        case O_SPRINKLER: rect(7, 24, 2, 7, rgb(220, 130, 70)); rect(5, 23, 6, 2, rgb(150, 150, 160)); P(4, 22, rgb(140, 200, 255)); P(11, 22, rgb(140, 200, 255)); break;
        case O_FRIDGE: rect(2, 8, 12, 23, rgb(200, 210, 220)); rect(2, 18, 12, 1, rgb(140, 150, 160), false); rect(11, 11, 1, 5, rgb(120, 130, 140)); rect(11, 21, 1, 6, rgb(120, 130, 140)); rect(4, 10, 3, 2, rgb(120, 200, 255)); break;
        case O_BIOGAS: ell(8, 24, 7.5f, 7, rgb(110, 130, 90)); rect(1, 24, 15, 7, rgb(170, 90, 66)); for (int y = 25; y < 31; y += 3) rect(1, y, 15, 1, rgb(140, 70, 50), false); rect(7, 14, 2, 4, rgb(220, 130, 70)); break;
        case O_TRADEPOST: rect(1, 18, 2, 13, dark); rect(13, 18, 2, 13, dark); rect(1, 24, 14, 5, wood); for (int x = 0; x < 16; x += 4) rect(x, 12, 4, 6, (x / 4) % 2 ? rgb(240, 236, 220) : rgb(70, 150, 220)); rect(5, 25, 2, 2, rgb(230, 190, 80)); rect(9, 25, 3, 2, rgb(200, 60, 60)); break;
        case O_TENT:
            if (v & 1) { // raider: ragged hide
                for (int k = 0; k < 14; k++) rect(8 - k / 2 - 1, 14 + k, (k / 2 + 1) * 2, 1, mix(rgb(90, 60, 50), rgb(60, 40, 34), k / 14.f), false);
                line(8, 10, 8, 16, rgb(220, 210, 190)); line(6, 11, 10, 11, rgb(220, 210, 190)); rect(6, 20, 1, 6, rgb(170, 30, 30), false); rect(10, 22, 1, 5, rgb(170, 30, 30), false); ell(8, 28, 2, 3, rgb(20, 10, 10), false);
            } else {
                for (int k = 0; k < 16; k++) rect(8 - k / 2 - 1, 14 + k, (k / 2 + 1) * 2, 1, mix(rgb(240, 230, 200), rgb(210, 196, 160), k / 16.f), false);
                for (int k = 4; k < 16; k += 4) rect(8 - k / 2 - 1, 14 + k, (k / 2 + 1) * 2, 1, rgb(70, 140, 210), false);
                ell(8, 28, 2, 3, rgb(60, 40, 30), false);
            }
            break;
        case O_HUT: ell(8, 24, 7.5f, 7, rgb(170, 140, 100)); for (int k = 0; k < 9; k++) rect(8 - k - 1, 9 + k, (k + 1) * 2, 1, mix(rgb(220, 190, 100), rgb(180, 150, 70), k / 9.f), false); rect(6, 25, 4, 6, rgb(70, 50, 30)); break;
        case O_BANNER: {
            Color c = v == 0 ? rgb(70, 150, 220) : rgb(230, 190, 60);
            rect(4, 4, 1, 27, dark); rect(5, 5, 9, 11, c); for (int x = 5; x < 14; x += 2) P(x, 16, c);
            ell(9, 10, 2, 2, rgb(255, 255, 255), false);  // trade emblem: coin
            P(9, 10, c);
            break;
        }
        case O_TOTEM: rect(6, 8, 4, 23, rgb(80, 60, 50)); ell(8, 8, 3.5f, 3.5f, rgb(230, 220, 200)); P(7, 8, BLACK); P(9, 8, BLACK); rect(7, 10, 3, 1, BLACK, false); rect(4, 14, 8, 1, rgb(170, 30, 30), false); rect(4, 19, 8, 1, rgb(170, 30, 30), false); break;
        case O_SKULLPOLE: rect(7, 10, 2, 21, rgb(70, 50, 40)); ell(8, 9, 3, 3, rgb(230, 220, 200)); P(7, 9, BLACK); P(9, 9, BLACK); line(4, 6, 6, 8, rgb(230, 220, 200)); line(12, 6, 10, 8, rgb(230, 220, 200)); break;
        case O_BONFIRE: for (int k = 0; k < 10; k++) { float a = k * 0.628f; ell(8 + std::cos(a) * 6.5f, 27 + std::sin(a) * 3, 1.6f, 1.3f, stone); } line(3, 29, 13, 24, dark); line(3, 24, 13, 29, dark); line(8, 23, 8, 30, dark); break;
        case O_PUMP: rect(3, 20, 10, 10, rgb(70, 110, 160)); rect(3, 20, 10, 2, rgb(110, 150, 200)); ell(8, 25, 2.5f, 2.5f, rgb(200, 200, 210)); rect(12, 26, 4, 2, rgb(150, 150, 160)); rect(6, 17, 4, 3, rgb(220, 130, 70)); break;
        case O_DRIP: line(1, 28, 14, 28, rgb(60, 60, 64)); line(1, 29, 14, 29, rgb(40, 40, 44)); for (int x = 3; x < 14; x += 4) P(x, 30, rgb(120, 190, 255)); rect(6, 25, 3, 3, rgb(90, 90, 96)); break;
        default: rect(4, 20, 8, 8, MAGENTA);
    }
}

// ---------------------------------------------------------------- entities (24x24, feet at y=22)
static void legs(int x0, int x1, int y, int len, int frame, Color c) {
    int off = frame == 1 ? 1 : (frame == 2 ? -1 : 0);
    rect(x0, y, 2, len + (off > 0), c); rect(x1, y, 2, len + (off < 0), c);
    if (frame) { rect(x0 + off, y + len - 1, 2, 1, c, false); }
}

static void human(Color shirt, Color pants, Color skin, Color hair, int frame, int style) {
    int bob = frame == 1 ? -1 : 0;
    // legs
    int l = frame == 1 ? 1 : (frame == 2 ? -1 : 0);
    rect(9 + l, 17, 2, 5, pants); rect(13 - l, 17, 2, 5, pants);
    rect(9 + l, 21, 2, 1, rgb(50, 36, 30), false); rect(13 - l, 21, 2, 1, rgb(50, 36, 30), false);
    // body
    rect(8, 11 + bob, 8, 7, shirt);
    rect(7, 12 + bob, 1, 4, shirt); rect(16, 12 + bob, 1, 4, mul(shirt, 0.85f));
    P(7, 16 + bob, skin); P(16, 16 + bob, skin);
    rect(8, 17 + bob, 8, 1, mul(pants, 0.8f), false);
    // head
    rect(9, 4 + bob, 7, 7, skin);
    P(14, 7 + bob, rgb(30, 20, 20)); P(12, 7 + bob, rgb(30, 20, 20));
    switch (style) {
        case 0: rect(9, 3 + bob, 7, 2, hair); rect(9, 5 + bob, 2, 3, hair); break;          // short hair
        case 1: rect(8, 3 + bob, 8, 2, hair); rect(8, 5 + bob, 2, 6, hair); break;          // long hair
        case 2: rect(8, 2 + bob, 9, 2, rgb(200, 170, 90)); rect(10, 1 + bob, 5, 1, rgb(200, 170, 90)); rect(9, 4 + bob, 2, 2, hair); break; // straw hat
        case 3: // raider: spiked hair, bone mask, war paint
            for (int k = 0; k < 4; k++) line(9 + k * 2, 4 + bob, 9 + k * 2, 1 + bob - (k % 2), hair);
            rect(11, 6 + bob, 5, 3, rgb(225, 215, 195)); P(12, 7 + bob, rgb(20, 10, 10)); P(14, 7 + bob, rgb(20, 10, 10));
            rect(9, 9 + bob, 7, 1, rgb(170, 30, 30), false);
            line(9, 12 + bob, 15, 16 + bob, rgb(170, 30, 30));
            break;
        case 4: rect(9, 3 + bob, 7, 2, hair); rect(9, 5 + bob, 2, 3, hair); rect(8, 11 + bob, 8, 2, rgb(150, 50, 50), false); break; // player: scarf
    }
}

static void drawEnt(int kind, int frame, int look) {
    bool sheared = frame >= 3;
    int f = frame % 3;
    switch (kind) {
        case E_SHEEP: {
            Color wool = sheared ? rgb(210, 190, 180) : (look == 3 ? rgb(90, 80, 76) : rgb(238, 234, 226));
            legs(7, 14, 17, 5, f, rgb(60, 50, 50));
            if (sheared) ell(11, 14, 6, 4, wool); else { ell(11, 13, 7.5f, 5.5f, wool); ell(8, 11, 3, 3, mul(wool, 1.05f)); ell(13, 10.5f, 3.5f, 3, mul(wool, 1.05f)); }
            ell(18, 11, 2.5f, 3, rgb(60, 50, 50)); P(19, 10, rgb(230, 230, 230)); rect(16, 9, 2, 1, rgb(60, 50, 50));
            break;
        }
        case E_DOG: {
            Color c = look == 0 ? rgb(170, 120, 70) : look == 1 ? rgb(50, 46, 46) : look == 2 ? rgb(210, 170, 90) : rgb(230, 225, 215);
            legs(7, 14, 16, 6, f, mul(c, 0.8f));
            ell(11, 14, 6, 3.2f, c);
            ell(17, 10, 3, 2.8f, c); rect(19, 11, 3, 2, mul(c, 0.9f)); P(21, 11, BLACK); P(18, 9, BLACK);
            rect(15, 7, 2, 2, mul(c, 0.7f));
            line(5, 13, 3, 9 + (f == 1), c);
            if (look == 3) { P(10, 13, rgb(60, 50, 40)); P(12, 14, rgb(60, 50, 40)); }
            break;
        }
        case E_WOLF: {
            Color c = look % 2 ? rgb(130, 130, 140) : rgb(105, 100, 104);
            legs(6, 14, 16, 6, f, mul(c, 0.8f));
            ell(11, 13.5f, 7, 3.6f, c); ell(8, 12, 3, 2.5f, mul(c, 1.15f));
            ell(18, 10, 3, 2.8f, c); rect(20, 11, 3, 2, mul(c, 0.9f)); P(22, 11, BLACK); P(19, 9, rgb(240, 210, 60));
            line(16, 7, 16, 5, mul(c, 0.7f)); line(18, 7, 19, 5, mul(c, 0.7f));
            line(4, 12, 1, 15, c); line(4, 13, 2, 16, c);
            break;
        }
        case E_BEAR: {
            Color c = look % 2 ? rgb(110, 76, 48) : rgb(80, 56, 40);
            legs(5, 15, 15, 7, f, mul(c, 0.8f)); rect(8, 15, 3, 6, mul(c, 0.8f));
            ell(11, 12, 9, 6, c); ell(9, 9, 5, 4, mul(c, 1.1f));
            ell(19, 11, 3.5f, 3.5f, c); rect(21, 12, 2, 2, rgb(180, 150, 120)); P(22, 12, BLACK); P(20, 10, BLACK);
            ell(18, 7.5f, 1.2f, 1.2f, c);
            break;
        }
        case E_DUCK: {
            Color body = look == 1 ? rgb(150, 120, 90) : rgb(240, 240, 232);
            rect(10, 19, 1, 3, rgb(240, 160, 40), false); rect(13, 19, 1, 3, rgb(240, 160, 40), false);
            ell(11, 16, 5, 3.5f, body);
            ell(15, 11, 2.5f, 2.5f, look == 1 ? rgb(40, 120, 70) : body);
            rect(17, 11, 3, 1, rgb(240, 160, 40), false); P(16, 10, BLACK);
            P(6, 14, mul(body, 0.8f));
            break;
        }
        case E_FOWL: {
            Color body = look % 2 ? rgb(200, 90, 40) : rgb(170, 60, 30);
            rect(10, 18, 1, 4, rgb(200, 170, 80), false); rect(13, 18, 1, 4, rgb(200, 170, 80), false);
            ell(11, 15, 4.5f, 3.5f, body);
            for (int k = 0; k < 5; k++) line(7, 14, 3 + k, 8 + k, rgb(30, 80, 60));
            ell(15, 10, 2, 2.2f, rgb(220, 120, 50)); rect(15, 7, 2, 2, rgb(220, 40, 40), false); P(16, 13, rgb(220, 40, 40));
            P(18, 10, rgb(200, 170, 80)); P(16, 9, BLACK);
            break;
        }
        case E_BUFFALO: {
            Color c = look % 2 ? rgb(70, 60, 58) : rgb(56, 50, 50);
            legs(5, 15, 15, 7, f, mul(c, 0.8f)); rect(8, 15, 2, 6, mul(c, 0.8f));
            ell(11, 12, 9, 5.5f, c); ell(8, 10, 5, 4, mul(c, 1.15f));
            ell(19, 12, 3.2f, 3.5f, c); P(21, 13, BLACK); P(19, 11, BLACK);
            line(17, 9, 15, 6, rgb(210, 200, 180)); line(15, 6, 16, 5, rgb(210, 200, 180));
            line(21, 9, 23, 6, rgb(210, 200, 180)); line(23, 6, 22, 5, rgb(210, 200, 180));
            break;
        }
        case E_HORSE: {
            Color c = look == 0 ? rgb(150, 100, 60) : look == 1 ? rgb(90, 60, 40) : look == 2 ? rgb(220, 210, 200) : rgb(60, 50, 50);
            Color mane = look == 2 ? rgb(170, 160, 150) : rgb(40, 30, 26);
            legs(6, 15, 13, 9, f, mul(c, 0.8f)); rect(9, 13, 2, 8, mul(c, 0.75f));
            ell(11, 11, 7.5f, 4, c);
            line(16, 9, 19, 3, c); line(17, 9, 20, 3, c); line(18, 9, 21, 4, c);
            ell(20.5f, 4.5f, 2.2f, 2, c); rect(21, 5, 2, 2, mul(c, 0.9f)); P(21, 4, BLACK);
            line(16, 7, 19, 2, mane); line(4, 9, 2, 15, mane); line(4, 10, 3, 16, mane);
            break;
        }
        case E_FROG: ell(12, 19, 3, 2.2f, rgb(80, 160, 70)); P(10, 17, rgb(240, 240, 120)); P(13, 17, rgb(240, 240, 120)); rect(8, 20, 2, 1, rgb(60, 120, 50)); rect(14, 20, 2, 1, rgb(60, 120, 50)); break;
        case E_PORCUPINE: {
            Color c = rgb(92, 72, 56);
            rect(8 + (f == 1), 19, 2, 3, mul(c, 0.7f)); rect(13 - (f == 1), 19, 2, 3, mul(c, 0.7f));
            ell(11, 16, 6, 4.5f, c);
            for (int k = 0; k < 9; k++) {   // quills
                float a = 3.4f + k * 0.33f;
                line(11 + (int)(std::cos(a) * 3), 16 + (int)(std::sin(a) * 2), 11 + (int)(std::cos(a) * 8), 16 + (int)(std::sin(a) * 7), k % 2 ? rgb(225, 215, 195) : rgb(70, 56, 46));
            }
            ell(17, 18, 2.2f, 2, rgb(110, 90, 70)); P(18, 17, BLACK); P(19, 18, rgb(40, 30, 30));
            break;
        }
        case E_COYOTE: {
            Color c = look % 2 ? rgb(185, 155, 115) : rgb(160, 135, 100);
            legs(7, 13, 16, 6, f, mul(c, 0.8f));
            ell(11, 14, 5.5f, 3, c); ell(9, 13, 2.5f, 2, rgb(220, 205, 180));
            ell(17, 11, 2.6f, 2.3f, c); rect(19, 12, 3, 1, mul(c, 0.85f)); P(21, 12, BLACK); P(18, 10, rgb(230, 200, 60));
            line(16, 9, 15, 5, mul(c, 0.7f)); line(18, 9, 18, 5, mul(c, 0.7f));   // big ears
            line(5, 14, 2, 17, mul(c, 0.9f)); P(2, 18, rgb(40, 34, 30));
            break;
        }
        case E_SPIDER: ell(12, 19, 2, 1.6f, rgb(50, 40, 40)); for (int k = 0; k < 4; k++) { line(11, 19, 8 - (f == 1 ? 1 : 0), 17 + k, rgb(40, 30, 30)); line(13, 19, 16 + (f == 2 ? 1 : 0), 17 + k, rgb(40, 30, 30)); } break;
        case E_WORKER: {
            Color shirts[4] = {rgb(90, 130, 190), rgb(170, 110, 60), rgb(120, 160, 90), rgb(190, 170, 110)};
            Color hair[4] = {rgb(60, 40, 30), rgb(200, 160, 90), rgb(30, 26, 24), rgb(150, 70, 40)};
            Color skin[4] = {rgb(236, 196, 160), rgb(200, 150, 110), rgb(150, 100, 70), rgb(230, 180, 140)};
            human(shirts[look], rgb(80, 70, 60), skin[look], hair[look], f, look == 1 ? 2 : look % 2);
            break;
        }
        case E_VILLAGER: {
            Color shirts[4] = {rgb(60, 140, 210), rgb(230, 190, 60), rgb(240, 236, 220), rgb(70, 160, 160)};
            Color skin[4] = {rgb(210, 160, 120), rgb(160, 110, 80), rgb(236, 196, 160), rgb(190, 140, 100)};
            human(shirts[look], rgb(110, 90, 70), skin[look], rgb(40, 30, 26), f, look % 2);
            rect(8, 14 + (f == 1 ? -1 : 0), 8, 1, rgb(230, 200, 80), false);  // sash marks a friendly village
            break;
        }
        case E_RAIDER: {
            Color skin[4] = {rgb(150, 110, 90), rgb(120, 90, 80), rgb(170, 130, 100), rgb(110, 84, 70)};
            human(rgb(70, 46, 40), rgb(50, 36, 32), skin[look], rgb(30, 20, 18), f, 3);
            line(3, 4, 6, 21, rgb(110, 80, 50)); line(3, 4, 2, 2, rgb(200, 200, 210));
            break;
        }
        case E_KCOUNT:  // player
            human(rgb(70, 120, 70), rgb(80, 66, 50), rgb(236, 196, 160), rgb(110, 70, 40), f, 4);
            break;
        default: rect(6, 6, 12, 12, MAGENTA);
    }
}

// ---------------------------------------------------------------- items
static void drawItem(int id) {
    const ItemDef& d = ITEMS[id];
    Color a = d.c1, b = d.c2;
    switch (d.icon) {
        case IC_BLOB:
            if (id == I_BERRIES) { ell(6, 9, 3, 3, a); ell(10, 10, 3, 3, b); ell(8, 6, 3, 3, a); P(8, 3, rgb(60, 130, 60)); }
            else if (id == I_POTATO) { ell(8, 9, 5.5f, 4, a); P(6, 8, b); P(10, 10, b); }
            else if (id == I_CABBAGE) { ell(8, 9, 6, 5.5f, b); ell(8, 9, 4, 3.5f, a); line(8, 5, 8, 12, mul(b, 0.8f)); }
            else if (id == I_WOOL) { ell(6, 9, 4, 3.5f, a); ell(10, 8, 4, 3.5f, a); ell(8, 11, 4, 3, b); }
            else { ell(8, 9, 5.5f, 4.5f, a); speckle(4, 6, 8, 6, b, 0.2f, id); }
            break;
        case IC_LOG: ell(8, 9, 6, 4, a); ell(12, 9, 2.5f, 3.5f, rgb(210, 170, 110)); P(12, 9, b); rect(2, 6, 9, 1, mul(a, 1.2f), false); break;
        case IC_ROCK: ell(8, 10, 5.5f, 4, a); ell(7, 8, 3, 2, mul(a, 1.2f)); if (id == I_ORE) { P(6, 10, rgb(70, 200, 150)); P(10, 9, b); P(9, 12, rgb(70, 200, 150)); } break;
        case IC_SEED: for (int k = 0; k < 5; k++) ell(4 + (int)(hn(k, id, 1) * 8), 5 + (int)(hn(k, id, 2) * 7), 1.6f, 1.2f, k % 2 ? a : b); break;
        case IC_MEAT: ell(7, 7, 4.5f, 4, a); line(10, 10, 13, 13, rgb(240, 230, 210)); P(13, 12, rgb(240, 230, 210)); P(12, 13, rgb(240, 230, 210)); ell(6, 6, 2, 1.5f, mul(a, 1.2f)); break;
        case IC_BAR: for (int k = 0; k < 5; k++) rect(3 + k / 2, 6 + k, 10 - k, 1, mix(mul(a, 1.2f), b, k / 5.f), false); rect(3, 11, 11, 1, mul(b, 0.8f), false); break;
        case IC_SHEET: rect(3, 4, 10, 9, a); rect(3, 4, 10, 2, mul(a, 1.2f)); line(3, 12, 12, 4, b); break;
        case IC_TOOL: {
            Color handle = b, head = a;
            line(4, 13, 11, 4, handle); line(5, 13, 12, 4, mul(handle, 0.8f));
            switch (d.tool) {
                case T_AXE: ell(11, 5, 3, 3.5f, head); break;
                case T_PICK: line(7, 2, 14, 7, head); line(7, 3, 13, 8, head); break;
                case T_HOE: rect(10, 3, 5, 2, head); break;
                case T_SPEAR: line(11, 4, 14, 1, head); line(12, 4, 14, 2, head); break;
                case T_SHEARS: ell(5, 11, 2, 2, rgb(80, 80, 80), false); line(6, 10, 13, 3, head); line(8, 11, 14, 5, head); break;
                case T_SICKLE: for (int k = 0; k < 7; k++) P(9 + (int)(std::cos(k * 0.5f) * 4), 4 + (int)(std::sin(k * 0.5f) * 3) - 2, head); break;
            }
            break;
        }
        case IC_BUCKET: rect(4, 6, 8, 8, a); rect(4, 6, 8, 1, mul(a, 0.7f), false); rect(4, 10, 8, 1, mul(a, 0.7f), false); line(4, 6, 8, 2, rgb(90, 90, 90)); line(12, 6, 8, 2, rgb(90, 90, 90)); if (b.b > 200) rect(5, 6, 6, 2, b, false); break;
        case IC_COAT: rect(4, 4, 8, 10, a); rect(2, 5, 2, 7, a); rect(12, 5, 2, 7, mul(a, 0.85f)); line(8, 4, 8, 13, b); rect(6, 3, 4, 2, b); break;
        case IC_HAT: ell(8, 9, 5, 4, a); rect(3, 10, 10, 2, b); ell(8, 4, 1.5f, 1.5f, b); break;
        case IC_BOOTS: rect(3, 4, 4, 8, a); rect(3, 11, 6, 3, b); rect(9, 4, 4, 8, a); rect(9, 11, 6, 3, b); break;
        case IC_PILE: ell(8, 10, 6, 4, a); ell(8, 8, 4, 3, mul(a, 1.1f)); speckle(3, 6, 10, 7, b, 0.2f, id); break;
        case IC_EGG: ell(8, 9, 4, 5, a); P(6, 6, WHITE); break;
        case IC_JAR: ell(8, 10, 5, 4, rgb(180, 120, 80)); ell(8, 8, 4.5f, 1.8f, a, false); if (id == I_MILK) { rect(5, 4, 6, 9, rgb(230, 230, 230)); rect(6, 5, 4, 7, a); } break;
        case IC_STICK: line(4, 13, 12, 3, a); line(5, 13, 13, 3, b); if (id == I_TORCH) { ell(12, 3, 2, 2.5f, rgb(255, 200, 60)); P(12, 2, rgb(255, 250, 200)); } break;
        case IC_BREAD: ell(8, 9, 6, 4, a); for (int k = 0; k < 3; k++) line(5 + k * 3, 7, 6 + k * 3, 10, b); break;
        case IC_PLANT:
            if (id == I_SAPLING) { line(8, 14, 8, 6, rgb(110, 80, 50)); ell(8, 6, 3.5f, 3, a); ell(5, 9, 2, 1.5f, b); }
            else for (int k = 0; k < 5; k++) { line(4 + k * 2, 14, 6 + k, 3, k % 2 ? a : b); P(6 + k, 3, mul(a, 1.2f)); }
            break;
    }
    outline(Color{30, 22, 26, 255});
}

// ---------------------------------------------------------------- atlas build
Rectangle groundSrc(int gr, int season, int var) {
    int idx = (gr * 4 + season) * 4 + (var & 3);
    return Rectangle{float((idx % 64) * 16), float(GROUND_Y + (idx / 64) * 16), 16, 16};
}
Rectangle objSrc(int obj, int season, int var) {
    int idx = (obj * 4 + season) * 4 + (var & 3);
    if (obj == O_BUSH) idx = (obj * 4 + season) * 4 + ((var & 4) ? 3 : (var & 3) % 3);   // slot 3 = picked bush
    return Rectangle{float((idx % 64) * 16), float(OBJ_Y + (idx / 64) * 32), 16, 32};
}
Rectangle itemSrc(int item) {
    const ItemDef& d = ITEMS[item];
    if (d.placeObj >= 0) return objSrc(d.placeObj, 1, 0);
    if (d.placeGround >= 0) return groundSrc(d.placeGround, 1, 0);
    return Rectangle{float((item % 64) * 16), float(ITEM_Y + (item / 64) * 16), 16, 16};
}
Rectangle entSrc(int kind, int frame, int look) {
    int idx = (kind * 6 + frame) * 4 + (look & 3);
    return Rectangle{float((idx % 42) * 24), float(ENT_Y + (idx / 42) * 24), 24, 24};
}

void buildAtlas() {
    PX = new Color[AW * AH];
    std::memset(PX, 0, sizeof(Color) * AW * AH);
    for (int gr = 0; gr < G_COUNT; gr++)
        for (int s = 0; s < 4; s++)
            for (int v = 0; v < 4; v++) {
                Rectangle r = groundSrc(gr, s, v);
                cell((int)r.x, (int)r.y, 16, 16);
                drawGround(gr, s, v);
            }
    for (int o = 1; o < O_COUNT; o++)
        for (int s = 0; s < 4; s++)
            for (int v = 0; v < 4; v++) {
                int idx = (o * 4 + s) * 4 + v;
                cell((idx % 64) * 16, OBJ_Y + (idx / 64) * 32, 16, 32);
                int vv = v;
                if (o == O_BUSH && v == 3) vv = 4;           // slot 3 = picked
                if (o == O_TENT) vv = v;                     // var&1 = raider
                drawObj(o, s, vv);
                if (o != O_POND && o != O_ASH && o != O_DAM) outline();
            }
    for (int i = 1; i < I_COUNT; i++) {
        if (ITEMS[i].placeObj >= 0 || ITEMS[i].placeGround >= 0) continue;
        cell((i % 64) * 16, ITEM_Y + (i / 64) * 16, 16, 16);
        drawItem(i);
    }
    for (int k = 0; k < ENT_KINDS; k++)
        for (int f = 0; f < 6; f++)
            for (int l = 0; l < 4; l++) {
                if (f >= 3 && k != E_SHEEP) continue;
                Rectangle r = entSrc(k, f, l);
                cell((int)r.x, (int)r.y, 24, 24);
                drawEnt(k, f, l);
                outline();
            }
    Image img{PX, AW, AH, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    atlas = LoadTextureFromImage(img);
    SetTextureFilter(atlas, TEXTURE_FILTER_POINT);
    delete[] PX;
    PX = nullptr;
}

void unloadAtlas() { UnloadTexture(atlas); }
