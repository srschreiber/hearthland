// Player, animals and people.
#include "game.h"
#include <algorithm>
#include <unordered_map>

static const char* NAMES[] = {"Ada", "Bram", "Cora", "Dov", "Esme", "Finn", "Gwen", "Hale", "Iris", "Joss", "Kira", "Lio",
                              "Mara", "Nils", "Oona", "Pell", "Quin", "Rhea", "Sol", "Tam", "Ugo", "Vera", "Wren", "Yara"};

static bool byPlayer = false;

// Tiles a worker recently failed to reach; planners skip them for a while.
static std::unordered_map<int, float> unreachableUntil;
static bool unreachable(int x, int y) {
    if (!inb(x, y)) return false;
    auto it = unreachableUntil.find(IDX(x, y));
    return it != unreachableUntil.end() && it->second > g->totalTime && it->second - g->totalTime < 1000;
}


static bool isTree(uint8_t o) { return isTreeObj(o); }
static Stack& held() { return g->pl.inv[g->pl.sel]; }
static int heldTool() { return held().id ? ITEMS[held().id].tool : T_NONE; }

// ---------------------------------------------------------------- movement
static bool blockedAt(float x, float y, float r, int who) {
    int pts[4][2] = {{(int)std::floor(x - r), (int)std::floor(y - r)}, {(int)std::floor(x + r), (int)std::floor(y - r)},
                     {(int)std::floor(x - r), (int)std::floor(y + r)}, {(int)std::floor(x + r), (int)std::floor(y + r)}};
    for (auto& p : pts) if (solidFor(p[0], p[1], who)) return true;
    return false;
}

// returns false if movement was blocked on both axes
static bool moveBody(float& x, float& y, float dx, float dy, int who, float r = 0.28f) {
    bool moved = false;
    bool stuck = blockedAt(x, y, r * 0.5f, who);   // already inside something solid: let it walk out
    if (dx != 0) { if (stuck || !blockedAt(x + dx, y, r, who)) { x += dx; moved = true; } }
    if (dy != 0) { if (stuck || !blockedAt(x, y + dy, r, who)) { y += dy; moved = true; } }
    // unstick if somehow inside a solid tile
    if (blockedAt(x, y, r * 0.5f, who) && !moved) {
        x += rndr(-0.05f, 0.05f); y += rndr(-0.05f, 0.05f);
    }
    return moved;
}

static int whoFor(const Ent& e) {
    if (e.kind == E_WORKER || e.kind == E_VILLAGER || (e.kind == E_DOG && e.tame)) return 2;
    if (e.kind == E_RAIDER) return 3;
    return 1;
}

static float speedMul(float x, float y) {
    int tx = (int)x, ty = (int)y;
    if (!inb(tx, ty)) return 1;
    uint8_t gr = g->ground[IDX(tx, ty)];
    if (gr == G_WATER) return 0.5f;
    if (gr == G_ROAD || gr == G_FLOOR) return 1.25f;
    if (g->snowCover > 0.5f) return 0.85f;
    return 1;
}

static bool stepToward(Ent& e, float tx, float ty, float spd, float dt) {
    float dx = tx - e.x, dy = ty - e.y, L = std::sqrt(dx * dx + dy * dy);
    if (L < 0.05f) return true;
    float s = spd * speedMul(e.x, e.y) * dt;
    if (s > L) s = L;
    e.vx = dx / L; e.vy = dy / L;
    if (std::fabs(dx) > 0.05f) e.face = dx > 0 ? 1.f : -1.f;
    e.anim += dt * spd * 2.5f;
    return moveBody(e.x, e.y, dx / L * s, dy / L * s, whoFor(e));
}

// Follow a path to tile (tx,ty). Returns 1 when adjacent/arrived, 0 moving, -1 unreachable.
static int goTo(Ent& e, int tx, int ty, float spd, float dt, float nearR = 1.3f) {
    float cx = tx + 0.5f, cy = ty + 0.5f;
    if (dist(e.x, e.y, cx, cy) <= nearR) { e.path.clear(); return 1; }
    bool needPath = e.path.empty() || e.pathI >= (int)e.path.size() || e.tx != tx || e.ty != ty;
    if (needPath) {
        if (e.repath > 0 && e.tx == tx && e.ty == ty) { e.repath -= dt; stepToward(e, cx, cy, spd, dt); return 0; }
        e.tx = tx; e.ty = ty; e.pathI = 0;
        if (!findPath((int)e.x, (int)e.y, tx, ty, whoFor(e), e.path, 6000)) { e.repath = 2.f; return -1; }
        if (e.path.empty()) return 1;
    }
    PathPt p = e.path[e.pathI];
    float px = p.x + 0.5f, py = p.y + 0.5f;
    stepToward(e, px, py, spd, dt);
    if (dist(e.x, e.y, px, py) < 0.25f) e.pathI++;
    if (e.pathI >= (int)e.path.size()) { e.path.clear(); return dist(e.x, e.y, cx, cy) <= nearR + 0.6f ? 1 : 0; }
    return 0;
}

// ---------------------------------------------------------------- damage
void hurtPlayer(float dmg, const char* why) {
    Player& p = g->pl;
    if (p.sleeping) { p.sleeping = false; logMsg("You are woken by an attack!", RED); }
    p.hp -= dmg;
    p.hurtFlash = 0.3f;
    g->shake = std::max(g->shake, 0.2f);
    burst(p.x, p.y - 0.3f, Color{200, 30, 30, 255}, 5);
    if (p.hp <= 0 && g->panel != P_DEAD) {
        p.hp = 0;
        g->panel = P_DEAD;
        logMsg(std::string("You died: ") + why, RED);
    }
}

static void dropLoot(Ent& e) {
    const SpeciesDef& s = SPECIES[e.kind];
    if (!byPlayer) return;
    int meat = s.meat * (e.baby ? 1 : 1), hide = e.baby ? 0 : s.hide;
    if (meat) addToPlayer(I_RAWMEAT, meat);
    if (hide) addToPlayer(I_HIDE, hide);
    if (e.kind == E_SHEEP && e.produce > 0.5f) addToPlayer(I_WOOL, 2);
    if (e.kind == E_RAIDER) {
        if (rnd() < 0.5f) addToPlayer(I_SPEAR, 1);
        if (e.carry.id) addToPlayer(e.carry.id, e.carry.n, e.carry.fresh);
        addToPlayer(I_HIDE, 1);
    }
    std::string msg = "+";
    if (meat) msg += std::to_string(meat) + " meat ";
    if (hide) msg += std::to_string(hide) + " hide";
    if (meat || hide) floatText(e.x, e.y - 0.5f, msg, Color{255, 220, 180, 255});
}

void damageEnt(Ent& e, float dmg, float fx, float fy) {
    if (!e.alive) return;
    e.hp -= dmg;
    e.hurtFlash = 0.25f;
    e.sleeping = false;
    float dx = e.x - fx, dy = e.y - fy, L = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
    moveBody(e.x, e.y, dx / L * 0.3f, dy / L * 0.3f, whoFor(e));
    burst(e.x, e.y - 0.2f, Color{190, 30, 30, 255}, 4);
    if (e.hp <= 0) {
        e.alive = false;
        dropLoot(e);
        if (e.tame) {
            g->stats.livestockLost++;
            logMsg(std::string("Your ") + SPECIES[e.kind].name + " was killed!", Color{255, 120, 100, 255});
        }
        if (e.kind == E_WORKER) logMsg(e.name + " was killed!", RED);
        if (e.kind == E_VILLAGER && byPlayer) {
            if (e.tribe >= 0) { g->tribes[e.tribe].hostile = true; logMsg(g->tribes[e.tribe].name + " will never trade with you again.", RED); }
        }
        if (g->pl.riding == e.id) g->pl.riding = -1;
        return;
    }
    // reactions
    if (e.kind == E_VILLAGER && byPlayer && e.tribe >= 0) logMsg("Hey! Watch it!", ORANGE);
}

Ent* entAt(Vector2 w, float r) {
    float mx = w.x / TS, my = w.y / TS;
    Ent* best = nullptr; float bd = r;
    for (auto& e : g->ents) {
        if (!e.alive) continue;
        float d = dist(mx, my, e.x, e.y - 0.3f * SPECIES[e.kind].size);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

int freeBed() {
    for (int i = 0; i < WW * WH; i++) {
        if (g->tiles[i].obj != O_BED || !g->indoor[i]) continue;
        bool taken = false;
        for (auto& e : g->ents) if (e.alive && e.kind == E_WORKER && e.bed == i) { taken = true; break; }
        if (!taken) return i;
    }
    return -1;
}

// ---------------------------------------------------------------- spawning
static bool freeLand(int x, int y) {
    return inb(x, y) && !isWater(x, y) && !solidFor(x, y, 1) && g->ground[IDX(x, y)] != G_VILLAGE;
}

static void spawnHerd(int kind, int n, bool farFromPlayer) {
    for (int attempt = 0; attempt < 120; attempt++) {
        int x = rndi(6, WW - 7), y = rndi(6, WH - 7);
        if (!freeLand(x, y)) continue;
        uint8_t gr = g->ground[IDX(x, y)];
        float pd = dist(x, y, g->pl.x, g->pl.y);
        if (farFromPlayer && pd < 30) continue;
        if (pd < 10) continue;
        bool ok = false;
        switch (kind) {
            case E_SHEEP: case E_BUFFALO: case E_HORSE: ok = gr == G_GRASS || gr == G_DIRT; break;
            case E_FOWL: case E_BEAR: ok = gr == G_FOREST; break;
            case E_DUCK: ok = isWater(x + 2, y) || isWater(x - 2, y) || isWater(x, y + 2) || isWater(x, y - 2); break;
            case E_DOG: ok = gr == G_GRASS; break;
            default: ok = true;
        }
        if (!ok) continue;
        for (auto& t : g->tribes) if (dist(x, y, t.x, t.y) < 14) ok = false;
        if (!ok) continue;
        for (int k = 0; k < n; k++) {
            float ex = x + rndr(-1.5f, 1.5f), ey = y + rndr(-1.5f, 1.5f);
            if (!freeLand((int)ex, (int)ey)) { ex = x + 0.5f; ey = y + 0.5f; }
            Ent& e = spawnEnt(kind, ex, ey);
            e.homeX = x + 0.5f; e.homeY = y + 0.5f;
            if (kind == E_BEAR && g->season() == 3) e.sleeping = true;
        }
        return;
    }
}

void spawnWildlife(bool initial) {
    struct Want { int kind, herds, size; };
    Want wants[] = {{E_SHEEP, 5, 4}, {E_BUFFALO, 3, 3}, {E_HORSE, 3, 4}, {E_DUCK, 6, 3}, {E_FOWL, 6, 3}, {E_DOG, 4, 1}, {E_BEAR, 4, 1}};
    for (auto& w : wants) {
        int wild = 0;
        for (auto& e : g->ents) if (e.alive && !e.tame && e.kind == w.kind) wild++;
        int target = w.herds * w.size;
        if (initial) { for (int h = 0; h < w.herds; h++) spawnHerd(w.kind, w.size, false); continue; }
        if (wild < target * 0.6f && rnd() < 0.5f) spawnHerd(w.kind, w.size, true);
    }
    // critters near ponds and leaf piles
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (o != O_POND && o != O_LEAFPILE) continue;
            int kind = o == O_POND ? E_FROG : E_SPIDER;
            int have = 0;
            for (auto& e : g->ents) if (e.alive && e.kind == kind && dist(e.x, e.y, x, y) < 6) have++;
            for (int k = have; k < (o == O_POND ? 3 : 2); k++) {
                Ent& e = spawnEnt(kind, x + rndr(-1, 2), y + rndr(-1, 2));
                e.homeX = x + 0.5f; e.homeY = y + 0.5f;
            }
        }
}

void spawnRaid(int ti) {
    Tribe& t = g->tribes[ti];
    int n = std::clamp(2 + g->day / 10 + countStructures() / 40, 2, 7);
    for (int k = 0; k < n; k++) {
        Ent& e = spawnEnt(E_RAIDER, t.x + rndr(-2, 2), t.y + rndr(-2, 2));
        e.tribe = ti; e.state = 1; e.homeX = t.x + 0.5f; e.homeY = t.y + 0.5f;
        e.torch = k % 2 == 0;
        e.maxhp = e.hp = SPECIES[E_RAIDER].hp + g->day * 0.8f;
    }
    logMsg("RAID! " + std::to_string(n) + " " + t.name + " are coming for your farm!", Color{255, 70, 60, 255});
}

void recruitWorker(int ti) {
    Tribe& t = g->tribes[ti];
    Ent& e = spawnEnt(E_WORKER, t.x + 0.5f, t.y + 1.5f);
    e.name = NAMES[rndi(0, (int)(sizeof(NAMES) / sizeof(NAMES[0])) - 1)];
    e.job = J_IDLE; e.tribe = -1;
    e.hunger = 90; e.thirst = 90; e.mood = 80;
    e.bed = freeBed();
    logMsg(e.name + " has joined your farm and is walking over. Assign a job with right-click.", Color{150, 230, 150, 255});
}

// ---------------------------------------------------------------- player
static void eatItem(int slot) {
    Player& p = g->pl;
    Stack& s = p.inv[slot];
    if (!s.id) return;
    const ItemDef& d = ITEMS[s.id];
    if (s.id == I_MOLDY) { floatText(p.x, p.y - 1, "It's moldy! Compost it.", ORANGE); return; }
    if (s.id == I_WATERBUCKET) {
        p.water = std::min(100.f, p.water + d.drink);
        s.id = I_BUCKET;
        floatText(p.x, p.y - 1, "Gulp", SKYBLUE);
        return;
    }
    if (d.food <= 0 && d.drink <= 0) return;
    if (p.food > 97 && d.drink <= 0) { floatText(p.x, p.y - 1, "Full", LIGHTGRAY); return; }
    float q = 0.5f + 0.5f * clampf(s.fresh, 0, 1);
    p.food = std::min(100.f, p.food + d.food * q);
    p.water = clampf(p.water + d.drink, 0, 100);
    g->stats.foodEaten += d.food;
    if ((s.id == I_RAWMEAT && rnd() < 0.4f) || (s.fresh < 0.2f && rnd() < 0.35f)) {
        p.sick = 90;
        logMsg("You feel sick... (bad food)", Color{170, 220, 90, 255});
    }
    floatText(p.x, p.y - 1, std::string("Ate ") + d.name, Color{255, 230, 180, 255});
    if (--s.n <= 0) s = Stack{};
}

static void equipItem(int slot) {
    Stack& s = g->pl.inv[slot];
    int w = ITEMS[s.id].wear;
    if (!w) return;
    Stack old = g->pl.wear[w];
    g->pl.wear[w] = s;
    s = old;
    logMsg(std::string("Wearing ") + ITEMS[g->pl.wear[w].id].name, Color{220, 220, 255, 255});
}

static float playerInsulation() {
    float i = 0;
    for (auto& w : g->pl.wear) if (w.id) i += ITEMS[w.id].insul;
    return i;
}

void updatePlayer(float dt) {
    Player& p = g->pl;
    if (g->panel == P_DEAD) return;
    p.atkCD -= dt; p.useAnim -= dt; p.hurtFlash -= dt;
    Ent* horse = p.riding >= 0 ? entById(p.riding) : nullptr;
    if (p.riding >= 0 && !horse) p.riding = -1;

    // movement
    if (!p.sleeping) {
        float mx = 0, my = 0;
        bool typing = g->panel == P_TRADE || g->panel == P_CONTAINER || g->panel == P_WORKER || g->panel == P_PAUSE;
        if (!typing) {
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) my -= 1;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) my += 1;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) mx -= 1;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) mx += 1;
        }
        if (mx || my) {
            float L = std::sqrt(mx * mx + my * my);
            mx /= L; my /= L;
            bool sprint = IsKeyDown(KEY_LEFT_SHIFT);
            float spd = (horse ? 8.5f : 4.6f) * (sprint && !horse ? 1.45f : 1.f) * speedMul(p.x, p.y);
            if (p.food < 10) spd *= 0.75f;
            moveBody(p.x, p.y, mx * spd * dt, my * spd * dt, 0, 0.3f);
            if (mx) p.face = mx > 0 ? 1.f : -1.f;
            p.anim += dt * spd * 2.2f;
            if (sprint) p.food -= dt * 0.08f;
            if (horse) { horse->x = p.x; horse->y = p.y; horse->face = p.face; horse->anim = p.anim; }
        }
    } else {
        // wake at dawn
        if (g->tod > 0.27f && g->tod < 0.6f) {
            p.sleeping = false;
            logMsg("Good morning. Day " + std::to_string(g->day + 1) + ".", Color{255, 240, 200, 255});
        }
    }

    if (p.fishing) {
        if (heldTool() != T_ROD || IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D)) p.fishing = false;
        else if (p.biteTimer > 0) {
            p.biteTimer -= dt;
            if (p.biteTimer <= 0) { p.fishing = false; floatText(p.bobX + 0.5f, p.bobY, "It got away...", LIGHTGRAY); }
        } else if ((p.fishTimer -= dt) <= 0) {
            p.biteTimer = 1.1f;
            floatText(p.bobX + 0.5f, p.bobY - 0.5f, "!", YELLOW);
        }
    }
    int px = (int)p.x, py = (int)p.y;
    for (int y = py - 12; y <= py + 12; y++)
        for (int x = px - 16; x <= px + 16; x++)
            if (inb(x, y)) g->seen[IDX(x, y)] = 1;

    // needs
    float air = tempAt(px, py);
    bool outdoors = !g->indoor[IDX(px, py)];
    bool wetting = outdoors && (g->weather == WX_RAIN || g->weather == WX_STORM);
    p.wet = clampf(p.wet + (wetting ? dt * 0.05f : -dt * 0.03f * (air > 15 ? 2 : 1)), 0, 1);
    float feels = air + playerInsulation() - p.wet * 7 - (g->weather == WX_BLIZZARD && outdoors ? 6 : 0);
    float sleepMul = p.sleeping ? 0.5f : 1.f;
    float foodRate = 100.f / (DAY_SECONDS * 1.6f), waterRate = 100.f / (DAY_SECONDS * 1.1f);
    if (feels < 5) foodRate *= 1.35f;
    if (feels > 30) waterRate *= 2.f;
    if (p.sick > 0) { p.sick -= dt; foodRate *= 1.5f; p.hp -= dt * 0.12f; }
    p.food = std::max(0.f, p.food - foodRate * dt * sleepMul);
    p.water = std::max(0.f, p.water - waterRate * dt * sleepMul);
    if (feels < 14) p.body -= (14 - feels) * 0.0032f * dt;
    else if (feels > 33) p.body += (feels - 33) * 0.002f * dt;
    else p.body += clampf(37 - p.body, -0.05f, 0.06f) * dt;
    p.body = clampf(p.body, 30, 41);
    if (p.body < 34.5f) hurtPlayer((34.5f - p.body) * 1.2f * dt, "froze to death");
    if (p.body > 39.5f) hurtPlayer((p.body - 39.5f) * 1.0f * dt, "heatstroke");
    if (p.food <= 0) hurtPlayer(0.45f * dt, "starved");
    if (p.water <= 0) hurtPlayer(0.8f * dt, "died of thirst");
    if (g->fire[IDX(px, py)] > 0.3f) hurtPlayer(8 * dt, "burned");
    if (p.food > 45 && p.water > 45 && p.body > 35.5f && p.sick <= 0) p.hp = std::min(100.f, p.hp + dt * (p.sleeping ? 0.6f : 0.15f));
}

static bool inReach(int tx, int ty, float r) { return dist(g->pl.x, g->pl.y, tx + 0.5f, ty + 0.5f) <= r; }

static bool canPlace(int item, int tx, int ty, std::string& why) {
    const ItemDef& d = ITEMS[item];
    if (!inb(tx, ty)) return false;
    int i = IDX(tx, ty);
    uint8_t gr = g->ground[i];
    Tile& t = g->tiles[i];
    bool water = gr == G_WATER || gr == G_DEEP;
    if (g->ground[i] == G_VILLAGE) { why = "That's village land"; return false; }
    if (d.placeGround >= 0) {
        if (t.obj && OBJS[t.obj].solid && d.placeGround != G_GREENHOUSE) { why = "Something is in the way"; return false; }
        if (water) { why = "Can't build on water"; return false; }
        if (d.placeGround == G_GREENHOUSE && gr != G_TILLED) { why = "Place on tilled soil"; return false; }
        if (gr == d.placeGround) { why = "Already there"; return false; }
        return true;
    }
    int o = d.placeObj;
    if (t.obj && t.obj != O_TALLGRASS && t.obj != O_BRANCH && t.obj != O_ASH) { why = "Something is in the way"; return false; }
    if (g->crops[i].type) { why = "There's a crop there"; return false; }
    if (o == O_DAM) {
        if (gr != G_WATER && gr != G_DEEP) { why = "Dams go on river water"; return false; }
        return true;
    }
    if (water) { why = "Can't build on water"; return false; }
    if (o == O_SPRINKLER || o == O_DRIP || o == O_WATERFALL) {
        bool src = false;
        for (int y = ty - 8; y <= ty + 8 && !src; y++)
            for (int x = tx - 8; x <= tx + 8; x++)
                if (inb(x, y) && (g->tiles[IDX(x, y)].obj == O_WELL || g->tiles[IDX(x, y)].obj == O_PUMP) && dist(x, y, tx, ty) <= 8) { src = true; break; }
        if (!src) { why = "Needs a well or pump within 8 tiles"; return false; }
    }
    if (o == O_PUMP) {
        bool wet = false;
        for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) if (isWater(tx + j, ty + k) || (inb(tx + j, ty + k) && g->tiles[IDX(tx + j, ty + k)].obj == O_POND)) wet = true;
        if (!wet) { why = "Pumps go beside water or a pond"; return false; }
    }
    if (o == O_WHEEL) {
        bool riv = false;
        for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) if (isWater(tx + j, ty + k)) riv = true;
        if (!riv) { why = "Water wheels go beside the river"; return false; }
    }
    if (OBJS[o].solid) {
        if (dist(g->pl.x, g->pl.y, tx + 0.5f, ty + 0.5f) < 0.8f) { why = "You're standing there"; return false; }
        for (auto& e : g->ents) if (e.alive && (int)e.x == tx && (int)e.y == ty) { why = "Something is standing there"; return false; }
    }
    return true;
}

static void place(int tx, int ty) {
    Stack& h = held();
    std::string why;
    if (!inReach(tx, ty, 4.5f)) { floatText(tx + 0.5f, ty, "Too far", LIGHTGRAY); return; }
    if (!canPlace(h.id, tx, ty, why)) { floatText(tx + 0.5f, ty, why, LIGHTGRAY); return; }
    const ItemDef& d = ITEMS[h.id];
    int i = IDX(tx, ty);
    if (d.placeGround >= 0) {
        g->ground[i] = (uint8_t)d.placeGround;
        if (d.placeGround == G_ROAD) checkRoads();
    } else {
        if (g->tiles[i].obj) removeObject(tx, ty, false);
        placeObject(tx, ty, d.placeObj);
        if (d.placeObj == O_KENNEL)
            for (auto& e : g->ents) if (e.alive && e.tame && e.kind == E_DOG && dist(e.x, e.y, tx, ty) < 14) { e.homeX = tx + 0.5f; e.homeY = ty + 1.5f; e.follow = false; }
        if (d.placeObj == O_COOP)
            for (auto& e : g->ents) if (e.alive && e.tame && (e.kind == E_DUCK || e.kind == E_FOWL) && dist(e.x, e.y, tx, ty) < 14) { e.homeX = tx + 0.5f; e.homeY = ty + 1.5f; }
        if (d.placeObj == O_TRADEPOST) checkRoads();
        if (isFruitPlant((uint8_t)d.placeObj)) g->fruitState[i] = 2;   // fruits after one cycle
        if (d.placeObj == O_DAM) fillReservoir(tx, ty);
        if (d.placeObj == O_SPRINKLER || d.placeObj == O_DRIP || d.placeObj == O_WATERFALL) floatText(tx + 0.5f, ty - 1, "Runs constantly - fit an irrigation timer to schedule it", SKYBLUE);
        if (d.placeObj == O_MINE && g->ground[i] != G_STONE) floatText(tx + 0.5f, ty - 1, "Poor ground - mines in rocky hills yield far more", ORANGE);
        if (isPowerObj((uint8_t)d.placeObj) && d.placeObj != O_BATTERY) {
            bool bank = false;
            for (int i = 0; i < WW * WH && !bank; i++)
                if (g->tiles[i].obj == O_BATTERY && dist(tx, ty, i % WW, i / WW) <= 8.f) bank = true;
            if (!bank) floatText(tx + 0.5f, ty - 1, "Not wired: needs a battery bank within 8 tiles", ORANGE);
        }
    }
    burst(tx + 0.5f, ty + 0.5f, Color{200, 180, 140, 255}, 6);
    if (--h.n <= 0) h = Stack{};
}

static void gatherObject(int tx, int ty) {
    int i = IDX(tx, ty);
    Tile& t = g->tiles[i];
    uint8_t o = t.obj;
    int tool = heldTool();
    Player& p = g->pl;
    p.useAnim = 0.2f;
    int power = 1;
    if (held().id == I_IRONAXE || held().id == I_IRONPICK) power = 2;
    auto hit = [&](int dmg) { dmg *= (dmg > 1 ? power : 1); t.hp -= dmg; burst(tx + 0.5f, ty + 0.5f, Color{180, 160, 120, 255}, 3, 0.6f); return t.hp <= 0; };
    switch (o) {
        case O_GRAPEVINE: case O_BLACKBERRY: case O_RASPBERRY:
            if (!harvestFruit(tx, ty, false, 0, 0)) floatText(tx + 0.5f, ty, g->fruitState[i] == 2 ? "Already picked this year" : "Not ripe yet", LIGHTGRAY);
            break;
        case O_IRONORE: case O_QUARTZ:
            if (tool != T_PICK) { floatText(tx + 0.5f, ty, "Need a pickaxe", LIGHTGRAY); return; }
            if (hit(3)) {
                addToPlayer(o == O_IRONORE ? I_IRONORE : I_QUARTZ, rndi(2, 3)); addToPlayer(I_STONE, 1);
                floatText(tx + 0.5f, ty, o == O_IRONORE ? "+Iron ore" : "+Quartz", WHITE);
                removeObject(tx, ty, false);
            }
            break;
        case O_WILDPOTATO: { int n = rndi(2, 4); addToPlayer(I_POTATO, n); floatText(tx + 0.5f, ty, "+" + std::to_string(n) + " Potatoes (plant one to grow more)", Color{230, 200, 150, 255}); removeObject(tx, ty, false); break; }
        case O_APPLE: case O_PEAR: case O_APRICOT:
            if (tool != T_AXE) {
                if (!harvestFruit(tx, ty, false, 0, 0)) floatText(tx + 0.5f, ty - 1, g->fruitState[i] == 2 ? "Already picked this year" : "No ripe fruit", LIGHTGRAY);
                p.atkCD = 0.4f;
                return;
            }
            [[fallthrough]];
        case O_OAK: case O_PINE: case O_BIRCH: case O_MAPLE: case O_MTNASH: case O_ASPEN: case O_SERVICEBERRY:
            if (tool != T_AXE) {
                if (o == O_SERVICEBERRY && g->season() == 1 && t.timer <= 0) {
                    addToPlayer(I_BERRIES, 3); t.timer = 1;   // picked for this season
                    floatText(tx + 0.5f, ty - 1, "+3 Serviceberries", Color{200, 120, 220, 255});
                }
                else if (rnd() < 0.35f) { addToPlayer(I_STICK, 1); floatText(tx + 0.5f, ty - 1, "+1 Stick", WHITE); }
                else if (rnd() < 0.4f) { addToPlayer(I_LEAVES, 1); floatText(tx + 0.5f, ty - 1, "+1 Leaves", WHITE); }
                else floatText(tx + 0.5f, ty - 1, "Need an axe to fell", LIGHTGRAY);
                p.atkCD = 0.5f;
                return;
            }
            g->shake = 0.05f;
            if (hit(2)) {
                int w = rndi(4, 6);
                addToPlayer(I_WOOD, w); addToPlayer(I_LEAVES, 2); addToPlayer(I_STICK, 1);
                if (rnd() < 0.6f) addToPlayer(saplingFor(o), 1);
                floatText(tx + 0.5f, ty - 1, "+" + std::to_string(w) + " Wood", WHITE);
                removeObject(tx, ty, false);
                placeObject(tx, ty, O_STUMP);
            }
            break;
        case O_STUMP: if (hit(tool == T_AXE ? 3 : 1)) { addToPlayer(I_WOOD, 1); removeObject(tx, ty, false); } break;
        case O_ROCK:
            if (hit(tool == T_PICK ? 3 : 1)) {
                addToPlayer(I_STONE, rndi(2, 3));
                if (rnd() < 0.3f) addToPlayer(I_FLINT, 1);
                floatText(tx + 0.5f, ty, "+Stone", WHITE);
                removeObject(tx, ty, false);
            }
            break;
        case O_ORE:
            if (tool != T_PICK) { floatText(tx + 0.5f, ty, "Need a pickaxe", LIGHTGRAY); return; }
            if (hit(3)) { addToPlayer(I_ORE, rndi(2, 3)); addToPlayer(I_STONE, 1); floatText(tx + 0.5f, ty, "+Copper ore", WHITE); removeObject(tx, ty, false); }
            break;
        case O_BUSH:
            if (t.var < 4) {
                int n = g->season() == 3 ? 0 : rndi(2, 4);
                if (n) { addToPlayer(I_BERRIES, n); floatText(tx + 0.5f, ty, "+" + std::to_string(n) + " Berries", Color{255, 120, 160, 255}); }
                t.var += 4; t.timer = 0;
            } else if (hit(1)) { addToPlayer(I_STICK, 2); removeObject(tx, ty, false); }
            break;
        case O_TALLGRASS:
            if (tool == T_SICKLE) { addToPlayer(I_HAY, 2); floatText(tx + 0.5f, ty, "+2 Hay", WHITE); }
            else {
                addToPlayer(I_FIBER, 2);
                float r = rnd();
                if (r < 0.18f) { addToPlayer(I_WHEATSEED, 1); floatText(tx + 0.5f, ty, "+Wheat seed", Color{255, 230, 150, 255}); }
                else if (r < 0.23f) { addToPlayer(I_BEANS, 1); floatText(tx + 0.5f, ty, "+Bean", Color{255, 230, 150, 255}); }
                else if (r < 0.26f) { addToPlayer(I_POTATO, 1); floatText(tx + 0.5f, ty, "+Wild potato", Color{255, 230, 150, 255}); }
            }
            removeObject(tx, ty, false);
            break;
        case O_REEDS: addToPlayer(I_FIBER, 3); removeObject(tx, ty, false); break;
        case O_BRANCH: addToPlayer(I_STICK, rndi(1, 2)); removeObject(tx, ty, false); break;
        case O_FLINT: addToPlayer(I_FLINT, 1); removeObject(tx, ty, false); break;
        case O_CLAY: addToPlayer(I_CLAY, 2); if (hit(3)) removeObject(tx, ty, false); break;
        case O_MANURE: addToPlayer(I_MANURE, 1); removeObject(tx, ty, false); break;
        case O_EGG: addToPlayer(I_EGG, 1); g->stats.foodMade += ITEMS[I_EGG].food; removeObject(tx, ty, false); break;
        case O_ASH: removeObject(tx, ty, false); break;
        case O_SAPLING: addToPlayer(I_SAPLING, 1); removeObject(tx, ty, false); break;
        case O_LEAFPILE: break;
        default: break;
    }
    p.atkCD = tool == T_AXE || tool == T_PICK ? 0.38f : 0.3f;
}

static void deconstruct(int tx, int ty) {
    Tile& t = g->tiles[IDX(tx, ty)];
    if (!t.obj || !OBJS[t.obj].structure) return;
    int drop = OBJS[t.obj].dropItem;
    bool dam = t.obj == O_DAM;
    removeObject(tx, ty, true);
    if (dam) drainReservoir(tx, ty);
    if (drop > 0) addToPlayer(drop, 1);
    burst(tx + 0.5f, ty + 0.5f, Color{180, 150, 110, 255}, 8);
}

static void attackEnt(Ent& e) {
    Player& p = g->pl;
    int tool = heldTool();
    float dmg = tool == T_SPEAR ? 17 : tool == T_AXE ? 10 : tool == T_PICK ? 8 : 4;
    if (e.kind == E_PORCUPINE && tool != T_SPEAR) { hurtPlayer(4, "porcupine quills"); floatText(p.x, p.y - 1, "Ouch! Quills - use a spear", ORANGE); }
    byPlayer = true;
    damageEnt(e, dmg, p.x, p.y);
    byPlayer = false;
    p.atkCD = tool == T_SPEAR ? 0.45f : 0.55f;
    p.useAnim = 0.2f;
    if (e.alive && e.kind < E_WORKER) {
        if (e.tame) { e.trust = std::max(0.f, e.trust - 30); }
        e.state = 9;  // provoked
        e.target = -2; // player
        e.timer = 6;
    }
}

void playerUseAt(int tx, int ty, Vector2 wm) {
    Player& p = g->pl;
    if (p.atkCD > 0 || p.sleeping) return;
    Stack& h = held();
    int tool = heldTool();
    Ent* e = entAt(wm, 0.9f);
    if (e && dist(p.x, p.y, e->x, e->y) < 2.4f && e->id != p.riding) {
        if (tool == T_SHEARS && e->kind == E_SHEEP && e->tame) {
            if (e->produce >= 1) { e->produce = 0; addToPlayer(I_WOOL, 3); floatText(e->x, e->y - 1, "+3 Wool", WHITE); }
            else floatText(e->x, e->y - 1, "Wool not grown yet", LIGHTGRAY);
            p.atkCD = 0.4f;
            return;
        }
        if (e->kind == E_FROG || e->kind == E_SPIDER) return;
        attackEnt(*e);
        return;
    }
    if (!inb(tx, ty)) return;
    int i = IDX(tx, ty);
    Tile& t = g->tiles[i];
    if (h.id && (ITEMS[h.id].placeObj >= 0 || ITEMS[h.id].placeGround >= 0)) { place(tx, ty); p.atkCD = 0.15f; return; }
    if (!inReach(tx, ty, 2.3f)) return;
    p.atkCD = 0.25f;
    if (IsKeyDown(KEY_LEFT_SHIFT) && t.obj && OBJS[t.obj].structure) { deconstruct(tx, ty); return; }
    uint8_t gr = g->ground[i];
    Crop& c = g->crops[i];
    switch (tool) {
        case T_HOE:
            if ((gr == G_TILLED || gr == G_GREENHOUSE) && c.type) {
                bool gm = c.type == CR_BEANS || c.type == CR_GREENBEAN || c.type == CR_BUCKWHEAT || c.type == CR_MUSTARD;
                floatText(tx + 0.5f, ty, c.dead ? "Dead crop hoed under" : gm ? "Green manure hoed under - soil enriched" : "Crop hoed under", Color{190, 150, 100, 255});
                plowUnder(tx, ty);
                return;
            }
            if (t.obj == O_ASH) {   // wood ash: potassium and a little phosphorus
                removeObject(tx, ty, false);
                addNutrients(i, 0.0f, 0.05f, 0.2f);
                if (gr == G_GRASS || gr == G_DIRT || gr == G_FOREST || gr == G_BURNT) g->ground[i] = G_TILLED;
                floatText(tx + 0.5f, ty, "Wood ash worked in (+K)", Color{190, 190, 190, 255});
                return;
            }
            if (gr == G_SAND && !t.obj) { addToPlayer(I_SAND, 1); floatText(tx + 0.5f, ty, "+Sand", WHITE); return; }
            if ((gr == G_TILLED || gr == G_GREENHOUSE) && !c.type && g->litter[i] > 0.01f) {
                tillInLitter(i);
                floatText(tx + 0.5f, ty, "Manure tilled in - soil enriched", Color{190, 150, 100, 255});
                burst(tx + 0.5f, ty + 0.5f, Color{120, 80, 50, 255}, 6);
                return;
            }
            if ((gr == G_GRASS || gr == G_DIRT || gr == G_FOREST || gr == G_BURNT) && (!t.obj || t.obj == O_TALLGRASS || t.obj == O_ASH)) {
                if (t.obj) removeObject(tx, ty, false);
                g->ground[i] = G_TILLED;
                tillInLitter(i);
                burst(tx + 0.5f, ty + 0.5f, Color{120, 80, 50, 255}, 6);
                return;
            }
            break;
        case T_WBUCKET: {
            bool fireNear = false;
            for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++)
                if (inb(tx + j, ty + k) && g->fire[IDX(tx + j, ty + k)] > 0) { g->fire[IDX(tx + j, ty + k)] = 0; fireNear = true; }
            if (!fireNear)
                for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++)
                    if (inb(tx + j, ty + k)) g->moist[IDX(tx + j, ty + k)] = 1;
            for (int n = 0; n < 14; n++)
                g->parts.push_back({tx + 0.5f, ty + 0.5f, rndr(-2, 2), rndr(-2, 1), 0.5f, 0.5f, Color{120, 180, 255, 220}, 2, 3});
            floatText(tx + 0.5f, ty, fireNear ? "Fire doused!" : "Watered", SKYBLUE);
            h.id = I_BUCKET;
            return;
        }
        case T_BUCKET:
            if (isWater(tx, ty) || t.obj == O_WELL || t.obj == O_POND) { h.id = I_WATERBUCKET; floatText(tx + 0.5f, ty, "Filled bucket", SKYBLUE); return; }
            break;
        case T_SICKLE:
            if (c.type && c.growth >= 1) { harvestCrop(tx, ty, false, 0, 0); return; }
            break;
        default: break;
    }
    // orchard work: cut a scion with shears, graft it onto a sapling, wire-guard trees, fit timers
    if (tool == T_SHEARS && isFruitTree(t.obj) && g->fruitGene[i] >= 0) {
        int left = addItem(p.inv, INV_SLOTS, I_SCION, 1, (float)g->fruitGene[i]);   // the scion carries its variety id
        if (left) floatText(tx + 0.5f, ty, "No room for the scion", LIGHTGRAY);
        else logMsg("Cut a scion: " + geneDesc(g->fruitGene[i]) + ". Use it on a sapling to graft.", Color{200, 230, 160, 255});
        p.atkCD = 0.6f;
        return;
    }
    if (h.id == I_SCION && t.obj == O_SAPLING) {
        int gene = (int)h.fresh;
        if (gene < 0 || gene >= (int)g->genes.size()) return;
        g->fruitGene[i] = gene;
        t.var = (uint8_t)((t.var & ~0x1C) | (5 << 2));
        logMsg("Grafted! This sapling will grow into: " + geneDesc(gene), Color{200, 240, 160, 255});
        burst(tx + 0.5f, ty + 0.5f, Color{140, 220, 120, 255}, 8);
        h = Stack{};
        return;
    }
    if (h.id == I_TREEGUARD && (isTreeObj(t.obj) || t.obj == O_SAPLING)) {
        if (t.var & VAR_GUARD) { floatText(tx + 0.5f, ty, "Already guarded", LIGHTGRAY); return; }
        t.var |= VAR_GUARD;
        floatText(tx + 0.5f, ty, "Wire guard fitted", Color{200, 200, 210, 255});
        if (--h.n <= 0) h = Stack{};
        return;
    }
    if (h.id == I_TIMER && (t.obj == O_SPRINKLER || t.obj == O_DRIP || t.obj == O_WATERFALL)) {
        if (t.var & VAR_TIMER) { floatText(tx + 0.5f, ty, "Already has a timer", LIGHTGRAY); return; }
        t.var |= VAR_TIMER;
        t.timer = 3;
        floatText(tx + 0.5f, ty, "Timer fitted - right-click to set schedule", SKYBLUE);
        if (--h.n <= 0) h = Stack{};
        return;
    }
    // fishing
    if (tool == T_ROD) {
        if (!isWater(tx, ty) || dist(p.x, p.y, tx + 0.5f, ty + 0.5f) > 5) { floatText(tx + 0.5f, ty, "Cast into water", LIGHTGRAY); return; }
        if (p.fishing && p.biteTimer > 0) {
            p.fishing = false; p.biteTimer = 0;
            int n = rnd() < 0.2f ? 2 : 1;
            addToPlayer(I_FISH, n);
            g->stats.foodMade += ITEMS[I_FISH].food * n;
            floatText(tx + 0.5f, ty, g->river[i] ? "Caught a trout!" : "Caught a whitefish!", Color{170, 210, 255, 255});
            burst(tx + 0.5f, ty + 0.5f, Color{170, 210, 255, 255}, 8);
        } else if (p.fishing) { p.fishing = false; floatText(tx + 0.5f, ty, "Reeled in too early", LIGHTGRAY); }
        else {
            p.fishing = true; p.bobX = tx; p.bobY = ty;
            p.fishTimer = rndr(3, 10) * (g->season() == 3 ? 1.6f : 1.f) * (g->tod > 0.75f || g->tod < 0.3f ? 0.7f : 1.f);   // fish bite at dawn and dusk
        }
        p.atkCD = 0.3f;
        return;
    }
    // seeds & soil items
    if (h.id) {
        int crop = 0;
        for (int k = 1; k < CR_COUNT; k++) if (CROPS[k].seed == h.id) crop = k;
        if (crop && (gr == G_TILLED || gr == G_GREENHOUSE) && !c.type && !t.obj) {
            if (gr != G_GREENHOUSE && (g->season() == 3 || (g->season() == 2 && !CROPS[crop].hardy && g->dayOfSeason() > 3))) {
                floatText(tx + 0.5f, ty, "Too cold to plant outdoors", Color{180, 210, 255, 255});
                return;
            }
            c = Crop{};
            c.type = (uint8_t)crop;
            if (--h.n <= 0) h = Stack{};
            burst(tx + 0.5f, ty + 0.5f, Color{120, 80, 50, 255}, 4);
            return;
        }
        if ((h.id == I_COMPOST || h.id == I_MANURE) && (gr == G_TILLED || gr == G_GREENHOUSE)) {
            if (h.id == I_COMPOST) addNutrients(i, 0.1f + 0.2f * h.fresh, 0.08f + 0.15f * h.fresh, 0.08f + 0.15f * h.fresh);
            else addNutrients(i, 0.2f, 0.06f, 0.08f);
            floatText(tx + 0.5f, ty, "Fertilized", Color{180, 140, 90, 255});
            if (--h.n <= 0) h = Stack{};
            return;
        }
        bool sapling = h.id == I_SAPLING || h.id == I_SAP_MAPLE || h.id == I_SAP_ASH || h.id == I_SAP_ASPEN || h.id == I_SAP_SERVICE ||
                       h.id == I_APPLESEED || h.id == I_SAP_PEAR || h.id == I_SAP_APRICOT;
        if (sapling && !t.obj && (gr == G_GRASS || gr == G_FOREST || gr == G_DIRT || gr == G_BURNT || gr == G_TILLED)) {
            placeObject(tx, ty, O_SAPLING);
            t.var = (uint8_t)((t.var & 3) | (treeForSapling(h.id) << 2));
            if (h.id == I_APPLESEED) { g->fruitGene[i] = newGene(FK_APPLE, true); floatText(tx + 0.5f, ty - 1, "A new apple variety - who knows what it'll be?", Color{240, 200, 150, 255}); }
            if (h.id == I_SAP_PEAR) g->fruitGene[i] = newGene(FK_PEAR, false);
            if (h.id == I_SAP_APRICOT) g->fruitGene[i] = newGene(FK_APRICOT, false);
            if (--h.n <= 0) h = Stack{};
            return;
        }
    }
    if (c.type) { harvestCrop(tx, ty, false, 0, 0); return; }
    if (t.obj) gatherObject(tx, ty);
}

static void tryTame(Ent& e) {
    Stack& h = held();
    const SpeciesDef& s = SPECIES[e.kind];
    bool liked = h.id && (h.id == s.tameFood[0] || h.id == s.tameFood[1] || h.id == s.tameFood[2]);
    if (!s.tameFood[0]) { floatText(e.x, e.y - 1, "Can't be tamed", LIGHTGRAY); return; }
    if (!liked) {
        std::string want = std::string(SPECIES[e.kind].name) + " likes " + ITEMS[s.tameFood[0]].name;
        if (s.tameFood[1]) want += std::string(" / ") + ITEMS[s.tameFood[1]].name;
        floatText(e.x, e.y - 1, want, LIGHTGRAY);
        return;
    }
    if (--h.n <= 0) h = Stack{};
    e.hunger = 100;
    if (e.tame) { floatText(e.x, e.y - 1, "Fed", Color{255, 200, 200, 255}); return; }
    e.trust += e.kind == E_DOG ? 35 : (e.kind == E_HORSE || e.kind == E_BUFFALO ? 18 : 26);
    burst(e.x, e.y - 0.6f, Color{255, 120, 160, 255}, 4, 0.5f);
    if (e.trust >= 100) {
        e.tame = true;
        e.follow = true;
        e.state = 0;
        e.homeX = e.x; e.homeY = e.y;
        logMsg(std::string("You tamed a ") + s.name + "! It follows you (right-click to make it stay).", Color{255, 170, 200, 255});
        floatText(e.x, e.y - 1, "Tamed!", Color{255, 150, 190, 255});
    } else floatText(e.x, e.y - 1, "Trust " + std::to_string((int)e.trust) + "%", Color{255, 190, 210, 255});
}

void playerInteractAt(int tx, int ty, Vector2 wm) {
    Player& p = g->pl;
    if (p.sleeping) return;
    Stack& h = held();
    Ent* e = entAt(wm, 0.9f);
    if (e && dist(p.x, p.y, e->x, e->y) < 2.6f) {
        if (e->id == p.riding) { p.riding = -1; p.x += 0.6f; logMsg("You dismount.", LIGHTGRAY); return; }
        switch (e->kind) {
            case E_WORKER: g->openWorker = e->id; openPanel(P_WORKER); return;
            case E_VILLAGER:
                if (e->tribe >= 0 && !g->tribes[e->tribe].hostile) {
                    if (e->state == 1) { g->openTribe = e->tribe; g->tradeRemote = false; openPanel(P_TRADE); }
                    else floatText(e->x, e->y - 1, "Talk to our trader by the banners.", Color{200, 230, 255, 255});
                }
                return;
            case E_RAIDER: floatText(e->x, e->y - 1, "Grrr!", RED); return;
            case E_FROG: case E_SPIDER: floatText(e->x, e->y - 0.5f, e->kind == E_FROG ? "Ribbit (eats pests)" : "Eats pests", Color{180, 230, 160, 255}); return;
            default: break;
        }
        bool liked = h.id && (h.id == SPECIES[e->kind].tameFood[0] || h.id == SPECIES[e->kind].tameFood[1] || h.id == SPECIES[e->kind].tameFood[2]);
        if (!e->tame || (liked && e->hunger < 90)) { tryTame(*e); return; }
        if (e->kind == E_HORSE) { p.riding = e->id; p.x = e->x; p.y = e->y; logMsg("You mount the horse. Right-click it to dismount.", LIGHTGRAY); return; }
        if (e->kind == E_BUFFALO && e->produce >= 1) { e->produce = 0; addToPlayer(I_MILK, 2); g->stats.foodMade += ITEMS[I_MILK].food * 2; floatText(e->x, e->y - 1, "+2 Milk", WHITE); return; }
        if (e->kind == E_SHEEP && e->produce >= 1 && heldTool() == T_SHEARS) { e->produce = 0; addToPlayer(I_WOOL, 3); floatText(e->x, e->y - 1, "+3 Wool", WHITE); return; }
        e->follow = !e->follow;
        if (!e->follow) { e->homeX = e->x; e->homeY = e->y; }
        floatText(e->x, e->y - 1, e->follow ? "Following" : "Staying here", Color{255, 220, 240, 255});
        return;
    }
    if (inb(tx, ty) && inReach(tx, ty, 2.6f)) {
        int i = IDX(tx, ty);
        Tile& t = g->tiles[i];
        if (t.cont >= 0) { g->openCont = t.cont; openPanel(P_CONTAINER); return; }
        switch (t.obj) {
            case O_BED: {
                bool night = g->tod > 0.76f || g->tod < 0.25f;
                if (!night) { floatText(tx + 0.5f, ty, "You can only sleep at night", LIGHTGRAY); return; }
                for (auto& o : g->ents)
                    if (o.alive && (o.kind == E_WOLF || o.kind == E_RAIDER) && o.state != 0 && dist(o.x, o.y, p.x, p.y) < 14) { floatText(tx + 0.5f, ty, "Enemies nearby!", RED); return; }
                p.sleeping = true; p.x = tx + 0.5f; p.y = ty + 0.5f;
                return;
            }
            case O_SPRINKLER: case O_DRIP: case O_WATERFALL: {
                if (lineBroken(t)) { repairLine(tx, ty); floatText(tx + 0.5f, ty, "Line repaired", SKYBLUE); return; }
                if (t.var & VAR_TIMER) t.timer = (float)(((int)t.timer + 1) % 5);
                else t.timer = (int)t.timer == 4 ? 0.f : 4.f;   // without a timer it's just a valve
                floatText(tx + 0.5f, ty, (t.var & VAR_TIMER) ? std::string("Timer: ") + timerName((int)t.timer) : std::string((int)t.timer == 4 ? "Valve closed" : "Valve open (fit a timer to schedule)"), SKYBLUE);
                return;
            }
            case O_BEEHIVE: {
                int n = collectHoney(tx, ty);
                if (n) { addToPlayer(I_HONEY, n); g->stats.foodMade += ITEMS[I_HONEY].food * n; floatText(tx + 0.5f, ty, "+" + std::to_string(n) + " Honey", Color{250, 200, 60, 255}); }
                else floatText(tx + 0.5f, ty, (t.var & VAR_BEES) ? "Honey not ready yet" : "No bees yet - plant flowers nearby", LIGHTGRAY);
                return;
            }
            case O_WELL: p.water = 100; if (heldTool() == T_BUCKET) h.id = I_WATERBUCKET; floatText(tx + 0.5f, ty, "Fresh water", SKYBLUE); return;
            case O_WORKBENCH: case O_KILN: case O_LOOM: case O_TANNER: case O_DRYRACK: case O_CAMPFIRE: openPanel(P_INV); return;
            case O_TRADEPOST:
                for (size_t k = 0; k < g->tribes.size(); k++)
                    if (g->tribes[k].road && !g->tribes[k].hostile) { g->openTribe = (int)k; g->tradeRemote = true; openPanel(P_TRADE); return; }
                floatText(tx + 0.5f, ty, "No road links this post to a village", LIGHTGRAY);
                return;
            default: break;
        }
        if (g->crops[i].type && g->crops[i].growth >= 1) { harvestCrop(tx, ty, false, 0, 0); return; }
        if (isWater(tx, ty) && !t.obj) {
            if (heldTool() == T_BUCKET) { h.id = I_WATERBUCKET; floatText(tx + 0.5f, ty, "Filled bucket", SKYBLUE); return; }
            p.water = std::min(100.f, p.water + 25);
            if (!g->river[i] && rnd() < 0.08f) { p.sick = 40; logMsg("The still water made you queasy. Wells are safer.", Color{170, 220, 90, 255}); }
            floatText(tx + 0.5f, ty, "Drink", SKYBLUE);
            return;
        }
    }
    // use held item on self
    if (h.id) {
        if (ITEMS[h.id].wear) { equipItem(p.sel); return; }
        if (ITEMS[h.id].food > 0 || h.id == I_WATERBUCKET || h.id == I_MOLDY) eatItem(p.sel);
    }
}

// ---------------------------------------------------------------- animal AI
static Ent* nearestEnt(float x, float y, float r, bool (*pred)(const Ent&)) {
    Ent* best = nullptr; float bd = r;
    for (auto& e : g->ents) {
        if (!e.alive || !pred(e)) continue;
        float d = dist(x, y, e.x, e.y);
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}
static bool isPredator(const Ent& e) { return e.kind == E_WOLF || (e.kind == E_BEAR && !e.sleeping); }
static bool isLivestock(const Ent& e) { return e.tame && SPECIES[e.kind].livestock && e.kind != E_HORSE; }
static bool isEnemyOfFarm(const Ent& e) { return e.kind == E_WOLF || e.kind == E_COYOTE || e.kind == E_PORCUPINE || (e.kind == E_RAIDER && e.state >= 1) || (e.kind == E_BEAR && e.state == 9); }
static bool isGuardDog(const Ent& e) { return e.kind == E_DOG && e.tame; }
static bool isWorkerEnt(const Ent& e) { return e.kind == E_WORKER; }
static bool isFarmPerson(const Ent& e) { return e.kind == E_WORKER || (e.kind == E_DOG && e.tame); }

static bool playerHoldsLiked(int kind) {
    int id = held().id;
    const SpeciesDef& s = SPECIES[kind];
    return id && (id == s.tameFood[0] || id == s.tameFood[1] || id == s.tameFood[2]);
}

static bool playerScary(float x, float y) {
    for (int yy = (int)y - 4; yy <= (int)y + 4; yy++)
        for (int xx = (int)x - 4; xx <= (int)x + 4; xx++)
            if (inb(xx, yy)) {
                uint8_t o = g->tiles[IDX(xx, yy)].obj;
                if ((o == O_CAMPFIRE || o == O_TORCHPOST || o == O_BONFIRE) && dist(x, y, xx, yy) < 4) return true;
            }
    return held().id == I_TORCH && dist(x, y, g->pl.x, g->pl.y) < 5;
}

static void chase(Ent& e, float tx, float ty, float spd, float dt, bool breakStuff);
static void wander(Ent& e, float dt, float radius, float spd) {
    e.timer -= dt;
    if (e.timer <= 0) {
        e.timer = rndr(1.5f, 5.f);
        if (rnd() < 0.45f) { e.vx = e.vy = 0; }
        else {
            float a = rndr(0, 6.283f);
            float hx = e.homeX - e.x, hy = e.homeY - e.y;
            if (std::sqrt(hx * hx + hy * hy) > radius) a = std::atan2(hy, hx) + rndr(-0.6f, 0.6f);
            e.vx = std::cos(a); e.vy = std::sin(a);
        }
    }
    if (e.vx || e.vy) {
        if (std::fabs(e.vx) > 0.1f) e.face = e.vx > 0 ? 1.f : -1.f;
        e.anim += dt * spd * 2.f;
        if (!moveBody(e.x, e.y, e.vx * spd * dt * speedMul(e.x, e.y), e.vy * spd * dt * speedMul(e.x, e.y), whoFor(e))) e.timer = 0;
        // ducks love water; land animals avoid it
        int tx = (int)e.x, ty = (int)e.y;
        if (e.kind != E_DUCK && e.kind != E_FROG && isWater(tx, ty) && rnd() < 0.2f) e.timer = 0;
    }
}

static void flee(Ent& e, float fx, float fy, float spd, float dt) {
    float dx = e.x - fx, dy = e.y - fy, L = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
    float a = std::atan2(dy, dx) + std::sin(g->totalTime * 2 + e.id) * 0.5f;
    if (std::fabs(dx) > 0.1f) e.face = dx > 0 ? 1.f : -1.f;
    e.anim += dt * spd * 2.f;
    if (!moveBody(e.x, e.y, std::cos(a) * spd * dt, std::sin(a) * spd * dt, whoFor(e))) {
        moveBody(e.x, e.y, -std::sin(a) * spd * dt, std::cos(a) * spd * dt, whoFor(e));
    }
    (void)L;
}

// chase a moving target; breaks fences/walls that are in the way
static void chase(Ent& e, float tx, float ty, float spd, float dt, bool breakStuff) {
    e.repath -= dt;
    if (e.repath <= 0 || e.path.empty()) {
        e.repath = 1.2f;
        e.pathI = 0;
        if (!findPath((int)e.x, (int)e.y, (int)tx, (int)ty, whoFor(e), e.path, 2500)) e.path.clear();
    }
    bool moved;
    if (!e.path.empty() && e.pathI < (int)e.path.size() && dist(e.x, e.y, tx, ty) > 1.5f) {
        PathPt p = e.path[e.pathI];
        moved = stepToward(e, p.x + 0.5f, p.y + 0.5f, spd, dt);
        if (dist(e.x, e.y, p.x + 0.5f, p.y + 0.5f) < 0.3f) e.pathI++;
    } else moved = stepToward(e, tx, ty, spd, dt);
    if (!moved && breakStuff) {
        // hit whatever blocks the straight line
        float dx = tx - e.x, dy = ty - e.y, L = std::max(0.01f, std::sqrt(dx * dx + dy * dy));
        int bx = (int)(e.x + dx / L * 0.7f), by = (int)(e.y + dy / L * 0.7f);
        if (inb(bx, by)) {
            Tile& t = g->tiles[IDX(bx, by)];
            if (t.obj && OBJS[t.obj].structure) {
                e.timer2 -= dt;
                if (e.timer2 <= 0) {
                    e.timer2 = 1.f;
                    t.hp -= 1;
                    burst(bx + 0.5f, by + 0.5f, Color{150, 110, 70, 255}, 3);
                    if (t.hp <= 0) {
                        logMsg(std::string(SPECIES[e.kind].name) + "s broke through your " + OBJS[t.obj].name + "!", ORANGE);
                        removeObject(bx, by, false);
                    }
                }
            }
        }
    }
}

static void melee(Ent& e, Ent* target, bool hitPlayer, float dt) {
    e.timer2 -= dt;
    if (e.timer2 > 0) return;
    e.timer2 = e.kind == E_BEAR ? 1.4f : 1.0f;
    float dmg = SPECIES[e.kind].dmg;
    if (hitPlayer) hurtPlayer(dmg, (std::string("killed by a ") + SPECIES[e.kind].name).c_str());
    else if (target) {
        damageEnt(*target, dmg, e.x, e.y);
        if (target->kind == E_PORCUPINE) damageEnt(e, 3, target->x, target->y);   // a mouthful of quills
    }
}

// food an animal will take from a trough (or, for dogs, from storage)
static bool eatsItem(int kind, int id) {
    bool meat = id == I_RAWMEAT || id == I_JERKY || id == I_COOKEDMEAT;
    if (!SPECIES[kind].herbivore) return meat;
    return id == I_HAY || id == I_WHEAT || id == I_CABBAGE || id == I_BERRIES || id == I_WHEATSEED || id == I_BEANS || id == I_APPLE_SPITTER || id == I_SQUASH;
}

static bool eatFrom(Ent& e, Container& c) {
    for (auto& s : c.slots)
        if (s.id && eatsItem(e.kind, s.id)) {
            int id = s.id;
            takeItem(c.slots.data(), (int)c.slots.size(), id, 1);
            e.hunger = 100;
            return true;
        }
    return false;
}

// Hungry penned animals walk to the nearest trough with suitable feed. Returns true while busy.
static bool seekFeed(Ent& e, float dt) {
    int best = -1; float bd = 14;
    for (size_t i = 0; i < g->conts.size(); i++) {
        Container& c = g->conts[i];
        bool poultry = e.kind == E_DUCK || e.kind == E_FOWL;
        bool ok = c.alive && (c.kind == C_TROUGH || (e.kind == E_DOG && (c.kind == C_CHEST || c.kind == C_GRANARY || c.kind == C_CELLAR)) || (poultry && c.kind == C_COMPOST));
        if (!ok) continue;
        bool has = false;
        if (c.kind == C_COMPOST) { int items = 0; compostRatio(c, &items); has = items >= 2; }   // larvae
        else for (auto& s : c.slots) if (s.id && eatsItem(e.kind, s.id)) { has = true; break; }
        if (!has) continue;
        float d = dist(e.x, e.y, c.x + 0.5f, c.y + 0.5f);
        if (d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) return false;
    Container& c = g->conts[best];
    if (bd < 1.6f) {
        if (c.kind == C_COMPOST) { e.hunger = std::min(100.f, e.hunger + dt * 25); return e.hunger < 95; }   // pecking larvae
        eatFrom(e, c);
        return false;
    }
    chase(e, c.x + 0.5f, c.y + 0.5f, SPECIES[e.kind].speed * 0.6f, dt, false);
    return true;
}

static void tameNeeds(Ent& e, float dt) {
    if (e.kind == E_FROG || e.kind == E_SPIDER) return;
    e.hunger -= dt * 100.f / (DAY_SECONDS * (e.kind == E_DOG ? 2.5f : 1.2f));
    bool grazable = g->season() != 3 && g->snowCover < 0.5f;
    int tx = (int)e.x, ty = (int)e.y;
    if (e.hunger < 70 && SPECIES[e.kind].herbivore && grazable && inb(tx, ty)) {
        uint8_t gr = g->ground[IDX(tx, ty)];
        bool forage = gr == G_GRASS || gr == G_FOREST || gr == G_DIRT;
        if (e.kind == E_DUCK) forage = forage || gr == G_WATER || gr == G_SAND;   // ducks dabble in shallows
        if (e.kind == E_FOWL) forage = forage || gr == G_TILLED || gr == G_VILLAGE;
        if (forage) e.hunger = std::min(100.f, e.hunger + dt * 6);
    }
    if (e.hunger <= 0) {
        e.hunger = 0;
        e.hp -= dt * 0.06f;
        if (e.hp <= 0) {
            e.alive = false;
            g->stats.livestockLost++;
            logMsg(std::string("Your ") + SPECIES[e.kind].name + " starved. Keep pasture nearby and troughs full.", ORANGE);
            extern bool g_echoLog;
            if (g_echoLog) {
                printf("    starve at %.1f,%.1f home %.1f,%.1f follow %d baby %d\n", e.x, e.y, e.homeX, e.homeY, e.follow, e.baby);
                for (auto& c : g->conts) if (c.alive && c.kind == C_TROUGH) { printf("    trough %d,%d d=%.1f:", c.x, c.y, dist(e.x, e.y, c.x, c.y)); for (auto& st : c.slots) if (st.id) printf(" %s x%d", ITEMS[st.id].name, st.n); printf("\n"); }
            }
        }
    }
    // produce
    float pdays = e.kind == E_SHEEP ? 2.5f : (e.kind == E_BUFFALO ? 1.f : (e.kind == E_DUCK || e.kind == E_FOWL ? 1.f : 0));
    if (pdays > 0 && !e.baby && e.hunger > 20) e.produce = std::min(1.f, e.produce + dt / (DAY_SECONDS * pdays));
    if ((e.kind == E_DUCK || e.kind == E_FOWL) && e.produce >= 1) {
        for (int k = 0; k < 8; k++) {
            int x = tx + rndi(-1, 1), y = ty + rndi(-1, 1);
            if (inb(x, y) && !g->tiles[IDX(x, y)].obj && !isWater(x, y) && !g->crops[IDX(x, y)].type) { placeObject(x, y, O_EGG); e.produce = 0; break; }
        }
    }
    // manure
    if (SPECIES[e.kind].livestock && e.kind != E_DUCK && e.kind != E_FOWL) {
        e.age += 0;  // growth handled separately
        e.timer2 += dt;
        if (e.timer2 > DAY_SECONDS * (e.kind == E_BUFFALO ? 0.5f : 0.9f)) {
            e.timer2 = 0;
            if (inb(tx, ty) && !g->tiles[IDX(tx, ty)].obj && !g->crops[IDX(tx, ty)].type && !isWater(tx, ty)) placeObject(tx, ty, O_MANURE);
        }
    }
    // poultry: droppings build up wherever they're kept; free-ranging birds raid unfenced crops
    if ((e.kind == E_FOWL || e.kind == E_DUCK) && inb(tx, ty) && !isWater(tx, ty)) {
        g->litter[IDX(tx, ty)] = std::min(1.5f, g->litter[IDX(tx, ty)] + dt / DAY_SECONDS * (e.kind == E_FOWL ? 1.f : 0.6f));
        for (int k = 0; k < 5; k++) {
            int cx = tx + (k == 1) - (k == 2), cy = ty + (k == 3) - (k == 4);
            if (!inb(cx, cy)) continue;
            Crop& c = g->crops[IDX(cx, cy)];
            if (!c.type || c.dead || e.hunger > 85 || dist(e.x, e.y, cx + 0.5f, cy + 0.5f) > 0.9f) continue;
            c.growth -= dt * 0.08f;
            e.hunger = std::min(100.f, e.hunger + dt * 4);
            if (c.growth <= 0) {
                c = Crop{};
                g->stats.cropsLost++;
                static float last = -1000;
                if (g->totalTime - last > 60 || g->totalTime < last) {
                    last = g->totalTime;
                    logMsg("Your free-range birds are eating the crops! Fence the garden or pen the flock.", ORANGE);
                }
            }
            break;
        }
    }
    if (e.baby) {
        e.age += dt / DAY_SECONDS;
        if (e.age > 3) { e.baby = false; e.maxhp = SPECIES[e.kind].hp; }
    }
}

static int lastBreedDay = -1;
static void breeding() {
    if (lastBreedDay == g->day) return;
    lastBreedDay = g->day;
    for (int k : {E_SHEEP, E_BUFFALO, E_HORSE, E_DUCK, E_FOWL, E_DOG}) {
        std::vector<Ent*> adults;
        int total = 0;
        for (auto& e : g->ents) if (e.alive && e.tame && e.kind == k) { total++; if (!e.baby && e.hunger > 40) adults.push_back(&e); }
        if (adults.size() < 2 || total >= 24 || g->season() == 3) continue;
        int births = std::min(3, (int)adults.size() / 2);
        for (int b = 0; b < births; b++) {
            if (rnd() > 0.15f) continue;
            Ent* m = adults[rndi(0, (int)adults.size() - 1)];
            float mx = m->x, my = m->y, hx = m->homeX, hy = m->homeY;
            bool mf = m->follow;
            Ent& baby = spawnEnt(k, mx + 0.3f, my + 0.2f);
            baby.tame = true; baby.baby = true; baby.trust = 100; baby.hp = baby.maxhp = SPECIES[k].hp * 0.4f;
            baby.homeX = hx; baby.homeY = hy; baby.follow = mf;
            logMsg(std::string("A baby ") + SPECIES[k].name + " was born!", Color{255, 200, 220, 255});
        }
    }
}

static void updateAnimal(Ent& e, float dt) {
    const SpeciesDef& s = SPECIES[e.kind];
    Player& p = g->pl;
    float pd = dist(e.x, e.y, p.x, p.y);
    float spd = s.speed * (e.baby ? 0.8f : 1.f);
    if (e.id == p.riding) { tameNeeds(e, dt); return; }
    if (e.tame) tameNeeds(e, dt);
    if (!e.alive) return;
    bool noPasture = !SPECIES[e.kind].herbivore || g->season() == 3 || g->snowCover >= 0.5f || e.hunger < 30 ||
                     ((e.kind == E_DUCK || e.kind == E_FOWL) && e.hunger < 60);   // poultry love a compost heap
    if (e.tame && !e.follow && e.hunger < 45 && noPasture && e.kind != E_FROG && e.kind != E_SPIDER) {
        bool threatened = nearestEnt(e.x, e.y, 6, isPredator) != nullptr;
        if (!threatened && seekFeed(e, dt)) return;
    }

    // bears hibernate
    if (e.kind == E_BEAR) {
        bool winter = g->season() == 3;
        if (winter && e.state != 9) { e.sleeping = true; return; }
        e.sleeping = false;
    }

    // provoked: fight back or flee
    if (e.state == 9) {
        e.timer -= dt;
        if (e.timer <= 0) e.state = 0;
        bool fighter = e.kind == E_BEAR || e.kind == E_BUFFALO || e.kind == E_WOLF || (e.kind == E_DOG && !e.tame);
        if (fighter) {
            if (pd < 1.0f) melee(e, nullptr, true, dt);
            else chase(e, p.x, p.y, spd * 1.1f, dt, false);
        } else flee(e, p.x, p.y, spd * 1.6f, dt);
        return;
    }

    switch (e.kind) {
        case E_WOLF: {
            if (e.state == 2) {  // retreat: run off and vanish once out of sight
                flee(e, p.x, p.y, spd, dt);
                e.timer -= dt;
                if (pd > 22 || e.timer < -40) e.alive = false;
                return;
            }
            if (playerScary(e.x, e.y)) { flee(e, p.x, p.y, spd, dt); return; }
            if (e.hp < e.maxhp * 0.5f) { e.state = 2; return; }
            int pack = 0;
            for (auto& o : g->ents) if (o.alive && o.kind == E_WOLF && dist(o.x, o.y, e.x, e.y) < 6) pack++;
            Ent* dog = nearestEnt(e.x, e.y, 7, isGuardDog);
            if (dog) {
                int dogs = 0;
                for (auto& o : g->ents) if (o.alive && isGuardDog(o) && dist(o.x, o.y, e.x, e.y) < 8) dogs++;
                if (pack <= dogs + 1 || sunLevel() > 0.5f) { flee(e, dog->x, dog->y, spd, dt); return; }
            }
            Ent* prey = nearestEnt(e.x, e.y, 40, isLivestock);
            float preyD = prey ? dist(e.x, e.y, prey->x, prey->y) : 1e9f;
            bool huntPlayer = (pd < 7 && (g->season() == 3 || pd < preyD)) || (!prey && pd < 30);
            if (huntPlayer) {
                if (pd < 0.9f) melee(e, nullptr, true, dt);
                else chase(e, p.x, p.y, spd, dt, false);
            } else if (prey) {
                if (preyD < 0.9f) {
                    melee(e, prey, false, dt);
                    if (!prey->alive)  // the pack drags off its kill
                        for (auto& o : g->ents) if (o.alive && o.kind == E_WOLF && dist(o.x, o.y, e.x, e.y) < 10) o.state = 2;
                }
                else chase(e, prey->x, prey->y, spd, dt, true);
            } else wander(e, dt, 8, spd * 0.5f);
            return;
        }
        case E_BEAR: {
            if (pd < 3.2f && !playerScary(e.x, e.y)) { e.state = 9; e.timer = 6; return; }
            wander(e, dt, 10, spd * 0.4f);
            return;
        }
        case E_PORCUPINE: {
            // bark is winter food; the rest of the year they browse the undergrowth
            // shuffle to the nearest living deciduous tree and strip its bark
            if (e.tx < 0 || !inb(e.tx, e.ty) || !isDeciduous(g->tiles[IDX(e.tx, e.ty)].obj) || (g->tiles[IDX(e.tx, e.ty)].var & VAR_GUARD)) {
                e.tx = -1;
                float bd = 25;
                for (int y = (int)e.y - 25; y <= (int)e.y + 25; y++)
                    for (int x = (int)e.x - 25; x <= (int)e.x + 25; x++) {
                        if (!inb(x, y)) continue;
                        uint8_t o = g->tiles[IDX(x, y)].obj;
                        if (!isDeciduous(o) || (g->tiles[IDX(x, y)].var & VAR_GUARD)) continue;
                        float d = dist(e.x, e.y, x, y) - (g->ground[IDX(x, y)] != G_FOREST ? 8 : 0);   // orchards and shade trees first
                        if (d < bd) { bd = d; e.tx = x; e.ty = y; }
                    }
                if (e.tx < 0) { wander(e, dt, 12, spd * 0.5f); return; }
            }
            if (dist(e.x, e.y, e.tx + 0.5f, e.ty + 0.5f) > 1.2f) { chase(e, e.tx + 0.5f, e.ty + 0.5f, spd, dt, false); return; }
            e.timer2 += dt;
            e.anim += dt * 3;
            if (e.timer2 > 120.f) {   // ring-barking takes ~1.5 days per tree
                e.timer2 = 0;
                Tile& t = g->tiles[IDX(e.tx, e.ty)];
                if (--t.hp <= 0) {
                    static float lastMsg = -1000;
                    if (g->totalTime - lastMsg > 40 || g->totalTime < lastMsg) {
                        lastMsg = g->totalTime;
                        logMsg(std::string("A porcupine ring-barked a ") + OBJS[t.obj].name + " - the tree died. Hunt them down!", Color{220, 180, 120, 255});
                    }
                    removeObject(e.tx, e.ty, false);
                    placeObject(e.tx, e.ty, O_STUMP);
                    e.tx = -1;
                }
            }
            return;
        }
        case E_COYOTE: {
            if (e.state == 2) {
                flee(e, p.x, p.y, spd, dt);
                e.timer -= dt;
                if (pd > 22 || e.timer < -40) e.alive = false;
                return;
            }
            // timid: run from people, fire and dogs
            if (pd < 6 || playerScary(e.x, e.y)) { flee(e, p.x, p.y, spd * 1.1f, dt); return; }
            if (Ent* dog = nearestEnt(e.x, e.y, 7, isGuardDog)) { flee(e, dog->x, dog->y, spd * 1.1f, dt); return; }
            Ent* w = nearestEnt(e.x, e.y, 7, isWorkerEnt);
            if (w) { flee(e, w->x, w->y, spd, dt); return; }
            // poultry first, otherwise chew the nearest intact irrigation line
            Ent* bird = nearestEnt(e.x, e.y, 8, [](const Ent& o) { return o.tame && (o.kind == E_DUCK || o.kind == E_FOWL); });
            if (bird) {
                if (dist(e.x, e.y, bird->x, bird->y) < 0.9f) { melee(e, bird, false, dt); if (!bird->alive) e.state = 2; }
                else chase(e, bird->x, bird->y, spd, dt, false);
                return;
            }
            if (e.tx < 0 || !inb(e.tx, e.ty) || lineBroken(g->tiles[IDX(e.tx, e.ty)]) ||
                (g->tiles[IDX(e.tx, e.ty)].obj != O_DRIP && g->tiles[IDX(e.tx, e.ty)].obj != O_SPRINKLER)) {
                e.tx = -1;
                float bd = 40;
                for (int y = (int)e.y - 40; y <= (int)e.y + 40; y++)
                    for (int x = (int)e.x - 40; x <= (int)e.x + 40; x++) {
                        if (!inb(x, y)) continue;
                        const Tile& t = g->tiles[IDX(x, y)];
                        if ((t.obj != O_DRIP && t.obj != O_SPRINKLER) || lineBroken(t)) continue;
                        float d = dist(e.x, e.y, x, y);
                        if (d < bd) { bd = d; e.tx = x; e.ty = y; }
                    }
                if (e.tx < 0) { wander(e, dt, 10, spd * 0.4f); return; }
            }
            if (dist(e.x, e.y, e.tx + 0.5f, e.ty + 0.5f) > 0.9f) { chase(e, e.tx + 0.5f, e.ty + 0.5f, spd * 0.7f, dt, false); return; }
            e.timer2 += dt;
            e.anim += dt * 4;
            if (e.timer2 > 4.f) { e.timer2 = 0; breakLine(e.tx, e.ty); e.tx = -1; if (rnd() < 0.8f) e.state = 2; }
            return;
        }
        case E_FROG: case E_SPIDER: {
            int ox, oy;
            if (!nearObj(e.homeX, e.homeY, e.kind == E_FROG ? O_POND : O_LEAFPILE, 1.5f, &ox, &oy)) { e.alive = false; return; }
            wander(e, dt, 2.5f, spd * (e.kind == E_FROG ? 1.f : 0.6f));
            return;
        }
        default: break;
    }

    // dogs
    if (e.kind == E_DOG) {
        if (e.tame) {
            float cx = e.follow ? p.x : e.homeX, cy = e.follow ? p.y : e.homeY;
            Ent* threat = nearestEnt(cx, cy, 11, isEnemyOfFarm);
            if (threat && dist(e.x, e.y, threat->x, threat->y) < 14) {
                if (dist(e.x, e.y, threat->x, threat->y) < 0.9f) melee(e, threat, false, dt);
                else chase(e, threat->x, threat->y, spd * 1.1f, dt, false);
                if (rnd() < dt * 0.6f) floatText(e.x, e.y - 1, "Woof!", WHITE);
                return;
            }
            if (e.follow) {
                if (pd > 2.5f) { if (pd > 25) { e.x = p.x; e.y = p.y; } else chase(e, p.x, p.y, spd * (pd > 6 ? 1.2f : 0.8f), dt, false); }
                else wander(e, dt, 2, spd * 0.3f);
            } else wander(e, dt, 5, spd * 0.4f);
            return;
        }
        if (pd < 4 && !playerHoldsLiked(e.kind)) { wander(e, dt, 8, spd * 0.3f); return; }
        if (pd < 6 && playerHoldsLiked(e.kind)) { if (pd > 1.5f) stepToward(e, p.x, p.y, spd * 0.5f, dt); return; }
        wander(e, dt, 12, spd * 0.4f);
        return;
    }

    // herbivores
    Ent* pred = nearestEnt(e.x, e.y, 6, isPredator);
    if (pred) { flee(e, pred->x, pred->y, spd * 1.5f, dt); return; }
    if (e.tame) {
        if (e.follow) {
            if (pd > 2.2f) { if (pd > 25) { e.x = p.x; e.y = p.y; } else chase(e, p.x, p.y, std::max(spd, 3.5f) * (pd > 5 ? 1.3f : 0.9f), dt, false); }
            else wander(e, dt, 1.5f, spd * 0.3f);
        } else {
            if ((e.kind == E_FOWL || e.kind == E_DUCK) && e.hunger < 85 && rnd() < dt * 0.3f) {
                // scratch toward any crop in reach - only fences stop them
                for (int k = 0; k < 12; k++) {
                    int cx = (int)e.x + rndi(-6, 6), cy = (int)e.y + rndi(-6, 6);
                    if (inb(cx, cy) && g->crops[IDX(cx, cy)].type && !g->crops[IDX(cx, cy)].dead) { e.vx = cx + 0.5f - e.x; e.vy = cy + 0.5f - e.y; float L = std::sqrt(e.vx * e.vx + e.vy * e.vy); if (L > 0) { e.vx /= L; e.vy /= L; } e.timer = 3; break; }
                }
            }
            wander(e, dt, 5, spd * 0.4f);
        }
        return;
    }
    float fleeR = e.kind == E_HORSE ? 6.f : (e.kind == E_BUFFALO ? 3.f : 4.5f);
    if (pd < fleeR && !playerHoldsLiked(e.kind) && e.trust < 50 && !p.sleeping) { flee(e, p.x, p.y, spd * 1.4f, dt); return; }
    if (playerHoldsLiked(e.kind) && pd < 6 && pd > 1.6f) { stepToward(e, p.x, p.y, spd * 0.4f, dt); return; }
    // ducks drift toward water
    if (e.kind == E_DUCK && rnd() < dt * 0.1f) {
        for (int k = 0; k < 10; k++) {
            int x = (int)e.x + rndi(-5, 5), y = (int)e.y + rndi(-5, 5);
            if (isWater(x, y) && !solidFor(x, y, 1)) { e.homeX = x + 0.5f; e.homeY = y + 0.5f; break; }
        }
    }
    wander(e, dt, 6, spd * 0.5f);
}

// ---------------------------------------------------------------- people
enum Task { TK_NONE, TK_EAT, TK_DRINK, TK_SLEEP, TK_DELIVER, TK_HARVEST, TK_PLANT, TK_WATER, TK_FERT, TK_TILL, TK_CHOP,
            TK_REPLANT, TK_FILLTROUGH, TK_SHEAR, TK_MILK, TK_COLLECT, TK_COOK, TK_PATROL, TK_GOHOME, TK_CUTHAY, TK_REPAIR, TK_PICK, TK_MINE, TK_FISH, TK_HONEY };

static int findContWith(int id, float x, float y) {
    int best = -1; float bd = 1e9f;
    for (size_t i = 0; i < g->conts.size(); i++) {
        Container& c = g->conts[i];
        if (!c.alive || !(c.kind == C_CHEST || c.kind == C_GRANARY || c.kind == C_CELLAR || c.kind == C_FRIDGE)) continue;
        if (countItem(c.slots.data(), (int)c.slots.size(), id) <= 0) continue;
        float d = dist(x, y, c.x, c.y);
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

static bool waterSource(float x, float y, int& wx, int& wy) {
    if (nearObj(x, y, O_WELL, 40, &wx, &wy) && !unreachable(wx, wy)) return true;
    float bd = 1e9f; bool found = false;
    for (int yy = (int)y - 25; yy <= (int)y + 25; yy++)
        for (int xx = (int)x - 25; xx <= (int)x + 25; xx++)
            if (inb(xx, yy) && g->ground[IDX(xx, yy)] == G_WATER && !unreachable(xx, yy)) {
                float d = dist(x, y, xx, yy);
                if (d < bd) { bd = d; wx = xx; wy = yy; found = true; }
            }
    return found;
}

static const int WORKER_FOODS[] = {I_STEW, I_COOKEDMEAT, I_COOKEDFISH, I_BREAD, I_HONEY, I_MUSTARDGREENS, I_APPLE_PRIZE, I_JERKY, I_EGG, I_MILK, I_SQUASH, I_TOMATO,
                                   I_GREENBEANS, I_PEAR, I_APRICOT, I_GRAPES, I_APPLE, I_RASPBERRY, I_BLACKBERRY, I_CABBAGE, I_POTATO,
                                   I_BERRIES, I_BEANS, I_APPLE_SPITTER, I_WHEAT};

int dietVariety(const Ent& e) {
    int n = 0;
    for (int k = 0; k < 12; k++) {
        if (!e.diet[k]) continue;
        bool seen = false;
        for (int j = 0; j < k; j++) if (e.diet[j] == e.diet[k]) seen = true;
        if (!seen) n++;
    }
    return n;
}

// Workers rotate through what's in storage: the food they've had least recently wins, then preference.
static int bestFood(float x, float y, int& cont, const Ent* who = nullptr) {
    int best = 0, bestScore = 1 << 30;
    for (int rank = 0; rank < (int)(sizeof(WORKER_FOODS) / sizeof(int)); rank++) {
        int id = WORKER_FOODS[rank];
        int c = findContWith(id, x, y);
        if (c < 0 || unreachable(g->conts[c].x, g->conts[c].y)) continue;
        int recent = 0;
        if (who) for (int k = 0; k < 12; k++) if (who->diet[k] == id) recent++;
        int score = recent * 100 + rank;
        if (score < bestScore) { bestScore = score; best = id; cont = c; }
    }
    return best;
}

static void startTask(Ent& e, int task, int tx, int ty) {
    e.task = task; e.taskStage = 0; e.tx = -9; e.ty = -9; e.path.clear();
    e.timer = 60;  // abandon timeout
    e.state = tx; e.target = ty;   // stash target tile in state/target
}

static bool someoneTargets(const Ent& self, int task, int x, int y) {
    if (task != TK_SHEAR && task != TK_MILK && unreachable(x, y)) return true;
    for (auto& e : g->ents)
        if (e.alive && e.kind == E_WORKER && e.id != self.id && e.task == task && e.state == x && e.target == y) return true;
    return false;
}

static bool sprinklerCovers(int x, int y) { return irrigationCovers(x, y); }

// Would this crop ripen before the autumn frost? Farmers don't waste seed on crops that can't.
static bool plantSeason(int crop, int i) {
    if (g->ground[i] == G_GREENHOUSE) return true;
    int s = g->season();
    if (s == 3) return false;
    if (s == 2) return CROPS[crop].hardy && g->dayOfSeason() < 5;
    return true;
}

// Put compostable material in the nearest bin with room, otherwise in storage.
static void toCompost(int id, int n, float x, float y) {
    for (auto& c : g->conts) {
        if (!c.alive || c.kind != C_COMPOST || dist(x, y, c.x, c.y) > 40) continue;
        n = addItem(c.slots.data(), (int)c.slots.size(), id, n, 1);
        if (n <= 0) return;
    }
    storeAnywhere(id, n, 1, x, y);
}

static void planFarmer(Ent& e, float bx, float by) {
    int R = 26;
    int ripe = -1, dead = -1, dry = -1, empty = -1, poor = -1, tillable = -1;
    float dR = 1e9f, dD = 1e9f, dDry = 1e9f, dE = 1e9f, dP = 1e9f, dT = 1e9f;
    int tilledCount = 0, emptyCount = 0;
    for (int y = (int)by - R; y <= (int)by + R; y++)
        for (int x = (int)bx - R; x <= (int)bx + R; x++) {
            if (!inb(x, y)) continue;
            int i = IDX(x, y);
            uint8_t gr = g->ground[i];
            if (gr != G_TILLED && gr != G_GREENHOUSE) continue;
            tilledCount++;
            Crop& c = g->crops[i];
            float d = dist(e.x, e.y, x, y);
            if (c.type) {
                if (c.dead) { if (d < dD && !someoneTargets(e, TK_HARVEST, x, y)) { dD = d; dead = i; } }
                else if (c.growth >= 1 || (c.manure && c.growth >= 0.8f)) { if (d < dR && !someoneTargets(e, TK_HARVEST, x, y)) { dR = d; ripe = i; } }
                else if (g->moist[i] < 0.3f && !sprinklerCovers(x, y)) { if (d < dDry && !someoneTargets(e, TK_WATER, x, y)) { dDry = d; dry = i; } }
            } else if (!g->tiles[i].obj) {
                emptyCount++;
                if (soilMin(i) < 0.4f) { if (d < dP && !someoneTargets(e, TK_FERT, x, y)) { dP = d; poor = i; } }
                if (d < dE && !someoneTargets(e, TK_PLANT, x, y)) { dE = d; empty = i; }
            }
        }
    if (ripe >= 0) { startTask(e, TK_HARVEST, ripe % WW, ripe / WW); return; }
    for (int y = (int)by - R; y <= (int)by + R; y++)
        for (int x = (int)bx - R; x <= (int)bx + R; x++)
            if (inb(x, y) && isFruitPlant(g->tiles[IDX(x, y)].obj) && g->fruitState[IDX(x, y)] == 1 && !someoneTargets(e, TK_PICK, x, y)) { startTask(e, TK_PICK, x, y); return; }
    if (dead >= 0) { startTask(e, TK_HARVEST, dead % WW, dead / WW); return; }
    if (dry >= 0) { startTask(e, TK_WATER, dry % WW, dry / WW); return; }
    if (poor >= 0 && (storageCount(I_COMPOST) > 0 || storageCount(I_MANURE) > 0)) { startTask(e, TK_FERT, poor % WW, poor / WW); return; }
    if (empty >= 0) {
        int seeds = 0;
        for (int c = 1; c < CR_COUNT; c++) seeds += storageCount(CROPS[c].seed);
        if (seeds > 0 && (plantSeason(CR_CABBAGE, empty) || plantSeason(CR_WHEAT, empty))) { startTask(e, TK_PLANT, empty % WW, empty / WW); return; }
    }
    // expand the farm when there are spare seeds
    int seeds = storageCount(I_WHEATSEED) + storageCount(I_POTATO) + storageCount(I_BEANS);
    if (emptyCount == 0 && seeds >= 6 && tilledCount < 220 && g->season() <= 1) {
        for (int y = (int)by - R; y <= (int)by + R; y++)
            for (int x = (int)bx - R; x <= (int)bx + R; x++) {
                if (!inb(x, y)) continue;
                int i = IDX(x, y);
                uint8_t gr = g->ground[i];
                if (!(gr == G_GRASS || gr == G_DIRT) || (g->tiles[i].obj && g->tiles[i].obj != O_TALLGRASS)) continue;
                if (nearObj(x + 0.5f, y + 0.5f, O_TROUGH, 6) || nearObj(x + 0.5f, y + 0.5f, O_FENCE, 3) || nearObj(x + 0.5f, y + 0.5f, O_COOP, 5)) continue;  // leave pasture
                bool adj = false;
                for (int k = 0; k < 4; k++) {
                    int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
                    if (inb(nx, ny) && g->ground[IDX(nx, ny)] == G_TILLED) adj = true;
                }
                float d = dist(e.x, e.y, x, y);
                if (adj && d < dT && !someoneTargets(e, TK_TILL, x, y)) { dT = d; tillable = i; }
            }
        if (tillable >= 0) { startTask(e, TK_TILL, tillable % WW, tillable / WW); return; }
    }
}

static void planWoodcutter(Ent& e, float bx, float by) {
    if (e.carry.id) { startTask(e, TK_DELIVER, 0, 0); return; }
    if (storageCount(I_SAPLING) > 0 && g->season() != 3) {
        // replant at the forest edge, well away from buildings
        for (int k = 0; k < 40; k++) {
            float a = rndr(0, 6.283f), d = rndr(14, 24);
            int x = (int)(bx + std::cos(a) * d), y = (int)(by + std::sin(a) * d);
            if (!inb(x, y) || g->tiles[IDX(x, y)].obj) continue;
            uint8_t gr = g->ground[IDX(x, y)];
            if (gr != G_GRASS && gr != G_FOREST && gr != G_BURNT) continue;
            bool nearStruct = false;
            for (int yy = y - 8; yy <= y + 8 && !nearStruct; yy++)
                for (int xx = x - 8; xx <= x + 8; xx++)
                    if (inb(xx, yy) && g->tiles[IDX(xx, yy)].obj && OBJS[g->tiles[IDX(xx, yy)].obj].structure) { nearStruct = true; break; }
            if (nearStruct) continue;
            startTask(e, TK_REPLANT, x, y);
            return;
        }
    }
    if (storageCount(I_WOOD) > 300) { startTask(e, TK_PATROL, (int)bx + rndi(-6, 6), (int)by + rndi(-6, 6)); return; }
    // fell trees, preferring those close to buildings (firebreak)
    int best = -1; float bs = 1e9f;
    int R = 30;
    for (int y = (int)by - R; y <= (int)by + R; y++)
        for (int x = (int)bx - R; x <= (int)bx + R; x++) {
            if (!inb(x, y)) continue;
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (!isTree(o) && o != O_STUMP) continue;
            if (isFruitTree(o) || (g->tiles[IDX(x, y)].var & VAR_GUARD)) continue;   // orchards and protected trees stay
            if (someoneTargets(e, TK_CHOP, x, y)) continue;
            float d = dist(e.x, e.y, x, y);
            bool nearStruct = false;
            for (int yy = y - 4; yy <= y + 4 && !nearStruct; yy++)
                for (int xx = x - 4; xx <= x + 4; xx++)
                    if (inb(xx, yy) && g->tiles[IDX(xx, yy)].obj && OBJS[g->tiles[IDX(xx, yy)].obj].structure) { nearStruct = true; break; }
            float sc = d - (nearStruct ? 25 : 0) + (o == O_STUMP ? 5 : 0);
            if (sc < bs) { bs = sc; best = IDX(x, y); }
        }
    if (best >= 0) startTask(e, TK_CHOP, best % WW, best / WW);
}

static void planHerder(Ent& e, float bx, float by) {
    if (e.carry.id) { startTask(e, TK_DELIVER, 0, 0); return; }
    // feed troughs
    for (size_t i = 0; i < g->conts.size(); i++) {
        Container& c = g->conts[i];
        if (!c.alive || c.kind != C_TROUGH) continue;
        int feed = 0;
        for (auto& s : c.slots) if (s.id) feed += s.n;
        if (feed < 15 && (storageCount(I_HAY) > 0 || storageCount(I_WHEAT) > 4) && !someoneTargets(e, TK_FILLTROUGH, c.x, c.y)) { startTask(e, TK_FILLTROUGH, c.x, c.y); return; }
    }
    // shear and milk
    bool shears = storageCount(I_SHEARS) > 0;
    for (auto& a : g->ents) {
        if (!a.alive || !a.tame || dist(a.x, a.y, bx, by) > 30 || a.produce < 1) continue;
        if (a.kind == E_SHEEP && shears && !someoneTargets(e, TK_SHEAR, a.id, 0)) { startTask(e, TK_SHEAR, a.id, 0); return; }
        if (a.kind == E_BUFFALO && !someoneTargets(e, TK_MILK, a.id, 0)) { startTask(e, TK_MILK, a.id, 0); return; }
    }
    // take honey from full hives
    for (int y = (int)by - 26; y <= (int)by + 26; y++)
        for (int x = (int)bx - 26; x <= (int)bx + 26; x++)
            if (inb(x, y) && g->tiles[IDX(x, y)].obj == O_BEEHIVE && g->tiles[IDX(x, y)].timer >= 2 && !someoneTargets(e, TK_HONEY, x, y)) { startTask(e, TK_HONEY, x, y); return; }
    // collect eggs and manure
    int best = -1; float bd = 1e9f;
    for (int y = (int)by - 26; y <= (int)by + 26; y++)
        for (int x = (int)bx - 26; x <= (int)bx + 26; x++) {
            if (!inb(x, y)) continue;
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (o != O_EGG && o != O_MANURE) continue;
            if (someoneTargets(e, TK_COLLECT, x, y)) continue;
            float d = dist(e.x, e.y, x, y);
            if (d < bd) { bd = d; best = IDX(x, y); }
        }
    if (best >= 0) { startTask(e, TK_COLLECT, best % WW, best / WW); return; }
    // make hay for winter
    if (g->season() != 3 && storageCount(I_HAY) < 80) {
        int hb = -1; float hd = 1e9f;
        for (int y = (int)by - 22; y <= (int)by + 22; y++)
            for (int x = (int)bx - 22; x <= (int)bx + 22; x++)
                if (inb(x, y) && g->tiles[IDX(x, y)].obj == O_TALLGRASS && !someoneTargets(e, TK_CUTHAY, x, y)) {
                    float d = dist(e.x, e.y, x, y);
                    if (d < hd) { hd = d; hb = IDX(x, y); }
                }
        if (hb >= 0) { startTask(e, TK_CUTHAY, hb % WW, hb / WW); return; }
    }
}

static void planCook(Ent& e, float bx, float by) {
    if (e.carry.id) { startTask(e, TK_DELIVER, 0, 0); return; }
    int fx, fy;
    if (!nearObj(bx, by, O_CAMPFIRE, 30, &fx, &fy)) return;
    if (storageCount(I_RAWMEAT) > 0 || storageCount(I_FISH) > 0 || storageCount(I_WHEAT) >= 3 + 20 ||
        (storageCount(I_POTATO) >= 2 && storageCount(I_CABBAGE) >= 1 && storageCount(I_BEANS) >= 2))
        startTask(e, TK_COOK, fx, fy);
}

static void planRepair(Ent& e, float bx, float by) {
    int best = -1; float bd = 1e9f;
    for (int y = (int)by - 40; y <= (int)by + 40; y++)
        for (int x = (int)bx - 40; x <= (int)bx + 40; x++) {
            if (!inb(x, y) || !lineBroken(g->tiles[IDX(x, y)]) || someoneTargets(e, TK_REPAIR, x, y)) continue;
            float d = dist(e.x, e.y, x, y);
            if (d < bd) { bd = d; best = IDX(x, y); }
        }
    if (best >= 0) startTask(e, TK_REPAIR, best % WW, best / WW);
    else startTask(e, TK_PATROL, (int)bx + rndi(-12, 12), (int)by + rndi(-12, 12));   // walk the lines
}

static void planMiner(Ent& e, float bx, float by) {
    if (e.carry.id) { startTask(e, TK_DELIVER, 0, 0); return; }
    int mx, my;
    if (nearObj(bx, by, O_MINE, 40, &mx, &my)) startTask(e, TK_MINE, mx, my);
}

static void planFisher(Ent& e, float bx, float by) {
    if (e.carry.id) { startTask(e, TK_DELIVER, 0, 0); return; }
    if (storageCount(I_FISH) + storageCount(I_COOKEDFISH) >= 40) { startTask(e, TK_PATROL, (int)bx + rndi(-5, 5), (int)by + rndi(-5, 5)); return; }   // plenty for now
    // a quiet bank spot near the farm
    float bd = 1e9f; int fx = -1, fy = -1;
    for (int y = (int)by - 30; y <= (int)by + 30; y++)
        for (int x = (int)bx - 30; x <= (int)bx + 30; x++) {
            if (!inb(x, y) || isWater(x, y) || solidFor(x, y, 2) || unreachable(x, y)) continue;
            if (!(isWater(x + 1, y) || isWater(x - 1, y) || isWater(x, y + 1) || isWater(x, y - 1))) continue;
            float d = dist(e.x, e.y, x, y) + rndr(0, 6);
            if (d < bd) { bd = d; fx = x; fy = y; }
        }
    if (fx >= 0) startTask(e, TK_FISH, fx, fy);
}

static void planGuard(Ent& e, float bx, float by) {
    startTask(e, TK_PATROL, (int)bx + rndi(-10, 10), (int)by + rndi(-10, 10));
}

static void finishTask(Ent& e) { e.task = TK_NONE; e.path.clear(); }

static bool deliverCarry(Ent& e, float dt) {
    if (!e.carry.id) return true;
    int dest = -1;
    bool toCompost = e.carry.id == I_MANURE || e.carry.id == I_LEAVES || e.carry.id == I_MOLDY;
    float bd = 1e9f;
    for (size_t i = 0; i < g->conts.size(); i++) {
        Container& c = g->conts[i];
        if (!c.alive) continue;
        bool ok = toCompost ? (c.kind == C_COMPOST || c.kind == C_BIOGAS) : (c.kind == C_CHEST || c.kind == C_GRANARY || c.kind == C_CELLAR || c.kind == C_FRIDGE);
        if (!ok) continue;
        bool space = false;
        for (auto& s : c.slots) if (!s.id || (s.id == e.carry.id && s.n < ITEMS[s.id].stack)) space = true;
        if (!space) continue;
        float d = dist(e.x, e.y, c.x, c.y);
        if (!toCompost && isFood(e.carry.id)) d -= (c.kind == C_FRIDGE && poweredAt(c.x, c.y)) ? 30 : (c.kind == C_CELLAR || c.kind == C_GRANARY ? 20 : 0);
        if (d < bd) { bd = d; dest = (int)i; }
    }
    if (dest < 0 && toCompost) {  // no compost bin, use any storage
        storeAnywhere(e.carry.id, e.carry.n, e.carry.fresh, e.x, e.y);
        e.carry = Stack{};
        return true;
    }
    if (dest < 0) {
        // storage is full: warn the player and set it down so other work isn't blocked
        static float lastWarn = -1000;
        if (g->totalTime - lastWarn > 60 || g->totalTime < lastWarn) {
            lastWarn = g->totalTime;
            logMsg(std::string("Storage is full! ") + e.name + " had to leave " + ITEMS[e.carry.id].name + " on the ground. Build more chests.", ORANGE);
        }
        e.carry = Stack{};
        return true;
    }
    Container& c = g->conts[dest];
    int r = goTo(e, c.x, c.y, SPECIES[E_WORKER].speed, dt, 1.4f);
    if (r == -1) { finishTask(e); return true; }
    if (r == 1) {
        int left = addItem(c.slots.data(), (int)c.slots.size(), e.carry.id, e.carry.n, e.carry.fresh);
        if (left > 0) e.carry.n = (int16_t)left; else e.carry = Stack{};
        if (c.kind == C_GRANARY || c.kind == C_CELLAR) g->stats.usedGranary = true;
        for (auto& h : e.haul) storeAnywhere(h.id, h.n, h.fresh, e.x, e.y);
        e.haul.clear();
        return true;
    }
    return false;
}

static void runTask(Ent& e, float dt) {
    float rate = workRate(e);
    float spd = SPECIES[E_WORKER].speed * rate;
    float wdt = dt * rate;   // work done this tick
    int tx = e.state, ty = e.target;
    e.timer -= dt;
    if (e.timer <= 0) { finishTask(e); return; }
    auto at = [&](int x, int y, float r = 1.3f) {
        int res = goTo(e, x, y, spd, dt, r);
        if (res == -1) {
            // can't get there: take a breather instead of retrying every frame, and tell the player
            finishTask(e);
            e.blocked = 3.f;
            if (inb(x, y)) unreachableUntil[IDX(x, y)] = g->totalTime + 120.f;
            static float lastWarn = -1000;
            uint8_t o = inb(x, y) ? g->tiles[IDX(x, y)].obj : 0;
            bool useful = o && (OBJS[o].slots > 0 || o == O_BED || o == O_WELL || o == O_CAMPFIRE || o == O_MINE || o == O_BEEHIVE);
            if (useful && (g->totalTime - lastWarn > 90 || g->totalTime < lastWarn)) {   // only nag about buildings workers rely on
                lastWarn = g->totalTime;
                logMsg(e.name + " can't reach the " + OBJS[o].name + " - check doors and paths.", ORANGE);
            }
        }
        return res == 1;
    };
    auto fetch = [&](int id, int n) -> bool {  // stage helper: go to storage holding item and take it
        int ci = findContWith(id, e.x, e.y);
        if (ci < 0) { finishTask(e); return false; }
        Container& c = g->conts[ci];
        if (!at(c.x, c.y, 1.4f)) return false;
        int have = countItem(c.slots.data(), (int)c.slots.size(), id);
        n = std::min(n, have);
        float fr = 1;
        for (auto& s : c.slots) if (s.id == id) fr = s.fresh;
        takeItem(c.slots.data(), (int)c.slots.size(), id, n);
        e.carry = Stack{(uint8_t)id, (int16_t)n, fr};
        return true;
    };
    switch (e.task) {
        case TK_EAT: {
            int ci = -1;
            int food = bestFood(e.x, e.y, ci, &e);
            if (!food) { finishTask(e); return; }
            Container& c = g->conts[ci];
            if (!at(c.x, c.y, 1.4f)) return;
            if (takeItem(c.slots.data(), (int)c.slots.size(), food, 1)) {
                e.diet[e.dietI] = (uint8_t)food; e.dietI = (uint8_t)((e.dietI + 1) % 12);
                e.hunger = std::min(100.f, e.hunger + ITEMS[food].food * 2.2f);
                e.thirst = std::min(100.f, e.thirst + ITEMS[food].drink);
                g->stats.foodEaten += ITEMS[food].food;
                if (e.hunger < 60) return;  // keep eating
            }
            finishTask(e);
            return;
        }
        case TK_DRINK: {
            int wx, wy;
            if (!waterSource(e.x, e.y, wx, wy)) { finishTask(e); return; }
            if (at(wx, wy, 1.5f)) { e.thirst = 100; finishTask(e); }
            return;
        }
        case TK_SLEEP: {
            if (e.bed < 0 || g->tiles[e.bed].obj != O_BED) { e.bed = freeBed(); if (e.bed < 0) { finishTask(e); return; } }
            if (!e.sleeping) {
                if (at(e.bed % WW, e.bed / WW, 0.6f)) { e.sleeping = true; e.x = e.bed % WW + 0.5f; e.y = e.bed / WW + 0.5f; }
            } else e.timer = 60;
            if (g->tod > 0.25f && g->tod < 0.8f) { e.sleeping = false; finishTask(e); }
            return;
        }
        case TK_DELIVER:
            if (deliverCarry(e, dt)) finishTask(e);
            return;
        case TK_HARVEST: {
            if (!g->crops[IDX(tx, ty)].type) { finishTask(e); return; }
            if (at(tx, ty)) {
                Crop& cc = g->crops[IDX(tx, ty)];
                if (cc.manure && !cc.dead) plowUnder(tx, ty);   // green manure goes back into the ground
                else { if (cc.dead) toCompost(I_MOLDY, 1, e.x, e.y); harvestCrop(tx, ty, true, e.x, e.y); }
                e.timer2 = 0; finishTask(e);
            }
            return;
        }
        case TK_WATER: {
            if (e.taskStage == 0) {
                int wx, wy;
                if (!waterSource(e.x, e.y, wx, wy)) { finishTask(e); return; }
                if (at(wx, wy, 1.5f)) { e.taskStage = 1; e.carry = Stack{I_WATERBUCKET, 4, 1}; }
                return;
            }
            if (at(tx, ty)) {
                for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) if (inb(tx + j, ty + k)) g->moist[IDX(tx + j, ty + k)] = 1;
                for (int n = 0; n < 8; n++) g->parts.push_back({tx + 0.5f, ty + 0.5f, rndr(-1.5f, 1.5f), rndr(-1.5f, 1), 0.4f, 0.4f, Color{120, 180, 255, 220}, 2, 3});
                if (--e.carry.n <= 0) { e.carry = Stack{}; finishTask(e); return; }
                // look for another dry crop nearby
                for (int y = ty - 6; y <= ty + 6; y++)
                    for (int x = tx - 6; x <= tx + 6; x++)
                        if (inb(x, y) && g->crops[IDX(x, y)].type && g->moist[IDX(x, y)] < 0.3f) { e.state = x; e.target = y; return; }
                e.carry = Stack{};
                finishTask(e);
            }
            return;
        }
        case TK_FERT: {
            if (e.taskStage == 0) {
                int id = storageCount(I_COMPOST) > 0 ? I_COMPOST : I_MANURE;
                if (fetch(id, 4)) e.taskStage = 1;
                return;
            }
            if (!e.carry.id) { finishTask(e); return; }
            if (at(tx, ty)) {
                // spread over poor tiles nearby
                for (int y = ty - 3; y <= ty + 3 && e.carry.n > 0; y++)
                    for (int x = tx - 3; x <= tx + 3 && e.carry.n > 0; x++) {
                        if (!inb(x, y)) continue;
                        int i = IDX(x, y);
                        if ((g->ground[i] == G_TILLED || g->ground[i] == G_GREENHOUSE) && soilMin(i) < 0.6f) {
                            float q = e.carry.fresh;
                            if (e.carry.id == I_COMPOST) addNutrients(i, 0.1f + 0.2f * q, 0.08f + 0.15f * q, 0.08f + 0.15f * q);
                            else addNutrients(i, 0.2f, 0.06f, 0.08f);
                            e.carry.n--;
                        }
                    }
                if (e.carry.n <= 0) e.carry = Stack{};
                finishTask(e);
            }
            return;
        }
        case TK_PLANT: {
            int i = IDX(tx, ty);
            if (g->crops[i].type || g->tiles[i].obj) { if (e.carry.id) startTask(e, TK_DELIVER, 0, 0); else finishTask(e); return; }
            if (e.taskStage == 0) {
                // crop rotation: depleted soil gets beans; otherwise the most plentiful seed
                // read the soil: nitrogen-poor gets legumes, phosphorus/potassium-poor gets buckwheat, both as green manure
                int choice = 0;
                bool manure = false;
                if (g->fert[i] < 0.35f) {
                    for (int id : {I_GREENBEANSEED, I_BEANS}) if (!choice && storageCount(id) > 0) { choice = id; manure = true; }
                } else if (g->phos[i] < 0.35f || g->pota[i] < 0.35f) {
                    if (storageCount(I_BUCKWHEAT) > 0) { choice = I_BUCKWHEAT; manure = true; }
                    else if (storageCount(I_MUSTARDSEED) > 0) { choice = I_MUSTARDSEED; manure = true; }
                }
                if (choice) {
                    int crop = 0;
                    for (int c = 1; c < CR_COUNT; c++) if (CROPS[c].seed == choice) crop = c;
                    if (!plantSeason(crop, i)) { choice = 0; manure = false; }
                }
                e.state2 = manure;
                if (!choice) {
                    int bestN = 0;
                    for (int c = 1; c < CR_COUNT; c++) {
                        if (c == CR_BUCKWHEAT || c == CR_MUSTARD) continue;
                        int n = storageCount(CROPS[c].seed);
                        if (n > bestN && plantSeason(c, i)) { bestN = n; choice = CROPS[c].seed; }
                    }
                }
                if (!choice) { finishTask(e); return; }
                if (fetch(choice, 1)) e.taskStage = 1;
                return;
            }
            if (!e.carry.id) { finishTask(e); return; }
            if (at(tx, ty)) {
                int crop = 0;
                for (int c = 1; c < CR_COUNT; c++) if (CROPS[c].seed == e.carry.id) crop = c;
                tillInLitter(i);   // the bed is worked before sowing
                if (crop) { g->crops[i] = Crop{}; g->crops[i].type = (uint8_t)crop; g->crops[i].manure = e.state2 != 0; }
                e.carry = Stack{};
                finishTask(e);
            }
            return;
        }
        case TK_TILL:
            if (at(tx, ty)) {
                int i = IDX(tx, ty);
                if (g->tiles[i].obj == O_TALLGRASS) removeObject(tx, ty, false);
                if (!g->tiles[i].obj) { g->ground[i] = G_TILLED; tillInLitter(i); }
                finishTask(e);
            }
            return;
        case TK_CHOP: {
            uint8_t o = g->tiles[IDX(tx, ty)].obj;
            if (!isTree(o) && o != O_STUMP) { finishTask(e); return; }
            if (!at(tx, ty)) return;
            e.timer2 += wdt;
            e.anim += dt * 6;
            if (rnd() < dt * 3) burst(tx + 0.5f, ty + 0.5f, Color{180, 140, 90, 255}, 2, 0.5f);
            if (e.timer2 > (o == O_STUMP ? 1.5f : 4.f)) {
                e.timer2 = 0;
                removeObject(tx, ty, false);
                if (o == O_STUMP) { e.carry = Stack{I_WOOD, 1, 1}; }
                else {
                    placeObject(tx, ty, O_STUMP);
                    e.carry = Stack{I_WOOD, (int16_t)rndi(4, 6), 1};
                    if (rnd() < 0.6f) storeAnywhere(saplingFor(o), 1, 1, e.x, e.y);
                    toCompost(I_LEAVES, 2, e.x, e.y);
                }
                startTask(e, TK_DELIVER, 0, 0);
            }
            return;
        }
        case TK_REPLANT:
            if (e.taskStage == 0) { if (fetch(I_SAPLING, 1)) e.taskStage = 1; return; }
            if (at(tx, ty)) {
                if (!g->tiles[IDX(tx, ty)].obj) placeObject(tx, ty, O_SAPLING);
                e.carry = Stack{};
                finishTask(e);
            }
            return;
        case TK_FILLTROUGH: {
            if (e.taskStage == 0) {
                int id = storageCount(I_HAY) > 0 ? I_HAY : I_WHEAT;
                if (fetch(id, 10)) e.taskStage = 1;
                return;
            }
            Tile& t = g->tiles[IDX(tx, ty)];
            if (t.obj != O_TROUGH || t.cont < 0) { startTask(e, TK_DELIVER, 0, 0); return; }
            if (at(tx, ty, 1.4f)) {
                Container& c = g->conts[t.cont];
                int left = addItem(c.slots.data(), (int)c.slots.size(), e.carry.id, e.carry.n, e.carry.fresh);
                if (left) { e.carry.n = (int16_t)left; startTask(e, TK_DELIVER, 0, 0); }
                else { e.carry = Stack{}; finishTask(e); }
            }
            return;
        }
        case TK_SHEAR: case TK_MILK: {
            Ent* a = entById(tx);
            if (!a || a->produce < 1) { finishTask(e); return; }
            if (dist(e.x, e.y, a->x, a->y) > 1.2f) { chase(e, a->x, a->y, spd, dt, false); return; }
            a->produce = 0;
            if (e.task == TK_SHEAR) e.carry = Stack{I_WOOL, 3, 1};
            else { e.carry = Stack{I_MILK, 2, 1}; g->stats.foodMade += ITEMS[I_MILK].food * 2; }
            startTask(e, TK_DELIVER, 0, 0);
            return;
        }
        case TK_COLLECT: {
            uint8_t o = g->tiles[IDX(tx, ty)].obj;
            if (o != O_EGG && o != O_MANURE) { finishTask(e); return; }
            if (at(tx, ty, 0.9f)) {
                int id = o == O_EGG ? I_EGG : I_MANURE;
                removeObject(tx, ty, false);
                if (id == I_EGG) g->stats.foodMade += ITEMS[I_EGG].food;
                if (e.carry.id == id && e.carry.n < 20) e.carry.n++;
                else if (!e.carry.id) e.carry = Stack{(uint8_t)id, 1, 1};
                // keep collecting nearby items of the same kind
                for (int y = ty - 5; y <= ty + 5; y++)
                    for (int x = tx - 5; x <= tx + 5; x++)
                        if (inb(x, y) && g->tiles[IDX(x, y)].obj == o && e.carry.n < 20) { e.state = x; e.target = y; e.timer = 60; return; }
                startTask(e, TK_DELIVER, 0, 0);
            }
            return;
        }
        case TK_CUTHAY:
            if (g->tiles[IDX(tx, ty)].obj != O_TALLGRASS) { finishTask(e); return; }
            if (at(tx, ty, 1.0f)) {
                removeObject(tx, ty, false);
                if (e.carry.id == I_HAY) e.carry.n += 2; else e.carry = Stack{I_HAY, 2, 1};
                if (e.carry.n >= 16) { startTask(e, TK_DELIVER, 0, 0); return; }
                for (int y = ty - 5; y <= ty + 5; y++)
                    for (int x = tx - 5; x <= tx + 5; x++)
                        if (inb(x, y) && g->tiles[IDX(x, y)].obj == O_TALLGRASS) { e.state = x; e.target = y; e.timer = 60; return; }
                startTask(e, TK_DELIVER, 0, 0);
            }
            return;
        case TK_COOK: {
            if (e.taskStage == 0) {
                // pick a recipe: meat first, then stew, then bread
                int id = 0, n = 0;
                if (storageCount(I_RAWMEAT) > 0) { id = I_RAWMEAT; n = std::min(5, storageCount(I_RAWMEAT)); }
                else if (storageCount(I_FISH) > 0) { id = I_FISH; n = std::min(5, storageCount(I_FISH)); }
                else if (storageCount(I_POTATO) >= 2 && storageCount(I_CABBAGE) >= 1 && storageCount(I_BEANS) >= 2) {
                    takeFromStorage(I_POTATO, 2, e.x, e.y); takeFromStorage(I_CABBAGE, 1, e.x, e.y); takeFromStorage(I_BEANS, 2, e.x, e.y);
                    e.carry = Stack{I_STEW, 0, 1}; e.taskStage = 1; return;
                } else if (storageCount(I_WHEAT) >= 23) { id = I_WHEAT; n = 9; }
                if (!id) { finishTask(e); return; }
                if (fetch(id, n)) e.taskStage = 1;
                return;
            }
            if (!at(tx, ty, 1.5f)) return;
            e.timer2 += wdt;
            if (e.timer2 < 3.f) return;
            e.timer2 = 0;
            if (e.carry.id == I_RAWMEAT) e.carry = Stack{I_COOKEDMEAT, e.carry.n, 1};
            else if (e.carry.id == I_FISH) e.carry = Stack{I_COOKEDFISH, e.carry.n, 1};
            else if (e.carry.id == I_WHEAT) e.carry = Stack{I_BREAD, (int16_t)(e.carry.n / 3), 1};
            else if (e.carry.id == I_STEW) e.carry = Stack{I_STEW, 1, 1};
            g->stats.foodMade += ITEMS[e.carry.id].food * e.carry.n * 0.3f;
            startTask(e, TK_DELIVER, 0, 0);
            return;
        }
        case TK_REPAIR:
            if (!lineBroken(g->tiles[IDX(tx, ty)])) { finishTask(e); return; }
            if (!at(tx, ty, 1.1f)) return;
            e.timer2 += wdt;
            e.anim += dt * 4;
            if (e.timer2 > 3.f) { e.timer2 = 0; repairLine(tx, ty); finishTask(e); }
            return;
        case TK_HONEY:
            if (g->tiles[IDX(tx, ty)].obj != O_BEEHIVE) { finishTask(e); return; }
            if (at(tx, ty, 1.4f)) {
                int n = collectHoney(tx, ty);
                if (n) { e.carry = Stack{I_HONEY, (int16_t)n, 1}; g->stats.foodMade += ITEMS[I_HONEY].food * n; startTask(e, TK_DELIVER, 0, 0); }
                else finishTask(e);
            }
            return;
        case TK_PICK:
            if (!isFruitPlant(g->tiles[IDX(tx, ty)].obj) || g->fruitState[IDX(tx, ty)] != 1) { finishTask(e); return; }
            if (!at(tx, ty, 1.4f)) return;
            e.timer2 += wdt;
            if (e.timer2 > 2.f) { e.timer2 = 0; harvestFruit(tx, ty, true, e.x, e.y); finishTask(e); }
            return;
        case TK_MINE: {
            if (g->tiles[IDX(tx, ty)].obj != O_MINE) { e.hidden = false; finishTask(e); return; }
            if (!e.hidden) {
                if (!at(tx, ty, 1.2f)) return;
                e.hidden = true; e.x = tx + 0.5f; e.y = ty + 0.9f; e.timer2 = 0; e.timer = 400;
                return;
            }
            e.timer2 += wdt;
            if (e.timer2 < 50.f) return;   // a shift underground
            e.timer2 = 0; e.hidden = false;
            for (int k = 0; k < 8; k++) {   // climb out onto a free tile beside the entrance
                int ox = tx + (k % 3) - 1, oy = ty + 1 - (k / 3);
                if (!solidFor(ox, oy, 2) && !isWater(ox, oy)) { e.x = ox + 0.5f; e.y = oy + 0.5f; break; }
            }
            bool rocky = g->ground[IDX(tx, ty)] == G_STONE;
            float m = rocky ? 1.f : 0.4f;
            e.haul.clear();
            e.carry = storageCount(I_STONE) < 150 ? Stack{I_STONE, (int16_t)rndi(4, 8), 1} : Stack{};   // enough stone: only bring ore
            auto find = [&](int id, float chance, int lo, int hi) { if (rnd() < chance * m) e.haul.push_back(Stack{(uint8_t)id, (int16_t)rndi(lo, hi), 1}); };
            find(I_IRONORE, 0.7f, 1, 3); find(I_ORE, 0.5f, 1, 2); find(I_QUARTZ, 0.3f, 1, 2); find(I_FLINT, 0.4f, 1, 2); find(I_CLAY, 0.3f, 1, 3);
            floatText(e.x, e.y - 1, "Ore!", Color{230, 200, 150, 255});
            if (!e.carry.id && !e.haul.empty()) { e.carry = e.haul.back(); e.haul.pop_back(); }
            startTask(e, TK_DELIVER, 0, 0);
            return;
        }
        case TK_FISH: {
            if (!at(tx, ty, 0.8f)) return;
            e.timer2 += wdt;
            e.face = isWater(tx + 1, ty) ? 1.f : -1.f;
            float catchTime = (g->season() == 3 ? 40.f : 25.f) * (g->tod > 0.75f || g->tod < 0.3f ? 0.7f : 1.f);
            if (e.timer2 < catchTime) return;
            e.timer2 = 0;
            if (e.carry.id == I_FISH) e.carry.n++; else e.carry = Stack{I_FISH, 1, 1};
            g->stats.foodMade += ITEMS[I_FISH].food;
            if (e.carry.n >= 4) startTask(e, TK_DELIVER, 0, 0);
            return;
        }
        case TK_PATROL:
        case TK_GOHOME:
            if (at(tx, ty, 1.5f)) { if (e.carry.id) startTask(e, TK_DELIVER, 0, 0); else finishTask(e); e.timer2 = rndr(1, 4); }
            return;
        default: finishTask(e);
    }
}

static void updateWorker(Ent& e, float dt) {
    float bx, by;
    bool hasBase = baseCentre(bx, by);
    if (!hasBase) { bx = g->pl.x; by = g->pl.y; }
    e.hunger = std::max(0.f, e.hunger - dt * 100.f / (DAY_SECONDS * 1.3f) * (e.sleeping ? 0.5f : 1));
    e.thirst = std::max(0.f, e.thirst - dt * 100.f / (DAY_SECONDS * 1.0f) * (e.sleeping ? 0.5f : 1));
    if (e.hunger <= 0) e.hp -= dt * 0.15f;
    if (e.thirst <= 0) e.hp -= dt * 0.25f;
    int px = (int)e.x, py = (int)e.y;
    float feels = tempAt(px, py) + 12;  // workers come with basic clothing
    if (feels < 0) e.hp -= dt * 0.1f * -feels / 5;
    if (e.hp <= 0) { e.alive = false; logMsg(e.name + " died. Workers need food, water, a bed and warmth.", RED); return; }
    if (e.hp < e.maxhp && e.hunger > 40) e.hp = std::min(e.maxhp, e.hp + dt * 0.2f);
    if (e.bed >= 0 && (g->tiles[e.bed].obj != O_BED || !g->indoor[e.bed])) e.bed = -1;
    if (e.bed < 0) e.bed = freeBed();

    // daily mood check at dawn
    if (g->tod > 0.27f && g->tod < 0.3f && e.produce != (float)g->day) {
        e.produce = (float)g->day;
        std::string why;
        if (e.bed < 0) { e.mood -= 34; why = "no bed indoors"; }
        if (e.hunger < 15) { e.mood -= 34; why += why.empty() ? "hungry" : ", hungry"; }
        if (e.thirst < 15) { e.mood -= 34; why += why.empty() ? "thirsty" : ", thirsty"; }
        e.mood = clampf(e.mood + clampf(farmBeauty() / 10.f, -3, 6), 0, 100);   // a lovely farm is a happy farm
        e.mood = clampf(e.mood + (dietVariety(e) - 3) * 2.5f, 0, 100);          // the same meal every day wears thin
        if (why.empty()) e.mood = std::min(100.f, e.mood + 20);
        else logMsg(e.name + " is unhappy: " + why + ".", ORANGE);
        if (e.mood <= 0) {
            logMsg(e.name + " has left your farm (" + why + ").", RED);
            if (e.carry.id) storeAnywhere(e.carry.id, e.carry.n, e.carry.fresh, e.x, e.y);
            e.alive = false;
            return;
        }
    }

    // threats: guards hunt; everyone else shelters at their bed from raiders and defends if cornered
    if (e.job != J_GUARD && e.bed >= 0) {
        Ent* raider = nearestEnt(e.x, e.y, 9.f, [](const Ent& o) { return o.kind == E_RAIDER && o.state >= 1; });
        if (raider && dist(e.x, e.y, raider->x, raider->y) > 1.2f) {
            if (e.task != TK_SLEEP) { startTask(e, TK_SLEEP, 0, 0); }
            int r = goTo(e, e.bed % WW, e.bed / WW, SPECIES[E_WORKER].speed * 1.2f, dt, 0.6f);
            if (r == -1) flee(e, raider->x, raider->y, SPECIES[E_WORKER].speed, dt);
            return;
        }
    }
    Ent* threat = nearestEnt(e.x, e.y, e.job == J_GUARD ? 14.f : 2.5f, isEnemyOfFarm);
    if (threat) {
        e.sleeping = false;
        if (dist(e.x, e.y, threat->x, threat->y) < 1.0f) {
            e.timer2 -= dt;
            if (e.timer2 <= 0) {
                e.timer2 = 0.9f;
                damageEnt(*threat, e.job == J_GUARD ? 15 : 7, e.x, e.y);
                if (threat->kind == E_PORCUPINE && e.job != J_GUARD) damageEnt(e, 3, threat->x, threat->y);
            }
        } else chase(e, threat->x, threat->y, SPECIES[E_WORKER].speed * 1.1f, dt, false);
        return;
    }

    if (e.hidden && e.task == TK_MINE) { runTask(e, dt); return; }
    // walk to the farm after being recruited
    if (dist(e.x, e.y, bx, by) > 40 && e.task != TK_GOHOME && e.blocked <= 0) {
        if (e.age > 60.f) {   // stranded with no path home: they find their own way back
            e.x = bx + 0.5f; e.y = by + 0.5f; e.age = 0; e.path.clear();
            logMsg(e.name + " found their way back to the farm.", LIGHTGRAY);
        } else { startTask(e, TK_GOHOME, (int)bx, (int)by); e.timer = 300; }
    }
    if (dist(e.x, e.y, bx, by) > 40 && e.blocked > 0) e.age += dt;
    else if (dist(e.x, e.y, bx, by) <= 40) e.age = 0;

    bool night = g->tod > 0.84f || g->tod < 0.24f;
    if (e.blocked > 0) e.blocked -= dt;
    if (e.task == TK_NONE || (e.task != TK_SLEEP && night && e.task != TK_GOHOME && !e.carry.id)) {
        if (night && e.bed >= 0) startTask(e, TK_SLEEP, 0, 0);
        else if (e.thirst < 40) startTask(e, TK_DRINK, 0, 0);
        else if (e.hunger < 40) { int ci; if (bestFood(e.x, e.y, ci)) startTask(e, TK_EAT, 0, 0); }
        if (e.task == TK_NONE && e.blocked <= 0) {
            if (e.carry.id) startTask(e, TK_DELIVER, 0, 0);
            else if (!night) {
                switch (e.job) {
                    case J_FARMER: planFarmer(e, bx, by); break;
                    case J_WOODCUTTER: planWoodcutter(e, bx, by); break;
                    case J_HERDER: planHerder(e, bx, by); break;
                    case J_COOK: planCook(e, bx, by); break;
                    case J_GUARD: planGuard(e, bx, by); break;
                    case J_REPAIR: planRepair(e, bx, by); break;
                    case J_MINER: planMiner(e, bx, by); break;
                    case J_FISHER: planFisher(e, bx, by); break;
                    default: break;
                }
            }
        }
        if (e.task == TK_NONE) { wander(e, dt, 4, 1.2f); e.homeX = bx; e.homeY = by; return; }
    }
    runTask(e, dt);
}

static void updateVillager(Ent& e, float dt) {
    if (e.state == 1) { e.face = g->pl.x > e.x ? 1.f : -1.f; return; }  // trader stands still
    wander(e, dt, 5, 1.0f);
}

static void updateRaider(Ent& e, float dt) {
    float spd = SPECIES[E_RAIDER].speed;
    Player& p = g->pl;
    if (e.state == 0) { wander(e, dt, 5, 1.0f); return; }
    if (e.hp < e.maxhp * 0.35f && e.state != 3) { e.state = 3; floatText(e.x, e.y - 1, "Retreat!", ORANGE); }
    e.age += dt;
    if (e.age > DAY_SECONDS * 0.5f && e.state != 3) e.state = 3;
    if (e.state == 3) {
        if (dist(e.x, e.y, p.x, p.y) > 25 && e.age > DAY_SECONDS * 0.25f) { e.alive = false; return; }
        int r = goTo(e, (int)e.homeX, (int)e.homeY, spd, dt, 2.f);
        if (r == 1 || r == -1) {
            if (r == -1) stepToward(e, e.homeX, e.homeY, spd, dt);
            if (dist(e.x, e.y, e.homeX, e.homeY) < 3) e.alive = false;
        }
        return;
    }
    float bx, by;
    baseCentre(bx, by);
    if (e.state == 1) {
        if (dist(e.x, e.y, bx, by) < 8) e.state = 2;
        else { int r = goTo(e, (int)bx, (int)by, spd, dt, 6.f); if (r == -1) chase(e, bx, by, spd, dt, true); }
        return;
    }
    // state 2: raiding
    float pd = dist(e.x, e.y, p.x, p.y);
    // raiders want loot: they only fight whoever gets in their way
    Ent* foe = nearestEnt(e.x, e.y, 2.5f, isFarmPerson);
    if (pd < 2.5f && (!foe || pd < dist(e.x, e.y, foe->x, foe->y))) {
        if (pd < 1.0f) melee(e, nullptr, true, dt); else chase(e, p.x, p.y, spd, dt, false);
        return;
    }
    if (foe) {
        if (dist(e.x, e.y, foe->x, foe->y) < 1.0f) melee(e, foe, false, dt); else chase(e, foe->x, foe->y, spd, dt, false);
        return;
    }
    // loot food
    if (e.task == 0) {
        int best = -1; float bd = 30;
        for (size_t i = 0; i < g->conts.size(); i++) {
            Container& c = g->conts[i];
            if (!c.alive || !(c.kind == C_CHEST || c.kind == C_GRANARY || c.kind == C_CELLAR || c.kind == C_FRIDGE)) continue;
            bool food = false;
            for (auto& s : c.slots) if (s.id && isFood(s.id)) food = true;
            if (!food) continue;
            float d = dist(e.x, e.y, c.x, c.y);
            if (d < bd) { bd = d; best = (int)i; }
        }
        if (best >= 0) { e.task = 1; e.tx = g->conts[best].x; e.ty = g->conts[best].y; e.taskStage = best; e.path.clear(); }
        else e.task = 2;
    }
    if (e.task == 1) {
        Container& c = g->conts[e.taskStage];
        if (!c.alive) { e.task = 0; return; }
        int r = goTo(e, c.x, c.y, spd, dt, 1.4f);
        if (r == -1) chase(e, c.x + 0.5f, c.y + 0.5f, spd, dt, true);
        if (r == 1) {
            for (auto& s : c.slots)
                if (s.id && isFood(s.id) && !e.carry.id) { e.carry = s; s = Stack{}; }
            logMsg(std::string("Raiders looted food from your ") + OBJS[g->tiles[IDX(c.x, c.y)].obj].name + "!", Color{255, 120, 90, 255});
            if (e.torch) startFire(c.x, c.y);
            e.state = 3;
        }
        return;
    }
    // burn or kill livestock
    if (e.torch) {
        int fx, fy;
        for (int R = 3; R <= 12; R += 3)
            for (int y = (int)e.y - R; y <= (int)e.y + R; y++)
                for (int x = (int)e.x - R; x <= (int)e.x + R; x++) {
                    if (!inb(x, y)) continue;
                    uint8_t o = g->tiles[IDX(x, y)].obj;
                    if (o && OBJS[o].structure && OBJS[o].flammable && g->fire[IDX(x, y)] <= 0) {
                        fx = x; fy = y;
                        if (dist(e.x, e.y, fx + 0.5f, fy + 0.5f) < 1.6f) { startFire(fx, fy); e.torch = false; e.state = 3; return; }
                        chase(e, fx + 0.5f, fy + 0.5f, spd, dt, true);
                        return;
                    }
                }
    }
    Ent* prey = nearestEnt(e.x, e.y, 20, isLivestock);
    if (prey) {
        if (dist(e.x, e.y, prey->x, prey->y) < 1.0f) { melee(e, prey, false, dt); if (!prey->alive) e.state = 3; }
        else chase(e, prey->x, prey->y, spd, dt, true);
        return;
    }
    e.state = 3;
}

void updateEnts(float dt) {
    breeding();
    for (size_t i = 0; i < g->ents.size(); i++) {
        Ent& e = g->ents[i];
        if (!e.alive) continue;
        e.hurtFlash -= dt;
        // fire hurts everything
        int tx = (int)e.x, ty = (int)e.y;
        if (inb(tx, ty) && g->fire[IDX(tx, ty)] > 0.3f) { e.hp -= dt * 6; if (e.hp <= 0) { e.alive = false; if (e.tame) logMsg(std::string("Your ") + SPECIES[e.kind].name + " burned to death.", ORANGE); continue; } }
        switch (e.kind) {
            case E_WORKER: updateWorker(e, dt); break;
            case E_VILLAGER: updateVillager(e, dt); break;
            case E_RAIDER: updateRaider(e, dt); break;
            default: updateAnimal(e, dt); break;
        }
        // keep inside the map
        e.x = clampf(e.x, 1, WW - 1.01f); e.y = clampf(e.y, 1, WH - 1.01f);
    }
    // raid over?
    static bool raidActive = false;
    bool any = false;
    for (auto& e : g->ents) if (e.alive && e.kind == E_RAIDER && e.state >= 1) { any = true; break; }
    if (raidActive && !any) logMsg("The raid is over.", Color{200, 255, 200, 255});
    raidActive = any;
}
