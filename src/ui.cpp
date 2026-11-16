// HUD and panels. Immediate-mode: panels draw and handle their own clicks.
#include "game.h"
#include <algorithm>

static bool overUI = false, overUIPrev = false;
static Texture2D minimapTex{};
static float minimapTimer = 0;
static const Color PANEL{34, 27, 24, 238}, BORDER{120, 92, 62, 255}, SLOT{58, 46, 40, 255}, SLOT_HI{96, 76, 58, 255}, TEXT{240, 228, 205, 255}, DIM{170, 155, 135, 255};

void openPanel(Panel p) { g->panel = p; g->craftScroll = 0; }

static void returnCursor() {
    if (g->cursor.id) {
        int left = addItem(g->pl.inv, INV_SLOTS, g->cursor.id, g->cursor.n, g->cursor.fresh);
        if (left) storeAnywhere(g->cursor.id, left, g->cursor.fresh, g->pl.x, g->pl.y);
        g->cursor = Stack{};
    }
}

static void closePanel() { returnCursor(); g->panel = P_NONE; g->openCont = -1; }

static Rectangle used(Rectangle r) {
    if (CheckCollisionPointRec(GetMousePosition(), r)) overUI = true;
    return r;
}

static void panelBox(Rectangle r, const char* title) {
    used(r);
    DrawRectangleRec(Rectangle{r.x + 4, r.y + 4, r.width, r.height}, Color{0, 0, 0, 90});
    DrawRectangleRec(r, PANEL);
    DrawRectangleLinesEx(r, 3, BORDER);
    DrawRectangleLinesEx(Rectangle{r.x + 3, r.y + 3, r.width - 6, r.height - 6}, 1, Color{70, 54, 40, 255});
    if (title) {
        DrawText(title, r.x + 16, r.y + 12, 24, Color{255, 214, 140, 255});
    }
}

static bool button(Rectangle r, const char* text, bool enabled = true, int fs = 18) {
    used(r);
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = !enabled ? Color{50, 44, 40, 255} : hover ? Color{120, 90, 60, 255} : Color{82, 62, 46, 255};
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 2, enabled ? BORDER : Color{70, 60, 50, 255});
    int tw = MeasureText(text, fs);
    DrawText(text, r.x + (r.width - tw) / 2, r.y + (r.height - fs) / 2, fs, enabled ? TEXT : DIM);
    return enabled && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void bar(float x, float y, float w, float h, float v, Color c, const char* label) {
    DrawRectangle(x, y, w, h, Color{20, 16, 14, 220});
    DrawRectangle(x + 2, y + 2, (w - 4) * clampf(v, 0, 1), h - 4, c);
    DrawRectangle(x + 2, y + 2, (w - 4) * clampf(v, 0, 1), 3, Color{255, 255, 255, 50});
    DrawRectangleLines(x, y, w, h, Color{90, 70, 50, 255});
    if (label) DrawText(label, x + 8, y + (h - 16) / 2, 16, WHITE);
}

void drawItemIcon(int id, float x, float y, float size) {
    if (!id) return;
    Rectangle src = itemSrc(id);
    if (src.height > 16) DrawTexturePro(atlas, src, Rectangle{x + size * 0.2f, y, size * 0.6f, size * 1.0f}, {0, 0}, 0, WHITE);
    else DrawTexturePro(atlas, src, Rectangle{x, y, size, size}, {0, 0}, 0, WHITE);
}

static std::string itemTooltip(const Stack& s) {
    const ItemDef& d = ITEMS[s.id];
    std::string t = d.name;
    if (d.food > 0) t += TextFormat("  (food %d", (int)d.food) + std::string(d.drink > 0 ? TextFormat(", water %d)", (int)d.drink) : ")");
    if (d.spoilDays > 0) t += TextFormat("  fresh %d%%", (int)(s.fresh * 100));
    t += "\n";
    t += d.desc;
    return t;
}

static std::string hoverTip;

// One inventory slot. Left-click swaps with the cursor; shift-click transfers to `other`.
static void slotUI(Stack& s, Rectangle r, Stack* other, int otherN, bool selected = false) {
    used(r);
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? SLOT_HI : SLOT);
    DrawRectangleLinesEx(r, 2, selected ? Color{255, 210, 120, 255} : Color{30, 24, 20, 255});
    if (s.id) {
        drawItemIcon(s.id, r.x + 6, r.y + 6, r.width - 12);
        if (s.n > 1) {
            const char* n = TextFormat("%d", s.n);
            DrawText(n, r.x + r.width - MeasureText(n, 16) - 4, r.y + r.height - 18, 16, Color{0, 0, 0, 200});
            DrawText(n, r.x + r.width - MeasureText(n, 16) - 5, r.y + r.height - 19, 16, WHITE);
        }
        if (ITEMS[s.id].spoilDays > 0) {
            float f = clampf(s.fresh, 0, 1);
            Color c = f > 0.5f ? Color{120, 220, 100, 255} : f > 0.2f ? Color{230, 200, 70, 255} : Color{230, 80, 60, 255};
            DrawRectangle(r.x + 4, r.y + r.height - 5, (r.width - 8) * f, 2, c);
        }
        if (hover) hoverTip = itemTooltip(s);
    }
    if (!hover) return;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) && other && s.id) {
            int left = addItem(other, otherN, s.id, s.n, s.fresh);
            if (left == 0) s = Stack{}; else s.n = (int16_t)left;
        } else if (g->cursor.id && g->cursor.id == s.id) {
            int room = ITEMS[s.id].stack - s.n, put = std::min<int>(room, g->cursor.n);
            s.fresh = (s.fresh * s.n + g->cursor.fresh * put) / std::max(1, s.n + put);
            s.n += put; g->cursor.n -= put;
            if (g->cursor.n <= 0) g->cursor = Stack{};
        } else std::swap(s, g->cursor);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if (!g->cursor.id && s.id && s.n > 1) { g->cursor = s; g->cursor.n = s.n / 2; s.n -= g->cursor.n; }
        else if (g->cursor.id && (!s.id || s.id == g->cursor.id) && s.n < ITEMS[g->cursor.id].stack) {
            if (!s.id) { s = g->cursor; s.n = 0; }
            s.n++; if (--g->cursor.n <= 0) g->cursor = Stack{};
        }
    }
}

static void invGrid(float x, float y, float sz, Stack* other, int otherN) {
    for (int i = 0; i < INV_SLOTS; i++) {
        int cx = i % 10, cy = i / 10;
        slotUI(g->pl.inv[i], Rectangle{x + cx * (sz + 4), y + cy * (sz + 4), sz, sz}, other, otherN, i == g->pl.sel);
    }
}

// ---------------------------------------------------------------- crafting
static int stationObj(int st) {
    switch (st) {
        case ST_WORKBENCH: return O_WORKBENCH; case ST_CAMPFIRE: return O_CAMPFIRE; case ST_KILN: return O_KILN;
        case ST_LOOM: return O_LOOM; case ST_TANNER: return O_TANNER; case ST_DRYRACK: return O_DRYRACK; default: return 0;
    }
}
static bool nearStation(int st) {
    if (st == ST_NONE) return true;
    if (st == ST_CAMPFIRE && nearObj(g->pl.x, g->pl.y, O_BONFIRE, 3.5f)) return true;
    return nearObj(g->pl.x, g->pl.y, stationObj(st), 3.5f);
}
static bool canCraft(const Recipe& r) {
    for (auto& in : r.in) if (in.item && countItem(g->pl.inv, INV_SLOTS, in.item) < in.n) return false;
    return nearStation(r.station);
}
static bool craft(const Recipe& r) {
    if (!canCraft(r)) return false;
    float fresh = 1;
    for (auto& in : r.in) if (in.item) takeItem(g->pl.inv, INV_SLOTS, in.item, in.n);
    addToPlayer(r.out, r.n, fresh);
    if (r.out == I_COOKEDMEAT || r.out == I_BREAD || r.out == I_STEW || r.out == I_JERKY) g->stats.foodMade += ITEMS[r.out].food * r.n * 0.3f;
    return true;
}

static const char* CATS[] = {"Basics", "Food", "Materials", "Clothing", "Building", "Farm", "Power"};

static void craftingPanel(Rectangle area) {
    // category tabs
    float tx = area.x;
    for (int c = 0; c < 7; c++) {
        float w = MeasureText(CATS[c], 16) + 18;
        if (button(Rectangle{tx, area.y, w, 28}, CATS[c], true, 16)) { g->craftCat = c; g->craftScroll = 0; }
        if (g->craftCat == c) DrawRectangle(tx, area.y + 26, w, 3, Color{255, 210, 120, 255});
        tx += w + 4;
    }
    std::vector<const Recipe*> list;
    for (auto& r : RECIPES) if (std::string(r.cat) == CATS[g->craftCat]) list.push_back(&r);
    float y = area.y + 40;
    float rowH = 56;
    int visible = (int)((area.height - 40) / rowH);
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(GetMousePosition(), area) && wheel != 0) g->craftScroll = std::clamp(g->craftScroll - (int)wheel, 0, std::max(0, (int)list.size() - visible));
    for (int k = g->craftScroll; k < (int)list.size() && k < g->craftScroll + visible; k++) {
        const Recipe& r = *list[k];
        Rectangle row{area.x, y, area.width, rowH - 4};
        bool ok = canCraft(r);
        DrawRectangleRec(row, ok ? Color{60, 52, 40, 255} : Color{44, 36, 32, 255});
        drawItemIcon(r.out, row.x + 6, row.y + 4, 44);
        DrawText(TextFormat("%s%s", ITEMS[r.out].name, r.n > 1 ? TextFormat(" x%d", r.n) : ""), row.x + 58, row.y + 4, 18, ok ? TEXT : DIM);
        float ix = row.x + 58;
        for (auto& in : r.in) {
            if (!in.item) continue;
            int have = countItem(g->pl.inv, INV_SLOTS, in.item);
            drawItemIcon(in.item, ix, row.y + 26, 20);
            const char* t = TextFormat("%d/%d", have, in.n);
            DrawText(t, ix + 22, row.y + 30, 14, have >= in.n ? Color{160, 230, 140, 255} : Color{240, 120, 100, 255});
            ix += 30 + MeasureText(t, 14);
        }
        if (r.station) {
            const char* st = STATION_NAMES[r.station];
            DrawText(st, ix + 8, row.y + 30, 14, nearStation(r.station) ? Color{160, 230, 140, 255} : Color{240, 120, 100, 255});
        }
        if (CheckCollisionPointRec(GetMousePosition(), row)) hoverTip = std::string(ITEMS[r.out].name) + "\n" + ITEMS[r.out].desc;
        if (button(Rectangle{row.x + row.width - 90, row.y + 10, 80, 32}, "Craft", ok)) {
            int times = IsKeyDown(KEY_LEFT_SHIFT) ? 5 : 1;
            for (int t = 0; t < times; t++) if (!craft(r)) break;
        }
        y += rowH;
    }
    if ((int)list.size() > visible) DrawText("scroll for more", area.x + area.width - 130, area.y + area.height - 14, 14, DIM);
}

static void inventoryPanel() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 600, sh / 2.f - 330, 1200, 640};
    panelBox(r, "Inventory & Crafting");
    float sz = 50;
    DrawText("Backpack  (shift-click to move, right-click to split)", r.x + 16, r.y + 50, 16, DIM);
    invGrid(r.x + 16, r.y + 74, sz, nullptr, 0);
    // equipment
    float ey = r.y + 74 + 3 * (sz + 4) + 20;
    DrawText("Wearing", r.x + 16, ey, 18, TEXT);
    const char* names[4] = {"", "Head", "Body", "Feet"};
    for (int w = 1; w <= 3; w++) {
        Rectangle sr{r.x + 16 + (w - 1) * 140, ey + 26, sz, sz};
        Stack& s = g->pl.wear[w];
        // only allow matching clothing into slot
        Stack before = s;
        slotUI(s, sr, nullptr, 0);
        if (s.id && ITEMS[s.id].wear != w) { std::swap(s, g->cursor); if (before.id != s.id) s = before; }
        DrawText(names[w], sr.x + sz + 8, sr.y + 16, 16, DIM);
    }
    float insul = 0;
    for (auto& w : g->pl.wear) if (w.id) insul += ITEMS[w.id].insul;
    DrawText(TextFormat("Clothing warmth: +%d C", (int)insul), r.x + 16, ey + 90, 18, Color{255, 200, 150, 255});
    DrawText("Right-click clothing in the hotbar (or drag here) to wear it.", r.x + 16, ey + 116, 14, DIM);
    DrawText("Stations nearby unlock more recipes: workbench, campfire,\nkiln, loom, tanning rack, drying rack.", r.x + 16, ey + 140, 14, DIM);
    craftingPanel(Rectangle{r.x + 570, r.y + 50, 615, r.height - 64});
}

// ---------------------------------------------------------------- containers
static const char* contInfo(const Container& c) {
    bool indoor = g->indoor[IDX(c.x, c.y)];
    bool rain = g->weather == WX_RAIN || g->weather == WX_STORM;
    switch (c.kind) {
        case C_CHEST: return (!indoor && rain) ? "Chest - DAMP! Food molds fast outdoors in rain. Move it indoors." : "Chest - general storage.";
        case C_GRANARY: return "Granary - dry storage, food keeps ~3x longer.";
        case C_CELLAR: return "Root cellar - cool storage, food keeps ~4x longer.";
        case C_FRIDGE: return g->powered ? "Cold store - powered. Food barely spoils." : "Cold store - NO POWER. Food spoils normally!";
        case C_TROUGH: return "Feed trough - hay, wheat, cabbage for herbivores; meat for dogs.";
        case C_COMPOST: return "Compost bin - manure, leaves and moldy food become compost.";
        case C_BIOGAS: return "Biogas digester - manure makes power and compost.";
    }
    return "";
}

static void containerPanel() {
    if (g->openCont < 0 || g->openCont >= (int)g->conts.size() || !g->conts[g->openCont].alive) { closePanel(); return; }
    Container& c = g->conts[g->openCont];
    if (dist(g->pl.x, g->pl.y, c.x + 0.5f, c.y + 0.5f) > 4) { closePanel(); return; }
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 300, sh / 2.f - 300, 600, 560};
    panelBox(r, OBJS[g->tiles[IDX(c.x, c.y)].obj].name);
    DrawText(contInfo(c), r.x + 16, r.y + 46, 16, DIM);
    float sz = 50;
    int n = (int)c.slots.size();
    for (int i = 0; i < n; i++) {
        int cx = i % 9, cy = i / 9;
        slotUI(c.slots[i], Rectangle{r.x + 16 + cx * (sz + 12), r.y + 76 + cy * (sz + 6), sz, sz}, g->pl.inv, INV_SLOTS);
    }
    float iy = r.y + 76 + ((n + 8) / 9) * (sz + 6) + 20;
    DrawText("Backpack (shift-click to move)", r.x + 16, iy, 16, DIM);
    for (int i = 0; i < INV_SLOTS; i++) {
        int cx = i % 10, cy = i / 10;
        slotUI(g->pl.inv[i], Rectangle{r.x + 16 + cx * (sz + 6), iy + 24 + cy * (sz + 6), sz, sz}, c.slots.data(), n, i == g->pl.sel);
    }
}

// ---------------------------------------------------------------- trade
static float wantMul(int tribe, int id) {
    if (tribe == 0) return (id == I_WOOL || id == I_LEATHER || id == I_HIDE || id == I_JERKY || id == I_COOKEDMEAT) ? 1.35f : 1.f;
    return (id == I_BREAD || id == I_WHEAT || id == I_CLOTH || id == I_GLASS || id == I_EGG || id == I_CABBAGE) ? 1.35f : 1.f;
}
static int buyPrice(const Tribe& t, int id) { return (int)std::ceil(ITEMS[id].value * (t.road ? 1.2f : 1.6f)); }
static int sellPrice(const Tribe& t, int ti, int id, float fresh) {
    float v = ITEMS[id].value * (t.road ? 0.85f : 0.6f) * wantMul(ti, id);
    if (ITEMS[id].spoilDays > 0) v *= 0.4f + 0.6f * fresh;
    return std::max(id == I_MOLDY ? 0 : 1, (int)std::floor(v));
}

static void tradePanel() {
    if (g->openTribe < 0) { closePanel(); return; }
    Tribe& t = g->tribes[g->openTribe];
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 560, sh / 2.f - 320, 1120, 640};
    panelBox(r, TextFormat("Trading with the %s%s", t.name.c_str(), g->tradeRemote ? " (via road)" : ""));
    DrawText(TextFormat("Trade credit: %d", t.credit), r.x + r.width - 260, r.y + 16, 22, Color{255, 220, 120, 255});
    DrawText(t.road ? "Road connected: better prices." : "Build a road to their village and a trading post for better prices.", r.x + 16, r.y + 46, 16, DIM);
    DrawText(g->openTribe == 0 ? "They especially want: wool, leather, hides, jerky, cooked meat." : "They especially want: bread, wheat, cloth, glass, eggs, cabbage.", r.x + 16, r.y + 66, 16, Color{180, 220, 160, 255});
    // their stock
    DrawText("Their goods (click to buy)", r.x + 16, r.y + 96, 18, TEXT);
    float x = r.x + 16, y = r.y + 122;
    for (auto& s : t.stock) {
        if (!s.id || s.n <= 0) continue;
        int price = buyPrice(t, s.id);
        Rectangle row{x, y, 520, 40};
        used(row);
        bool can = t.credit >= price;
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        DrawRectangleRec(row, hover ? SLOT_HI : SLOT);
        drawItemIcon(s.id, row.x + 4, row.y + 4, 32);
        DrawText(TextFormat("%s  (%d left)", ITEMS[s.id].name, s.n), row.x + 44, row.y + 11, 18, TEXT);
        DrawText(TextFormat("%d", price), row.x + 470, row.y + 11, 18, can ? Color{255, 220, 120, 255} : Color{200, 100, 90, 255});
        if (hover) hoverTip = itemTooltip(s);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && can) {
            t.credit -= price;
            s.n--;
            addToPlayer(s.id, 1);
        }
        y += 44;
        if (y > r.y + r.height - 110) break;
    }
    // recruit
    int bed = freeBed();
    bool canRecruit = t.recruits > 0 && t.credit >= 60 && bed >= 0;
    if (button(Rectangle{r.x + 16, r.y + r.height - 60, 520, 44}, TextFormat("Recruit a worker  (60 credit, %d available)", t.recruits), canRecruit)) {
        t.credit -= 60; t.recruits--;
        recruitWorker(g->openTribe);
    }
    if (bed < 0) DrawText("Recruits need a free bed INDOORS at your farm.", r.x + 16, r.y + r.height - 80, 14, Color{240, 140, 110, 255});
    // your goods
    DrawText("Your goods (click to sell 1, shift-click sells the stack)", r.x + 560, r.y + 96, 18, TEXT);
    float sz = 48;
    for (int i = 0; i < INV_SLOTS; i++) {
        Stack& s = g->pl.inv[i];
        Rectangle sr{r.x + 560 + (i % 10) * (sz + 4), r.y + 124 + (i / 10) * (sz + 22), sz, sz};
        used(sr);
        bool hover = CheckCollisionPointRec(GetMousePosition(), sr);
        DrawRectangleRec(sr, hover ? SLOT_HI : SLOT);
        if (!s.id) continue;
        drawItemIcon(s.id, sr.x + 5, sr.y + 5, sz - 10);
        if (s.n > 1) DrawText(TextFormat("%d", s.n), sr.x + 4, sr.y + sz - 16, 14, WHITE);
        int price = sellPrice(t, g->openTribe, s.id, s.fresh);
        DrawText(TextFormat("%d", price), sr.x + 4, sr.y + sz + 2, 14, Color{255, 220, 120, 255});
        if (hover) hoverTip = itemTooltip(s) + TextFormat("\nSells for %d each", price);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && price > 0) {
            int n = IsKeyDown(KEY_LEFT_SHIFT) ? s.n : 1;
            t.credit += price * n;
            bool merged = false;
            for (auto& st : t.stock) if (st.id == s.id) { st.n += n; merged = true; break; }
            if (!merged) t.stock.push_back(Stack{s.id, (int16_t)n, 1});
            s.n -= n;
            if (s.n <= 0) s = Stack{};
        }
    }
    DrawText("Credit stays with this village for your next visit.", r.x + 560, r.y + r.height - 40, 16, DIM);
}

// ---------------------------------------------------------------- worker panel
static void workerPanel() {
    Ent* e = entById(g->openWorker);
    if (!e) { closePanel(); return; }
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 300, sh / 2.f - 250, 600, 500};
    panelBox(r, e->name.c_str());
    float y = r.y + 56;
    bar(r.x + 16, y, 270, 24, e->hp / e->maxhp, Color{200, 60, 60, 255}, "Health"); y += 30;
    bar(r.x + 16, y, 270, 24, e->hunger / 100, Color{220, 150, 60, 255}, "Food"); y += 30;
    bar(r.x + 16, y, 270, 24, e->thirst / 100, Color{70, 140, 230, 255}, "Water"); y += 30;
    bar(r.x + 16, y, 270, 24, e->mood / 100, Color{230, 200, 90, 255}, "Mood"); y += 40;
    DrawText(e->bed >= 0 ? "Has a bed indoors." : "NO BED - build an enclosed room with a bed!", r.x + 16, y, 18, e->bed >= 0 ? Color{160, 230, 140, 255} : Color{240, 120, 100, 255}); y += 26;
    int wx, wy;
    bool well = nearObj(e->x, e->y, O_WELL, 40, &wx, &wy);
    DrawText(well ? "Drinks from your well." : "No well - walks to open water (build a well).", r.x + 16, y, 18, well ? Color{160, 230, 140, 255} : Color{240, 200, 100, 255}); y += 26;
    int food = 0;
    for (int id : {I_STEW, I_COOKEDMEAT, I_BREAD, I_JERKY, I_EGG, I_MILK, I_CABBAGE, I_POTATO, I_BERRIES, I_BEANS, I_WHEAT}) food += storageCount(id);
    DrawText(TextFormat("Food in storage: %d items", food), r.x + 16, y, 18, food > 10 ? Color{160, 230, 140, 255} : Color{240, 120, 100, 255}); y += 40;
    DrawText("Job:", r.x + 16, y, 20, TEXT); y += 28;
    static const char* desc[J_COUNT] = {
        "Wanders near the base.",
        "Harvests, replants (with crop rotation), waters, fertilizes and expands fields.",
        "Fells trees near buildings first (firebreak), replants saplings further out.",
        "Fills troughs, shears, milks, gathers eggs and manure, makes hay.",
        "Cooks meat, bakes bread and makes stew at your campfire.",
        "Patrols and fights wolves and raiders."};
    for (int j = 0; j < J_COUNT; j++) {
        Rectangle b{r.x + 16 + (j % 3) * 190, y + (j / 3) * 46, 180, 40};
        if (button(b, JOB_NAMES[j], true)) e->job = (uint8_t)j;
        if (e->job == j) DrawRectangleLinesEx(b, 3, Color{255, 210, 120, 255});
        if (CheckCollisionPointRec(GetMousePosition(), b)) hoverTip = desc[j];
    }
    DrawText(desc[e->job], r.x + 16, y + 100, 16, DIM);
    if (button(Rectangle{r.x + r.width - 140, r.y + r.height - 54, 124, 40}, "Dismiss")) {
        logMsg(e->name + " leaves your farm.", ORANGE);
        e->alive = false;
        closePanel();
    }
}

// ---------------------------------------------------------------- goals & ledger
static void goalsPanel() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 600, sh / 2.f - 340, 1200, 680};
    panelBox(r, "Homestead - milestones & ledger");
    float y = r.y + 56;
    int done = 0;
    for (auto& gl : g->goals) done += gl.done;
    DrawText(TextFormat("Toward a modern sustainable farm: %d / %d", done, (int)g->goals.size()), r.x + 16, y, 18, Color{255, 220, 120, 255});
    y += 30;
    for (size_t k = 0; k < g->goals.size(); k++) {
        auto& gl = g->goals[k];
        Color c = gl.done ? Color{150, 230, 130, 255} : TEXT;
        DrawText(gl.done ? "[x]" : "[ ]", r.x + 16, y, 16, c);
        DrawText(gl.title, r.x + 56, y, 16, c);
        if (!gl.done) DrawText(gl.hint, r.x + 290, y, 14, DIM);
        y += 27;
    }
    // ledger
    float lx = r.x + 810, ly = r.y + 60;
    DrawText("Ledger", lx, ly, 22, Color{255, 214, 140, 255}); ly += 32;
    int workers = 0;
    int counts[E_KCOUNT] = {};
    for (auto& e : g->ents) if (e.alive) { if (e.kind == E_WORKER) workers++; if (e.tame) counts[e.kind]++; }
    DrawText(TextFormat("Workers: %d", workers), lx, ly, 18, TEXT); ly += 24;
    for (int k : {E_SHEEP, E_BUFFALO, E_DUCK, E_FOWL, E_HORSE, E_DOG})
        if (counts[k]) { DrawText(TextFormat("%s: %d", SPECIES[k].name, counts[k]), lx, ly, 18, TEXT); ly += 24; }
    ly += 8;
    DrawText(TextFormat("Food produced yesterday: %d", (int)g->stats.lastFoodMade), lx, ly, 18, TEXT); ly += 24;
    DrawText(TextFormat("Food eaten yesterday: %d", (int)g->stats.lastFoodEaten), lx, ly, 18, TEXT); ly += 24;
    DrawText(TextFormat("Today so far: +%d / -%d", (int)g->stats.foodMade, (int)g->stats.foodEaten), lx, ly, 18, DIM); ly += 32;
    int crops = 0, gh = 0, tilled = 0;
    for (int i = 0; i < WW * WH; i++) { if (g->crops[i].type && !g->crops[i].dead) { crops++; if (g->ground[i] == G_GREENHOUSE) gh++; } if (g->ground[i] == G_TILLED || g->ground[i] == G_GREENHOUSE) tilled++; }
    DrawText(TextFormat("Fields: %d tiles, %d growing (%d under glass)", tilled, crops, gh), lx, ly, 16, TEXT); ly += 24;
    DrawText(TextFormat("Power: %.0fW made, %.0fW used", g->powerGen, g->powerUse), lx, ly, 18, TEXT); ly += 24;
    if (g->powerCap > 0) { DrawText(TextFormat("Battery: %d%%", (int)(100 * g->powerStored / g->powerCap)), lx, ly, 18, TEXT); ly += 24; }
    ly += 8;
    DrawText(TextFormat("Livestock lost: %d", g->stats.livestockLost), lx, ly, 18, g->stats.livestockLost ? Color{240, 140, 110, 255} : TEXT); ly += 24;
    DrawText(TextFormat("Crops lost: %d", g->stats.cropsLost), lx, ly, 18, g->stats.cropsLost ? Color{240, 140, 110, 255} : TEXT); ly += 24;
    DrawText(TextFormat("Nights survived: %d", g->stats.nightsSurvived), lx, ly, 18, TEXT); ly += 24;
    DrawText(TextFormat("Compost made: %d", g->stats.composted), lx, ly, 18, TEXT); ly += 32;
    DrawText("Tribes:", lx, ly, 18, TEXT); ly += 24;
    for (auto& t : g->tribes) {
        DrawText(TextFormat("%s - %s%s", t.name.c_str(), t.hostile ? "HOSTILE raiders" : "traders", t.road ? " (road)" : ""), lx, ly, 16, t.hostile ? Color{240, 110, 100, 255} : Color{140, 200, 255, 255});
        ly += 22;
    }
}

// ---------------------------------------------------------------- minimap / map
static Color groundColor(int i) {
    uint8_t gr = g->ground[i];
    uint8_t o = g->tiles[i].obj;
    if (o && OBJS[o].structure) return Color{200, 140, 80, 255};
    if (o == O_OAK || o == O_PINE || o == O_BIRCH) return g->snowCover > 0.5f ? Color{120, 140, 130, 255} : Color{40, 100, 50, 255};
    if (g->fire[i] > 0) return Color{255, 120, 40, 255};
    switch (gr) {
        case G_DEEP: return Color{36, 70, 130, 255};
        case G_WATER: return Color{70, 130, 200, 255};
        case G_SAND: return Color{220, 200, 140, 255};
        case G_GRASS: return g->snowCover > 0.5f ? Color{225, 230, 240, 255} : Color{90, 160, 70, 255};
        case G_FOREST: return Color{60, 120, 55, 255};
        case G_DIRT: return Color{130, 98, 64, 255};
        case G_STONE: return Color{125, 125, 132, 255};
        case G_TILLED: case G_GREENHOUSE: return Color{110, 76, 50, 255};
        case G_ROAD: return Color{170, 160, 140, 255};
        case G_BURNT: return Color{50, 46, 42, 255};
        case G_FLOOR: return Color{170, 120, 70, 255};
        case G_VILLAGE: return Color{190, 160, 110, 255};
    }
    return BLACK;
}

static void refreshMinimap() {
    static std::vector<Color> px;
    px.resize(WW * WH);
    for (int i = 0; i < WW * WH; i++) px[i] = g->seen[i] ? groundColor(i) : Color{14, 12, 12, 255};
    if (minimapTex.id == 0) {
        Image img{px.data(), WW, WH, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        minimapTex = LoadTextureFromImage(img);
    } else UpdateTexture(minimapTex, px.data());
}

static void drawMapAt(Rectangle r, bool big) {
    DrawTexturePro(minimapTex, Rectangle{0, 0, WW, WH}, r, {0, 0}, 0, WHITE);
    float sx = r.width / WW, sy = r.height / WH;
    for (auto& t : g->tribes) {
        if (!g->seen[IDX(t.x, t.y)] && !big) continue;
        Color c = t.hostile ? Color{220, 40, 40, 255} : t.col;
        float cx = r.x + t.x * sx, cy = r.y + t.y * sy;
        DrawPoly({cx, cy}, 4, big ? 9 : 5, 45, c);
        DrawPolyLines({cx, cy}, 4, big ? 9 : 5, 45, BLACK);
        if (big) DrawText(t.name.c_str(), cx + 12, cy - 8, 18, c);
    }
    float px = r.x + g->pl.x * sx, py = r.y + g->pl.y * sy;
    DrawCircle(px, py, big ? 6 : 3, WHITE);
    DrawCircleLines(px, py, big ? 6 : 3, BLACK);
    DrawRectangleLinesEx(r, 2, BORDER);
}

// ---------------------------------------------------------------- HUD
static std::string tileTip(int tx, int ty) {
    if (!inb(tx, ty)) return "";
    int i = IDX(tx, ty);
    std::string s;
    Tile& t = g->tiles[i];
    Crop& c = g->crops[i];
    uint8_t gr = g->ground[i];
    if (c.type) {
        s += TextFormat("%s - %s\n", CROPS[c.type].name, c.dead ? "dead" : (c.growth >= 1 ? "RIPE" : TextFormat("%d%% grown", (int)(c.growth * 100))));
        if (!c.dead && c.pest > 0.15f) s += TextFormat("Pests: %d%%  (ponds, leaf piles, ducks help)\n", (int)(c.pest * 100));
        if (CROPS[c.type].hardy) s += "Frost-hardy\n";
    }
    if (t.obj) {
        s += OBJS[t.obj].name;
        if (t.obj == O_BUSH) s += t.var >= 4 ? " (picked)" : " (berries)";
        if (t.obj == O_SAPLING) s += TextFormat(" (grows in %d days)", std::max(0, 6 - (int)t.timer));
        if (t.cont >= 0) {
            int n = 0;
            for (auto& st : g->conts[t.cont].slots) if (st.id) n += st.n;
            s += TextFormat(" - %d items", n);
        }
        if (t.obj == O_WHEEL || t.obj == O_SOLAR || t.obj == O_BIOGAS) s += TextFormat("\nGrid: %.0fW made / %.0fW used", g->powerGen, g->powerUse);
        if (OBJS[t.obj].structure) s += "\n(shift+click to pick up)";
        s += "\n";
    }
    if (gr == G_TILLED || gr == G_GREENHOUSE) s += TextFormat("%s - moisture %d%%, fertility %d%%\n", gr == G_GREENHOUSE ? "Greenhouse soil" : "Tilled soil", (int)(g->moist[i] * 100), (int)(g->fert[i] * 100));
    if (g->indoor[i] && !t.obj) s += "Indoors\n";
    return s;
}

static void hud() {
    Player& p = g->pl;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // vitals
    Rectangle vr{12, 12, 300, 168};
    used(vr);
    DrawRectangleRec(vr, Color{20, 16, 14, 170});
    bar(22, 22, 280, 26, p.hp / 100, Color{200, 50, 50, 255}, TextFormat("Health %d", (int)p.hp));
    bar(22, 54, 280, 26, p.food / 100, Color{220, 140, 50, 255}, TextFormat("Food %d", (int)p.food));
    bar(22, 86, 280, 26, p.water / 100, Color{60, 130, 230, 255}, TextFormat("Water %d", (int)p.water));
    float bt = clampf((p.body - 33) / 7.f, 0, 1);
    Color bc = p.body < 35.5f ? Color{120, 180, 255, 255} : p.body > 38.5f ? Color{255, 110, 60, 255} : Color{120, 210, 120, 255};
    bar(22, 118, 280, 26, bt, bc, TextFormat("Body %.1f C", p.body));
    std::string status;
    int px = (int)p.x, py = (int)p.y;
    float air = tempAt(px, py);
    float insul = 0;
    for (auto& w : p.wear) if (w.id) insul += ITEMS[w.id].insul;
    float feels = air + insul - p.wet * 7;
    if (p.body < 35.5f) status += "FREEZING  ";
    else if (feels < 12) status += "Cold  ";
    if (p.body > 38.5f) status += "OVERHEATING  ";
    if (p.sick > 0) status += "Sick  ";
    if (p.wet > 0.3f) status += "Wet  ";
    if (p.food < 15) status += "Starving  ";
    if (p.water < 15) status += "Thirsty  ";
    if (g->indoor[IDX(px, py)]) status += "Indoors";
    DrawText(status.c_str(), 22, 150, 16, Color{255, 200, 170, 255});

    // clock
    Rectangle cr{(float)sw - 332, 12, 320, 132};
    used(cr);
    DrawRectangleRec(cr, Color{20, 16, 14, 170});
    int hh = (int)(g->tod * 24), mm = (int)(g->tod * 24 * 60) % 60;
    static const char* WN[] = {"Clear", "Cloudy", "Rain", "Thunderstorm", "Snow", "BLIZZARD", "Fog"};
    Color sc[4] = {Color{150, 230, 130, 255}, Color{255, 220, 90, 255}, Color{230, 140, 60, 255}, Color{170, 210, 255, 255}};
    DrawText(TextFormat("%s, Day %d", SEASON_NAMES[g->season()], g->dayOfSeason() + 1), cr.x + 12, cr.y + 10, 22, sc[g->season()]);
    DrawText(TextFormat("Year %d   %02d:%02d", g->year(), hh, mm / 10 * 10), cr.x + 12, cr.y + 38, 18, TEXT);
    DrawText(TextFormat("%s  %d C  (feels %d C)", WN[g->weather], (int)std::round(airTemp()), (int)std::round(feels)), cr.x + 12, cr.y + 62, 18, TEXT);
    std::string alerts;
    if (g->drought) alerts += "DROUGHT  ";
    if (g->heatwave) alerts += "HEATWAVE  ";
    if (g->floodActive) alerts += "FLOOD  ";
    if (!g->fireTiles.empty()) alerts += "FIRE!  ";
    for (auto& e : g->ents) if (e.alive && e.kind == E_RAIDER && e.state >= 1) { alerts += "RAID!"; break; }
    DrawText(alerts.c_str(), cr.x + 12, cr.y + 86, 18, Color{255, 110, 80, 255});
    if (g->powerGen > 0 || g->powerUse > 0)
        DrawText(TextFormat("Power %.0fW / %.0fW%s", g->powerGen, g->powerUse, g->powerCap > 0 ? TextFormat("  batt %d%%", (int)(100 * g->powerStored / g->powerCap)) : ""), cr.x + 12, cr.y + 108, 16,
                 g->powered || g->powerUse == 0 ? Color{140, 230, 255, 255} : Color{255, 120, 90, 255});

    // minimap
    minimapTimer -= GetFrameTime();
    if (minimapTimer <= 0 || minimapTex.id == 0) { minimapTimer = 1.0f; refreshMinimap(); }
    if (g->showMinimap) {
        float ms = 200;
        // window around the player
        float vx = clampf(g->pl.x - 40, 0, WW - 80), vy = clampf(g->pl.y - 40, 0, WH - 80);
        Rectangle mr{(float)sw - 12 - ms, 152, ms, ms};
        used(mr);
        DrawTexturePro(minimapTex, Rectangle{vx, vy, 80, 80}, mr, {0, 0}, 0, WHITE);
        float s = ms / 80;
        DrawCircle(mr.x + (g->pl.x - vx) * s, mr.y + (g->pl.y - vy) * s, 3, WHITE);
        for (auto& t : g->tribes) {
            float tx = (t.x - vx) * s, ty = (t.y - vy) * s;
            if (tx >= 0 && ty >= 0 && tx < ms && ty < ms) DrawPoly({mr.x + tx, mr.y + ty}, 4, 5, 45, t.hostile ? RED : t.col);
        }
        DrawRectangleLinesEx(mr, 2, BORDER);
    }

    // hotbar
    float sz = 56, gap = 6, total = HOTBAR * (sz + gap) - gap;
    float hx = sw / 2.f - total / 2, hy = sh - sz - 14;
    for (int i = 0; i < HOTBAR; i++) {
        Rectangle r{hx + i * (sz + gap), hy, sz, sz};
        slotUI(p.inv[i], r, nullptr, 0, i == p.sel);
        DrawText(TextFormat("%d", (i + 1) % 10), r.x + 4, r.y + 2, 12, DIM);
    }
    Stack& h = p.inv[p.sel];
    if (h.id) {
        const char* nm = ITEMS[h.id].name;
        DrawText(nm, sw / 2 - MeasureText(nm, 20) / 2, hy - 28, 20, TEXT);
    }

    // log
    float ly = sh - 110;
    for (auto& l : g->log) {
        float a = clampf(1 - (l.t - 14) / 4, 0, 1);
        if (a <= 0) continue;
        Color c = l.c; c.a = (unsigned char)(255 * a);
        DrawText(l.text.c_str(), 15, ly + 1, 18, Color{0, 0, 0, (unsigned char)(200 * a)});
        DrawText(l.text.c_str(), 14, ly, 18, c);
        ly -= 24;
    }
    DrawText("Tab inventory/craft   G milestones   M map   H help   Esc menu", sw - 640, sh - 26, 16, Color{230, 220, 200, 160});

    // world hover tooltip
    if (g->panel == P_NONE && !overUIPrev) {
        Vector2 wm = GetScreenToWorld2D(GetMousePosition(), g->cam);
        Ent* e = entAt(wm, 0.8f);
        std::string tip;
        if (e) {
            const SpeciesDef& s = SPECIES[e->kind];
            if (e->kind == E_WORKER) tip = e->name + " - " + JOB_NAMES[e->job] + "\nRight-click to manage";
            else if (e->kind == E_VILLAGER) tip = g->tribes[e->tribe].name + (e->state == 1 ? " trader\nRight-click to trade" : " villager");
            else if (e->kind == E_RAIDER) tip = "Raider (" + g->tribes[e->tribe].name + ")";
            else {
                tip = std::string(e->tame ? "Tame " : (e->baby ? "Young " : "Wild ")) + s.name;
                if (e->tame) {
                    tip += TextFormat("\nHunger %d%%  %s", (int)e->hunger, e->follow ? "(following)" : "(staying)");
                    if (e->kind == E_SHEEP) tip += e->produce >= 1 ? "\nReady to shear" : TextFormat("\nWool %d%%", (int)(e->produce * 100));
                    if (e->kind == E_BUFFALO) tip += e->produce >= 1 ? "\nReady to milk (right-click)" : "";
                    if (e->kind == E_HORSE) tip += "\nRight-click to ride";
                } else if (s.tameFood[0]) {
                    tip += TextFormat("\nTame with %s", ITEMS[s.tameFood[0]].name);
                    if (e->trust > 0) tip += TextFormat("  (trust %d%%)", (int)e->trust);
                } else if (e->kind == E_WOLF || e->kind == E_BEAR) tip += "\nDangerous!";
            }
        } else {
            int tx = (int)std::floor(wm.x / TS), ty = (int)std::floor(wm.y / TS);
            tip = tileTip(tx, ty);
        }
        if (!tip.empty() && tip.back() == '\n') tip.pop_back();
        if (!tip.empty()) hoverTip = tip;
    }
}

static void helpPanel() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 520, sh / 2.f - 350, 1040, 700};
    panelBox(r, "How to survive");
    const char* lines =
        "WASD move  -  Shift sprint  -  1-0 / mouse wheel select hotbar\n"
        "Left click: use held item - chop, mine, gather, till (hoe), plant seeds, attack, place buildings\n"
        "Right click: interact - open storage, eat/drink, equip clothes, tame/feed animals, trade, sleep\n"
        "Shift + left click on a building picks it back up.   F5 save   F9 load   +/- zoom\n"
        "\n"
        "COLD KILLS. Night and winter are freezing. Stay near fires, build enclosed rooms (indoors is warmer),\n"
        "and wear clothes: fur coats from hides, wool clothing from sheared sheep on a loom.\n"
        "\n"
        "FOOD ROTS. Raw food molds in your pack. Granaries and root cellars slow it; a powered cold store\n"
        "nearly stops it. Chests left outdoors in the rain get damp and mold fast. Mold spreads.\n"
        "\n"
        "FARMING. Till, plant, water. Frost in late autumn kills crops still in the ground, so harvest early.\n"
        "Cabbage is frost-hardy. Harvests drain the soil. Manure, compost, or beans (rotation) restore it.\n"
        "No pesticides: ponds bring frogs, leaf piles bring spiders, and ducks and fowl eat insects.\n"
        "\n"
        "ANIMALS. Hold food an animal likes to calm it, then right-click to tame. Wolves hunt livestock at\n"
        "night. Guard dogs, fences and fire keep them off. Livestock need hay in troughs during winter.\n"
        "\n"
        "TRIBES. Villages with banners trade (look for the $). Raider camps with skulls attack once your\n"
        "farm grows. Build roads to a trading village plus a trading post for better prices.\n"
        "Recruit workers there. They need an indoor bed, food in storage, and water (a well).\n"
        "\n"
        "DISASTERS. Lightning starts wildfires in dry summers. Trees near buildings let fire spread, so keep\n"
        "a firebreak. Spring floods hit riverbanks unless you dam the river, but a dam's reservoir drowns\n"
        "the low land upstream for good. Choose where to dam. Blizzards, droughts, heatwaves.\n"
        "\n"
        "GO MODERN. Water wheels next to a dam and solar panels make power. Batteries store it. Power runs\n"
        "lamps, heaters, sprinklers and cold stores. Biogas turns manure into power and compost.";
    DrawText(lines, r.x + 20, r.y + 56, 17, TEXT);
}

static void pausePanel() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle r{sw / 2.f - 180, sh / 2.f - 170, 360, 340};
    panelBox(r, "Paused");
    float y = r.y + 60;
    if (button(Rectangle{r.x + 30, y, 300, 44}, "Resume")) g->panel = P_NONE; y += 54;
    if (button(Rectangle{r.x + 30, y, 300, 44}, "Save game")) { if (saveGame("hearthland.sav")) logMsg("Game saved.", GREEN); g->panel = P_NONE; } y += 54;
    if (button(Rectangle{r.x + 30, y, 300, 44}, "Load game")) { if (loadGame("hearthland.sav")) { logMsg("Game loaded.", GREEN); g->panel = P_NONE; } } y += 54;
    if (button(Rectangle{r.x + 30, y, 300, 44}, "Help")) g->panel = P_HELP; y += 54;
    if (button(Rectangle{r.x + 30, y, 300, 44}, "Save & quit to title")) { saveGame("hearthland.sav"); g->panel = P_TITLE; }
}

static void deadPanel() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{40, 0, 0, 150});
    Rectangle r{sw / 2.f - 260, sh / 2.f - 140, 520, 280};
    panelBox(r, "You died");
    if (!g->log.empty()) DrawText(g->log.front().text.c_str(), r.x + 20, r.y + 60, 18, Color{255, 150, 130, 255});
    DrawText(TextFormat("You lasted %d days.", g->day + 1), r.x + 20, r.y + 90, 18, TEXT);
    if (button(Rectangle{r.x + 30, r.y + 140, 460, 44}, "Load last save", FileExists("hearthland.sav"))) { if (loadGame("hearthland.sav")) g->panel = P_NONE; }
    if (button(Rectangle{r.x + 30, r.y + 194, 460, 44}, "New world")) { newGame((uint32_t)GetRandomValue(1, 1 << 30)); g->panel = P_NONE; }
}

static void titleScreen() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Color{10, 14, 20, 170});
    const char* t = "HEARTHLAND";
    int fs = 96;
    DrawText(t, sw / 2 - MeasureText(t, fs) / 2 + 5, sh / 4 + 5, fs, Color{0, 0, 0, 180});
    DrawText(t, sw / 2 - MeasureText(t, fs) / 2, sh / 4, fs, Color{255, 200, 120, 255});
    const char* sub = "From nothing to a self-sustaining modern farm. Survive the seasons.";
    DrawText(sub, sw / 2 - MeasureText(sub, 22) / 2, sh / 4 + 110, 22, TEXT);
    float y = sh / 2.f + 20;
    Rectangle b{sw / 2.f - 160, y, 320, 50};
    if (button(b, "New world", true, 22)) { newGame((uint32_t)GetRandomValue(1, 1 << 30)); g->panel = P_HELP; }
    b.y += 62;
    if (button(b, "Continue", FileExists("hearthland.sav"), 22)) { if (loadGame("hearthland.sav")) g->panel = P_NONE; }
    b.y += 62;
    if (button(b, "Quit", true, 22)) { g->panel = P_TITLE; CloseWindow(); exit(0); }
}

static void drawTooltip() {
    if (hoverTip.empty()) return;
    Vector2 m = GetMousePosition();
    int lines = 1, maxw = 0;
    size_t start = 0;
    for (size_t i = 0; i <= hoverTip.size(); i++)
        if (i == hoverTip.size() || hoverTip[i] == '\n') {
            maxw = std::max(maxw, MeasureText(hoverTip.substr(start, i - start).c_str(), 16));
            if (i < hoverTip.size()) lines++;
            start = i + 1;
        }
    float w = maxw + 20, h = lines * 20 + 12;
    float x = std::min(m.x + 18, GetScreenWidth() - w - 4), y = std::min(m.y + 18, GetScreenHeight() - h - 4);
    DrawRectangle(x, y, w, h, Color{24, 20, 18, 235});
    DrawRectangleLines(x, y, w, h, BORDER);
    DrawText(hoverTip.c_str(), x + 10, y + 8, 16, TEXT);
}

void drawUI() {
    overUIPrev = overUI;
    overUI = false;
    hoverTip.clear();
    if (g->panel == P_TITLE) { titleScreen(); return; }
    hud();
    switch (g->panel) {
        case P_INV: inventoryPanel(); break;
        case P_CONTAINER: containerPanel(); break;
        case P_TRADE: tradePanel(); break;
        case P_WORKER: workerPanel(); break;
        case P_GOALS: goalsPanel(); break;
        case P_MAP: {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float s = std::min(sw, sh) - 120.f;
            Rectangle r{sw / 2.f - s / 2, sh / 2.f - s / 2, s, s};
            panelBox(Rectangle{r.x - 10, r.y - 50, r.width + 20, r.height + 60}, "Map");
            drawMapAt(r, true);
            break;
        }
        case P_HELP: helpPanel(); break;
        case P_PAUSE: pausePanel(); break;
        case P_DEAD: deadPanel(); break;
        default: break;
    }
    // close buttons for regular panels
    if (g->panel == P_INV || g->panel == P_CONTAINER || g->panel == P_TRADE || g->panel == P_WORKER || g->panel == P_GOALS || g->panel == P_HELP || g->panel == P_MAP) {
        DrawText("Esc to close", GetScreenWidth() / 2 - 50, 16, 18, DIM);
    }
    if (g->sleepFade > 0) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 10, (unsigned char)(g->sleepFade * 200)});
    if (g->cursor.id) {
        Vector2 m = GetMousePosition();
        drawItemIcon(g->cursor.id, m.x - 20, m.y - 20, 40);
        if (g->cursor.n > 1) DrawText(TextFormat("%d", g->cursor.n), m.x + 10, m.y + 8, 16, WHITE);
    }
    drawTooltip();
}

bool uiHandleInput() {
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (g->panel == P_NONE) g->panel = P_PAUSE;
        else if (g->panel != P_TITLE && g->panel != P_DEAD) closePanel();
    }
    if (g->panel == P_TITLE || g->panel == P_DEAD || g->panel == P_PAUSE) return true;
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I)) { if (g->panel == P_INV) closePanel(); else if (g->panel == P_NONE) openPanel(P_INV); }
    if (IsKeyPressed(KEY_G)) { if (g->panel == P_GOALS) closePanel(); else if (g->panel == P_NONE) openPanel(P_GOALS); }
    if (IsKeyPressed(KEY_M)) { if (g->panel == P_MAP) closePanel(); else if (g->panel == P_NONE) openPanel(P_MAP); }
    if (IsKeyPressed(KEY_H)) { if (g->panel == P_HELP) closePanel(); else if (g->panel == P_NONE) openPanel(P_HELP); }
    if (IsKeyPressed(KEY_N)) g->showMinimap = !g->showMinimap;
    return g->panel != P_NONE || overUIPrev;
}
