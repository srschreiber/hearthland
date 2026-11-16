// World simulation: time, seasons, weather, temperature, farming, pests, spoilage,
// fire, power, natural disasters, tribes and goals.
#include "game.h"
#include <algorithm>
#include <functional>

// ---------------------------------------------------------------- log & fx
bool g_echoLog = false;
void logMsg(const std::string& s, Color c) {
    if (g_echoLog) printf("  [d%d %02d:%02d] %s\n", g->day, (int)(g->tod * 24), (int)(g->tod * 1440) % 60, s.c_str());
    g->log.push_front({s, c, 0});
    while (g->log.size() > 8) g->log.pop_back();
}
void floatText(float x, float y, const std::string& s, Color c) { g->floats.push_back({x, y, 1.6f, s, c}); }
void burst(float x, float y, Color c, int n, float spd) {
    for (int i = 0; i < n && g->parts.size() < 3000; i++) {
        float a = rndr(0, 6.283f), v = rndr(0.5f, 2.5f) * spd;
        g->parts.push_back({x, y, std::cos(a) * v, std::sin(a) * v - 1.f, rndr(0.3f, 0.8f), 0.8f, c, rndr(1, 2.5f), 0});
    }
}

// ---------------------------------------------------------------- inventory helpers
bool isFood(int id) { return ITEMS[id].food > 0; }

int addItem(Stack* slots, int n, int id, int count, float fresh) {
    if (id <= 0 || count <= 0) return 0;
    int maxS = ITEMS[id].stack;
    for (int i = 0; i < n && count > 0; i++) {
        Stack& s = slots[i];
        if (s.id == id && s.n < maxS) {
            int put = std::min(count, maxS - s.n);
            s.fresh = (s.fresh * s.n + fresh * put) / float(s.n + put);
            s.n += put; count -= put;
        }
    }
    for (int i = 0; i < n && count > 0; i++) {
        Stack& s = slots[i];
        if (s.id == 0 || s.n <= 0) {
            int put = std::min(count, maxS);
            s.id = (uint8_t)id; s.n = (int16_t)put; s.fresh = fresh; count -= put;
        }
    }
    return count;
}
int countItem(Stack* slots, int n, int id) {
    int c = 0;
    for (int i = 0; i < n; i++) if (slots[i].id == id) c += slots[i].n;
    return c;
}
bool takeItem(Stack* slots, int n, int id, int count) {
    if (countItem(slots, n, id) < count) return false;
    for (int i = n - 1; i >= 0 && count > 0; i--) {
        Stack& s = slots[i];
        if (s.id != id) continue;
        int t = std::min<int>(count, s.n);
        s.n -= t; count -= t;
        if (s.n <= 0) s = Stack{};
    }
    return true;
}

int addToPlayer(int id, int count, float fresh) {
    int left = addItem(g->pl.inv, INV_SLOTS, id, count, fresh);
    if (left > 0) {
        if (storeAnywhere(id, left, fresh, g->pl.x, g->pl.y)) {
            floatText(g->pl.x, g->pl.y - 1, "Stored in chest", LIGHTGRAY);
            return 0;
        }
        logMsg(std::string("Inventory full - lost ") + std::to_string(left) + " " + ITEMS[id].name, ORANGE);
    }
    return left;
}

static bool storageKind(uint8_t k) { return k == C_CHEST || k == C_GRANARY || k == C_CELLAR || k == C_FRIDGE; }

int storageCount(int id) {
    int c = 0;
    for (auto& ct : g->conts) if (ct.alive && storageKind(ct.kind)) c += countItem(ct.slots.data(), (int)ct.slots.size(), id);
    return c;
}

static float storePreference(uint8_t kind, int id) {
    bool food = ITEMS[id].spoilDays > 0;
    switch (kind) {
        case C_FRIDGE: return food ? 4.f : 0.2f;
        case C_CELLAR: return food ? 3.f : 0.5f;
        case C_GRANARY: return food ? 2.5f : 0.6f;
        case C_CHEST: return food ? 1.f : 2.f;
        default: return 0;
    }
}

bool storeAnywhere(int id, int count, float fresh, float nx, float ny) {
    while (count > 0) {
        int best = -1; float bestScore = -1;
        for (size_t i = 0; i < g->conts.size(); i++) {
            Container& c = g->conts[i];
            if (!c.alive || !storageKind(c.kind)) continue;
            bool space = false;
            for (auto& s : c.slots) if (!s.id || (s.id == id && s.n < ITEMS[id].stack)) { space = true; break; }
            if (!space) continue;
            float d = dist(nx, ny, c.x + 0.5f, c.y + 0.5f);
            if (d > 60) continue;
            float sc = storePreference(c.kind, id) * 100.f - d;
            if (sc > bestScore) { bestScore = sc; best = (int)i; }
        }
        if (best < 0) return false;
        Container& c = g->conts[best];
        if (c.kind == C_GRANARY || c.kind == C_CELLAR) g->stats.usedGranary = g->stats.usedGranary || isFood(id);
        int before = count;
        count = addItem(c.slots.data(), (int)c.slots.size(), id, count, fresh);
        if (count == before) return false;
    }
    return true;
}

bool takeFromStorage(int id, int count, float nx, float ny, float* freshOut) {
    if (storageCount(id) < count) return false;
    std::vector<std::pair<float, int>> order;
    for (size_t i = 0; i < g->conts.size(); i++) {
        Container& c = g->conts[i];
        if (c.alive && storageKind(c.kind) && countItem(c.slots.data(), (int)c.slots.size(), id) > 0)
            order.push_back({dist(nx, ny, c.x, c.y), (int)i});
    }
    std::sort(order.begin(), order.end());
    float fsum = 0; int taken = 0;
    for (auto& o : order) {
        Container& c = g->conts[o.second];
        for (auto& s : c.slots) {
            if (count <= 0) break;
            if (s.id != id) continue;
            int t = std::min<int>(count, s.n);
            fsum += s.fresh * t; taken += t;
            s.n -= t; count -= t;
            if (s.n <= 0) s = Stack{};
        }
    }
    if (freshOut) *freshOut = taken ? fsum / taken : 1.f;
    return true;
}

// ---------------------------------------------------------------- climate
static void sunTimes(float& rise, float& set) {
    // winter days are short, summer days long
    int s = g->season();
    rise = s == 3 ? 0.30f : (s == 1 ? 0.22f : 0.26f);
    set = s == 3 ? 0.74f : (s == 1 ? 0.84f : 0.79f);
}

float sunLevel() {
    float rise, set;
    sunTimes(rise, set);
    float t = g->tod;
    if (t < rise - 0.04f || t > set + 0.04f) return 0;
    if (t < rise + 0.04f) return (t - (rise - 0.04f)) / 0.08f;
    if (t > set - 0.04f) return 1 - (t - (set - 0.04f)) / 0.08f;
    return 1;
}

static float tempAtTime(int day, float tod, float offset) {
    (void)day;
    float t = 21.f;                                      // a mild, endless growing season
    t -= std::cos((tod - 0.12f) * 6.2832f) * 5.f;      // cool nights, warm afternoons
    return t + offset;
}

float airTemp() {
    float t = tempAtTime(g->day, g->tod, g->dayTempOffset);
    switch (g->weather) {
        case WX_CLOUDY: t -= 1; break;
        case WX_RAIN: t -= 3; break;
        case WX_STORM: t -= 4; break;
        case WX_SNOW: t -= 2; break;
        case WX_BLIZZARD: t -= 14; break;
        default: break;
    }
    if (g->heatwave) t += 11;
    return t;
}

float tempAt(int x, int y) {
    float t = airTemp();
    if (!inb(x, y)) return t;
    if (g->indoor[IDX(x, y)]) t += 7;
    if (g->ground[IDX(x, y)] == G_GREENHOUSE) t += 5;
    float bonus = 0;
    for (int yy = y - 5; yy <= y + 5; yy++)
        for (int xx = x - 5; xx <= x + 5; xx++) {
            if (!inb(xx, yy)) continue;
            uint8_t o = g->tiles[IDX(xx, yy)].obj;
            float d = dist(x, y, xx, yy);
            if (o == O_CAMPFIRE || o == O_BONFIRE) bonus = std::max(bonus, 20.f * clampf(1 - d / 4.5f, 0, 1));
            else if (o == O_HEATER && poweredAt(xx, yy)) bonus = std::max(bonus, (g->indoor[IDX(xx, yy)] ? 26.f : 14.f) * clampf(1 - d / 6.f, 0, 1));
            else if (o == O_KILN) bonus = std::max(bonus, 10.f * clampf(1 - d / 3.f, 0, 1));
        }
    if (g->fire[IDX(x, y)] > 0) bonus += 30;
    return t + bonus;
}

static bool raining() { return g->weather == WX_RAIN || g->weather == WX_STORM; }

static void pickWeather() {
    int s = g->season();
    float r = rnd();
    uint8_t w = WX_CLEAR;
    (void)s;
    w = r < 0.45f ? WX_CLEAR : r < 0.68f ? WX_CLOUDY : r < 0.89f ? WX_RAIN : r < 0.96f ? WX_STORM : WX_FOG;
    if (g->drought && (w == WX_RAIN || w == WX_CLOUDY) && rnd() < 0.8f) w = WX_CLEAR;
    if (g->heatwave && w != WX_STORM) w = WX_CLEAR;
    if (airTemp() < 0 && w == WX_RAIN) w = WX_SNOW;
    if (airTemp() > 3 && (w == WX_SNOW || w == WX_BLIZZARD)) w = WX_RAIN;
    if (w != g->weather) {
        if (w == WX_STORM) logMsg("Thunder rolls in the distance. A storm is coming.", Color{180, 180, 255, 255});
        if (w == WX_BLIZZARD) logMsg("A blizzard sweeps in! Get indoors or by a fire.", Color{200, 230, 255, 255});
    }
    g->weather = w;
    g->weatherTimer = rndr(70, 200);
    if (w == WX_BLIZZARD) g->weatherTimer = rndr(90, 150);
}

// ---------------------------------------------------------------- fire
void startFire(int x, int y) {
    if (!inb(x, y)) return;
    int i = IDX(x, y);
    if (g->fire[i] > 0) return;
    if (isWater(x, y)) return;
    g->fire[i] = 0.2f;
    g->fireTiles.push_back(i);
}

static bool tileFlammable(int i) {
    uint8_t o = g->tiles[i].obj;
    if (o && OBJS[o].flammable) return true;
    if (g->crops[i].type) return true;
    // green grass doesn't carry fire; parched grass in a drought and dry forest litter do
    uint8_t gr = g->ground[i];
    if (g->snowCover > 0.3f) return false;
    if (gr == G_GRASS) return g->drought && g->moist[i] < 0.2f;
    if (gr == G_FOREST) return g->moist[i] < 0.25f && g->season() == 1;
    return gr == G_FLOOR;
}

static void updateFire(float dt) {
    if (g->fireTiles.empty()) return;
    float dry = g->drought ? 2.2f : (g->season() == 1 ? 1.3f : 0.8f);
    if (raining()) dry *= 0.08f;
    if (g->weather == WX_SNOW || g->weather == WX_BLIZZARD) dry *= 0.05f;
    std::vector<int> next;
    std::vector<int> spawned;
    for (int i : g->fireTiles) {
        float& f = g->fire[i];
        if (f <= 0) continue;
        int x = i % WW, y = i / WW;
        Tile& t = g->tiles[i];
        bool fuel = tileFlammable(i);
        if (raining()) f -= 0.25f * dt;
        if (!fuel) f -= 0.35f * dt;
        else f = std::min(1.f, f + 0.25f * dt);
        if (fuel && f > 0.5f) {
            t.timer += dt;
            float burnTime = isTreeObj(t.obj) ? 9.f : (t.obj && OBJS[t.obj].structure ? 7.f : 3.f);
            if (t.timer > burnTime) {
                if (t.obj) {
                    bool st = OBJS[t.obj].structure;
                    if (st) logMsg(std::string("Fire destroyed your ") + OBJS[t.obj].name + "!", Color{255, 120, 80, 255});
                    removeObject(x, y, false);
                    placeObject(x, y, O_ASH);
                }
                if (g->crops[i].type) { g->crops[i] = Crop{}; g->stats.cropsLost++; }
                if (g->ground[i] == G_GRASS || g->ground[i] == G_FOREST || g->ground[i] == G_FLOOR) g->ground[i] = G_BURNT;
                g->moist[i] = 0.6f;
                t.timer = 0;
            }
            // spread
            if (rnd() < dt * 0.9f) {
                int nx = x + rndi(-1, 1), ny = y + rndi(-1, 1);
                if (inb(nx, ny)) {
                    int j = IDX(nx, ny);
                    uint8_t o = g->tiles[j].obj;
                    float chance = 0.25f * dry;
                    if (isTreeObj(o)) chance *= 1.6f;
                    if (o == O_PINE) chance *= 1.3f;
                    if (g->moist[j] > 0.6f) chance *= 0.3f;   // irrigated ground resists fire
                    if (g->fire[j] <= 0 && tileFlammable(j) && rnd() < chance) { g->fire[j] = 0.15f; spawned.push_back(j); }
                }
            }
        }
        if (rnd() < dt * 6) {
            g->parts.push_back({(float)x + rndr(0.2f, 0.8f), (float)y + rndr(0.3f, 0.9f), rndr(-0.2f, 0.2f), rndr(-1.5f, -0.7f), rndr(0.5f, 1.2f), 1.2f,
                                Color{255, (unsigned char)rndi(90, 200), 40, 255}, rndr(1.5f, 3), 1});
            if (rnd() < 0.4f)
                g->parts.push_back({x + 0.5f, (float)y, rndr(-0.3f, 0.3f), rndr(-1.2f, -0.6f), rndr(1.5f, 3.f), 3.f, Color{70, 70, 70, 140}, rndr(3, 6), 2});
        }
        if (f > 0) next.push_back(i);
        else f = 0;
    }
    for (int j : spawned) next.push_back(j);
    g->fireTiles.swap(next);
}

void warnStorageFull(const char* what) {
    static float last = -1000;
    if (g->totalTime - last < 60 && g->totalTime >= last) return;
    last = g->totalTime;
    logMsg(std::string("Storage is full - harvested ") + what + " is going to waste! Build more chests or a granary.", ORANGE);
}

// ---------------------------------------------------------------- crops
// ---------------------------------------------------------------- soil N/P/K
// What each crop takes out of the soil (N, P, K) per harvest. Legumes put nitrogen back.
static const float CROP_NPK[CR_COUNT][3] = {
    {0, 0, 0}, {0.2f, 0.1f, 0.08f}, {0.12f, 0.12f, 0.25f}, {0.2f, 0.08f, 0.12f}, {-0.3f, 0.05f, 0.05f},
    {0.15f, 0.15f, 0.2f}, {0.18f, 0.1f, 0.2f}, {-0.2f, 0.04f, 0.05f}, {0.05f, 0.02f, 0.02f}, {0.05f, 0.03f, 0.03f}};

void addNutrients(int i, float n, float p, float k) {
    g->fert[i] = clampf(g->fert[i] + n, 0.02f, 1.4f);
    g->phos[i] = clampf(g->phos[i] + p, 0.02f, 1.4f);
    g->pota[i] = clampf(g->pota[i] + k, 0.02f, 1.4f);
}

float soilMin(int i) { return std::min(g->fert[i], std::min(g->phos[i], g->pota[i])); }

// Liebig's law: yield is held back by whichever nutrient the crop needs that is scarcest.
float soilFactor(int i, int crop) {
    float f = 1.4f;
    const float* need = CROP_NPK[crop];
    if (need[0] > 0.04f) f = std::min(f, g->fert[i]);
    if (need[1] > 0.04f) f = std::min(f, g->phos[i]);
    if (need[2] > 0.04f) f = std::min(f, g->pota[i]);
    return f;
}

std::string soilAdvice(int i) {
    float n = g->fert[i], p = g->phos[i], k = g->pota[i];
    std::string s = TextFormat("Soil N %d%%  P %d%%  K %d%%", (int)(n * 100), (int)(p * 100), (int)(k * 100));
    if (n < 0.35f) s += "\nLow nitrogen: grow beans and hoe them under, or add manure/compost";
    if (p < 0.35f) s += "\nLow phosphorus: grow buckwheat and hoe it under, or add compost";
    if (k < 0.35f) s += "\nLow potassium: buckwheat, wood ash or compost";
    return s;
}

void tillInLitter(int i) {
    float l = g->litter[i];
    if (l <= 0.01f) return;
    addNutrients(i, 0.5f * l, 0.4f * l, 0.4f * l);   // poultry manure is rich in N, P and K
    g->litter[i] = 0;
}

// Hoe a standing crop into the soil. Green manures feed this tile and its neighbours; anything else
// adds a little organic matter and clears the plot.
void plowUnder(int x, int y) {
    int i = IDX(x, y);
    Crop& c = g->crops[i];
    if (!c.type) return;
    float m = c.dead ? 0.3f : clampf(c.growth, 0.2f, 1.f);
    float n = 0.04f, p = 0.02f, k = 0.02f, spread = 0;
    bool green = !c.dead;
    switch (c.type) {
        case CR_BEANS: case CR_GREENBEAN: n = 0.3f; p = 0.03f; k = 0.05f; spread = 0.5f; break;      // fixed nitrogen
        case CR_BUCKWHEAT: n = 0.05f; p = 0.28f; k = 0.22f; spread = 0.5f; break;                  // mined P and K
        case CR_MUSTARD: n = 0.12f; p = 0.1f; k = 0.1f; spread = 0.5f; break;                      // scavenged nutrients
        default: break;
    }
    if (!green) spread = 0;
    addNutrients(i, n * m, p * m, k * m);
    for (int yy = y - 1; yy <= y + 1; yy++)
        for (int xx = x - 1; xx <= x + 1; xx++) {
            if (!inb(xx, yy) || (xx == x && yy == y)) continue;
            int j = IDX(xx, yy);
            if (spread > 0 && (g->ground[j] == G_TILLED || g->ground[j] == G_GREENHOUSE)) addNutrients(j, n * m * spread, p * m * spread, k * m * spread);
            if (c.type == CR_MUSTARD && green) g->crops[j].pest = std::max(0.f, g->crops[j].pest - 0.3f);   // biofumigation
        }
    burst(x + 0.5f, y + 0.5f, Color{110, 80, 50, 255}, 8);
    c = Crop{};
}

void harvestCrop(int x, int y, bool byWorker, float wx, float wy) {
    int i = IDX(x, y);
    Crop& c = g->crops[i];
    if (!c.type) return;
    const CropDef& d = CROPS[c.type];
    if (c.dead) {
        c = Crop{};
        return;
    }
    if (c.growth < 1.f) {
        if (!byWorker) floatText(x + 0.5f, y, "Not ripe", LIGHTGRAY);
        return;
    }
    float f = soilFactor(i, c.type);
    float pollinators = (g->season() <= 1 && nearObj(x + 0.5f, y + 0.5f, O_FLOWERS, 4.5f)) ? 1.2f : 1.f;
    int n = std::max(1, (int)std::round(d.yield * (0.35f + f * 0.9f) * (1 - c.pest * 0.6f) * pollinators));
    int seeds = c.type == CR_WHEAT ? 2 : 0;
    auto give = [&](int id, int cnt) {
        if (cnt <= 0 || !id) return;
        if (byWorker) { if (!storeAnywhere(id, cnt, 1.f, wx, wy)) warnStorageFull(ITEMS[id].name); }
        else addToPlayer(id, cnt);
        if (isFood(id)) g->stats.foodMade += ITEMS[id].food * cnt;
    };
    if (beesNear(x, y)) {   // well-pollinated plants set more seed
        if (c.type == CR_WHEAT) seeds += 2;
        else if (d.seed != d.produce) give(d.seed, 2);
        else n += 1;
    }
    give(d.produce, n);
    give(d.extra, d.extraN);
    give(I_WHEATSEED, seeds);
    if (!byWorker) floatText(x + 0.5f, y, "+" + std::to_string(n) + " " + ITEMS[d.produce].name, Color{200, 255, 150, 255});
    addNutrients(i, -CROP_NPK[c.type][0], -CROP_NPK[c.type][1], -CROP_NPK[c.type][2]);
    c = Crop{};
    burst(x + 0.5f, y + 0.5f, d.col, 6);
}

static float predatorOf(int x, int y) { return g->predator[IDX(x, y)]; }

static void computePredators() {
    std::fill(g->predator.begin(), g->predator.end(), 0.f);
    auto addRadius = [](int cx, int cy, int r, float s) {
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++)
                if (inb(x, y) && dist(x, y, cx, cy) <= r) g->predator[IDX(x, y)] = std::min(1.2f, g->predator[IDX(x, y)] + s);
    };
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (o == O_POND) addRadius(x, y, 7, 0.6f);
            else if (o == O_LEAFPILE) addRadius(x, y, 4, 0.5f);
            else if (o == O_MTNASH) addRadius(x, y, 4, 0.2f);
            else if (o == O_BIRDHOUSE) addRadius(x, y, 6, 0.35f);     // nesting songbirds feed chicks on caterpillars        // berry-eating birds also eat insects
            else if (o == O_FLOWERS) addRadius(x, y, 3, 0.15f);      // hoverflies and parasitic wasps
            else if (o == O_COMPOST && g->tiles[IDX(x, y)].cont >= 0) {   // wasps, spiders and beetles
                int items = 0;
                compostRatio(g->conts[g->tiles[IDX(x, y)].cont], &items);
                if (items >= 2) addRadius(x, y, 5, 0.4f);
            }
        }
    for (auto& e : g->ents)
        if (e.alive && e.tame && (e.kind == E_DUCK || e.kind == E_FOWL)) addRadius((int)e.x, (int)e.y, 5, 0.25f);
}

// ---------------------------------------------------------------- bees
static int bloomNear(int x, int y, int R) {
    int n = 0;
    bool spring = g->season() == 0, summer = g->season() == 1;
    for (int yy = y - R; yy <= y + R; yy++)
        for (int xx = x - R; xx <= x + R; xx++) {
            if (!inb(xx, yy)) continue;
            int i = IDX(xx, yy);
            uint8_t o = g->tiles[i].obj;
            if (o == O_FLOWERS && (spring || summer)) n += 2;
            else if ((isFruitTree(o) || o == O_SERVICEBERRY || o == O_MTNASH) && (spring || summer)) n += 2;
            else if ((o == O_RASPBERRY || o == O_BLACKBERRY || o == O_BUSH) && (spring || summer)) n += 1;
            else if (g->crops[i].type && !g->crops[i].dead && g->crops[i].growth > 0.4f && (spring || summer)) n += 1;
        }
    return n;
}

bool beesNear(int x, int y) {
    for (int yy = y - 10; yy <= y + 10; yy++)
        for (int xx = x - 10; xx <= x + 10; xx++)
            if (inb(xx, yy) && g->tiles[IDX(xx, yy)].obj == O_BEEHIVE && (g->tiles[IDX(xx, yy)].var & VAR_BEES)) return true;
    return false;
}

int collectHoney(int x, int y) {
    Tile& t = g->tiles[IDX(x, y)];
    if (t.obj != O_BEEHIVE || t.timer < 1) return 0;
    int n = (int)t.timer * 2;
    t.timer -= (int)t.timer;
    return n;
}

static void dailyBees() {
    int s = g->season();
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            Tile& t = g->tiles[IDX(x, y)];
            if (t.obj != O_BEEHIVE) continue;
            int bloom = bloomNear(x, y, 8);
            if (!(t.var & VAR_BEES) && (s == 0 || s == 1) && bloom >= 6 && rnd() < 0.5f) {
                t.var |= VAR_BEES;
                logMsg("A wild swarm of bees has moved into your hive!", Color{250, 210, 80, 255});
            }
            if (!(t.var & VAR_BEES)) continue;
            if (s != 3) t.timer = std::min(5.f, t.timer + std::min(1.f, bloom / 20.f));   // honey flow follows the bloom
            // pollinated wildflowers self-seed onto nearby grass
            if (s <= 1)
                for (int yy = y - 10; yy <= y + 10; yy++)
                    for (int xx = x - 10; xx <= x + 10; xx++) {
                        if (!inb(xx, yy) || g->tiles[IDX(xx, yy)].obj != O_FLOWERS || rnd() > 0.25f) continue;
                        int nx = xx + rndi(-1, 1), ny = yy + rndi(-1, 1);
                        if (inb(nx, ny) && !g->tiles[IDX(nx, ny)].obj && g->ground[IDX(nx, ny)] == G_GRASS && !g->crops[IDX(nx, ny)].type) placeObject(nx, ny, O_FLOWERS);
                    }
        }
}

static void dailyCrops() {
    computePredators();
    int lostPests = 0;
    bool summerish = g->season() == 0 || g->season() == 1;
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            int i = IDX(x, y);
            Crop& c = g->crops[i];
            if (!c.type || c.dead) continue;
            if (!summerish && g->ground[i] != G_GREENHOUSE) continue;
            int same = 0;
            for (int yy = y - 3; yy <= y + 3; yy++)
                for (int xx = x - 3; xx <= x + 3; xx++)
                    if (inb(xx, yy) && g->crops[IDX(xx, yy)].type == c.type) same++;
            float mono = same / 49.f;
            bool mustardNear = false;
            for (int yy = y - 1; yy <= y + 1 && !mustardNear; yy++)
                for (int xx = x - 1; xx <= x + 1; xx++)
                    if (inb(xx, yy) && g->crops[IDX(xx, yy)].type == CR_MUSTARD && !g->crops[IDX(xx, yy)].dead) { mustardNear = true; break; }
            if (mustardNear) mono *= 0.3f;
            float gain = 0.07f + 0.2f * mono - predatorOf(x, y) * 0.45f;
            if (g->ground[i] == G_GREENHOUSE) gain *= 0.6f;
            c.pest = clampf(c.pest + gain, 0, 1);
            if (c.pest >= 0.97f) { c.dead = true; lostPests++; g->stats.cropsLost++; }
        }
    if (lostPests) logMsg("Insects devoured " + std::to_string(lostPests) + " crops. Ponds, leaf piles and ducks keep pests down.", Color{230, 200, 90, 255});
}

static void updateCrops(float dtDays) {
    float air = airTemp();
    bool frost = air < -0.5f;
    int killed = 0;
    for (int i = 0; i < WW * WH; i++) {
        Crop& c = g->crops[i];
        if (!c.type || c.dead) continue;
        bool gh = g->ground[i] == G_GREENHOUSE;
        float t = air + (gh ? 5.f : 0.f);
        if (frost && !gh && !CROPS[c.type].hardy) { c.dead = true; killed++; continue; }
        if (c.growth >= 1.f) continue;
        float rate = 1.f / CROPS[c.type].days;
        float tf = t < 3 ? 0.f : (t < 11 ? 0.5f : (t > 34 ? 0.4f : 1.f));
        if (c.type == CR_TOMATO && t < 15) tf *= 0.4f;   // tomatoes sulk in the cold
        if (CROPS[c.type].hardy && t >= 0 && t < 11) tf = 0.8f;
        float mf = g->moist[i] > 0.22f ? 1.f : 0.12f;
        float pf = c.pest > 0.6f ? 0.5f : 1.f;
        c.growth = std::min(1.f, c.growth + rate * tf * mf * pf * (gh ? 1.25f : 1.f) * dtDays);   // greenhouses grow a bit faster
    }
    if (killed) {
        g->stats.cropsLost += killed;
        g->frostHappened = true;
        logMsg("FROST! " + std::to_string(killed) + " crops froze in the fields.", Color{170, 210, 255, 255});
    }
}

// ---------------------------------------------------------------- spoilage
static void spoilStacks(Stack* s, int n, float mult, float dtDays, bool moldSpread) {
    bool hasMold = false;
    if (moldSpread) for (int i = 0; i < n; i++) if (s[i].id == I_MOLDY) hasMold = true;
    if (hasMold) mult *= 1.5f;
    for (int i = 0; i < n; i++) {
        Stack& st = s[i];
        if (!st.id || st.n <= 0) continue;
        float sd = ITEMS[st.id].spoilDays;
        if (sd <= 0) continue;
        st.fresh -= dtDays * mult / sd;
        if (st.fresh <= 0) { st.id = I_MOLDY; st.fresh = 1; }
    }
}

static void updateSpoilage(float dtDays) {
    float air = airTemp();
    float tf = air < 4 ? 0.35f : (air > 26 ? 1.4f : 1.f);
    spoilStacks(g->pl.inv, INV_SLOTS, tf, dtDays, false);
    for (auto& c : g->conts) {
        if (!c.alive) continue;
        int i = IDX(c.x, c.y);
        bool indoor = g->indoor[i];
        float m = tf;
        switch (c.kind) {
            case C_CHEST: m *= 0.9f; if (!indoor && raining()) m *= 2.f; break;
            case C_GRANARY: m *= 0.33f; break;
            case C_CELLAR: m = 0.22f * 0.6f; break;
            case C_FRIDGE: m = poweredAt(c.x, c.y) ? 0.04f : tf * 1.1f; break;
            case C_TROUGH: m *= indoor ? 1.f : (raining() ? 2.f : 1.2f); break;
            default: continue;
        }
        if (g->flooded[i]) m *= 4.f;
        spoilStacks(c.slots.data(), (int)c.slots.size(), m, dtDays, true);
    }
}

// ---------------------------------------------------------------- compost & biogas
// Carbon and nitrogen parts of compostable items. Browns (leaves, hay, fiber, sticks) are carbon-rich;
// greens (manure, rotten food, vegetable scraps) are nitrogen-rich. A pile composts best near 30:1.
bool compostParts(int id, float& c, float& n) {
    n = 1;
    switch (id) {
        case I_STICK: c = 100; return true;
        case I_LEAVES: c = 60; return true;
        case I_HAY: case I_FIBER: c = 45; return true;
        case I_MANURE: c = 15; return true;
        case I_MOLDY: case I_BERRIES: case I_CABBAGE: case I_POTATO: case I_BEANS: case I_TOMATO: case I_SQUASH: case I_GREENBEANS:
        case I_APPLE: case I_APPLE_SPITTER: case I_APPLE_PRIZE: case I_PEAR: case I_APRICOT: case I_GRAPES: case I_BLACKBERRY: case I_RASPBERRY: c = 15; return true;
        default: return false;
    }
}

float compostRatio(const Container& c, int* items) {
    float sc = 0, sn = 0; int n = 0;
    for (auto& s : c.slots) {
        float pc, pn;
        if (s.id && compostParts(s.id, pc, pn)) { sc += pc * s.n; sn += pn * s.n; n += s.n; }
    }
    if (items) *items = n;
    return sn > 0 ? sc / sn : 0;
}

float compostQuality(float ratio) {
    if (ratio <= 0) return 0;
    return clampf(1.f - std::fabs(std::log(ratio / 30.f)) / std::log(4.f), 0.15f, 1.f);
}

static void updateProcessors(float dtDays) {
    for (auto& c : g->conts) {
        if (!c.alive) continue;
        Tile& t = g->tiles[IDX(c.x, c.y)];
        int n = (int)c.slots.size();
        if (c.kind == C_BIOGAS) {
            if (countItem(c.slots.data(), n, I_MANURE) < 2) continue;
            t.timer += dtDays;
            if (t.timer < 0.12f) continue;
            t.timer = 0;
            takeItem(c.slots.data(), n, I_MANURE, 2);
            if (!storeAnywhere(I_COMPOST, 1, 0.6f, c.x, c.y)) addItem(c.slots.data(), n, I_COMPOST, 1, 0.6f);   // digestate
            g->stats.composted++;
        } else if (c.kind == C_COMPOST) {
            int items = 0;
            float q = compostQuality(compostRatio(c, &items));
            if (items < 3) continue;
            t.timer += dtDays * (0.35f + 0.65f * q);   // a balanced pile heats up and works fast
            if (t.timer < 0.2f) continue;
            t.timer = 0;
            // consume one brown and one green where possible, plus one more of whatever is most plentiful
            int took = 0, brown = 0, green = 0;
            for (auto& s : c.slots) {
                float pc, pn;
                if (!s.id || !compostParts(s.id, pc, pn)) continue;
                if (pc >= 40) brown += s.n; else green += s.n;
            }
            auto takeKind = [&](bool wantBrown) {
                for (auto& s : c.slots) {
                    float pc, pn;
                    if (s.id && compostParts(s.id, pc, pn) && (pc >= 40) == wantBrown) { if (--s.n <= 0) s = Stack{}; took++; return true; }
                }
                return false;
            };
            takeKind(true); takeKind(false);
            while (took < 3 && (takeKind(brown >= green) || takeKind(brown < green))) {}
            if (!storeAnywhere(I_COMPOST, 1, q, c.x, c.y)) addItem(c.slots.data(), n, I_COMPOST, 1, q);
            g->stats.composted++;
        }
    }
}

// ---------------------------------------------------------------- power
// Sources charge battery banks; machines draw from them. Anything more than CABLE tiles from a
// bank is unconnected and does nothing. Banks within CABLE tiles of each other share one network.
// Units: kW for flow, kWh for storage (battery charge lives in the bank tile's timer).
static const float CABLE = 8.f;
static const float BATTERY_KWH = 10.f;

struct PowerNet { float gen = 0, load = 0, cap = 0, stored = 0; std::vector<int> banks; std::vector<std::pair<int, float>> loads; };
static std::vector<PowerNet> nets;
static std::vector<int> linkOf, netOf;      // per tile: linked bank tile, network index
static std::vector<uint8_t> running;        // per tile: machine has power this tick
static std::vector<int> linked;              // tiles to reset next pass
static bool anyBiogas = false;

static bool isPowerSource(uint8_t o) { return o == O_SOLAR || o == O_WHEEL || o == O_BIOGAS; }
static bool isPowerLoad(uint8_t o) { return o == O_LAMP || o == O_HEATER || o == O_PUMP || o == O_FRIDGE; }
static std::vector<uint8_t> pumpWanted;   // per tile: a pump has active irrigation downstream
bool isPowerObj(uint8_t o) { return isPowerSource(o) || isPowerLoad(o) || o == O_BATTERY; }

float solarFactor() {
    float rise, set;
    sunTimes(rise, set);
    float t = g->tod;
    if (t <= rise || t >= set) return 0;
    float elev = std::sin(3.14159f * (t - rise) / (set - rise));   // low sun at dawn and dusk
    static const float seasonF[4] = {0.85f, 1.f, 0.65f, 0.35f};    // winter sun is low and weak
    float f = elev * seasonF[g->season()];
    switch (g->weather) {
        case WX_CLOUDY: case WX_FOG: f *= 0.45f; break;
        case WX_RAIN: f *= 0.25f; break;
        case WX_SNOW: f *= 0.2f; break;
        case WX_STORM: f *= 0.15f; break;
        case WX_BLIZZARD: f *= 0.05f; break;
        default: break;
    }
    if (g->snowCover > 0.6f) f *= 0.6f;   // snow on the panels
    return f;
}

// Instantaneous output (kW, positive) or demand (kW, negative) of a power object.
float powerFlowAt(int x, int y) {
    if (!inb(x, y)) return 0;
    Tile& t = g->tiles[IDX(x, y)];
    switch (t.obj) {
        case O_SOLAR: return 0.4f * solarFactor();
        case O_WHEEL: {
            bool dam = false, water = false;
            for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) {
                if (!inb(x + j, y + k)) continue;
                if (g->tiles[IDX(x + j, y + k)].obj == O_DAM) dam = true;
                if (isWater(x + j, y + k)) water = true;
            }
            if (!water) return 0;
            float f = dam ? 1.5f : 0.3f;
            return airTemp() < -6 ? f * 0.3f : f;   // a frozen river barely turns the wheel
        }
        case O_BIOGAS: {
            if (t.cont < 0) return 0;
            Container& c = g->conts[t.cont];
            return countItem(c.slots.data(), (int)c.slots.size(), I_MANURE) > 0 ? 1.f : 0.f;
        }
        case O_LAMP: return sunLevel() < 0.3f ? -0.06f : 0.f;
        case O_HEATER: return airTemp() < 16 ? -1.5f : 0.f;
        case O_PUMP: return (!pumpWanted.empty() && pumpWanted[IDX(x, y)]) ? -0.4f : 0.f;
        case O_FRIDGE: return -0.12f;
        default: return 0;
    }
}

static int findRoot(std::vector<int>& p, int a) { while (p[a] != a) a = p[a] = p[p[a]]; return a; }

static void updatePower(float dt) {
    if (linkOf.empty()) { linkOf.assign(WW * WH, -1); netOf.assign(WW * WH, -1); running.assign(WW * WH, 0); }
    for (int i : linked) { linkOf[i] = -1; netOf[i] = -1; running[i] = 0; }
    linked.clear();
    nets.clear();
    std::vector<int> banks, devices;
    for (int i = 0; i < WW * WH; i++) {
        uint8_t o = g->tiles[i].obj;
        if (o == O_BATTERY) banks.push_back(i);
        else if (o && isPowerObj(o)) devices.push_back(i);
    }
    // group banks into networks
    std::vector<int> parent(banks.size());
    for (size_t a = 0; a < banks.size(); a++) parent[a] = (int)a;
    for (size_t a = 0; a < banks.size(); a++)
        for (size_t b = a + 1; b < banks.size(); b++)
            if (dist(banks[a] % WW, banks[a] / WW, banks[b] % WW, banks[b] / WW) <= CABLE) parent[findRoot(parent, (int)a)] = findRoot(parent, (int)b);
    std::vector<int> rootNet(banks.size(), -1);
    for (size_t a = 0; a < banks.size(); a++) {
        int r = findRoot(parent, (int)a);
        if (rootNet[r] < 0) { rootNet[r] = (int)nets.size(); nets.emplace_back(); }
        PowerNet& n = nets[rootNet[r]];
        n.banks.push_back(banks[a]);
        n.cap += BATTERY_KWH;
        n.stored += clampf(g->tiles[banks[a]].timer, 0, BATTERY_KWH);
        netOf[banks[a]] = rootNet[r]; linkOf[banks[a]] = banks[a]; linked.push_back(banks[a]);
    }
    // wire each device to its nearest bank
    anyBiogas = false;
    for (int i : devices) {
        int x = i % WW, y = i / WW, best = -1;
        float bd = CABLE + 0.01f;
        for (int b : banks) { float d = dist(x, y, b % WW, b / WW); if (d < bd) { bd = d; best = b; } }
        if (best < 0) continue;
        int n = netOf[best];
        linkOf[i] = best; netOf[i] = n; linked.push_back(i);
        float f = powerFlowAt(x, y);
        if (f > 0) nets[n].gen += f;
        else if (isPowerLoad(g->tiles[i].obj)) { nets[n].load -= f; nets[n].loads.push_back({i, -f}); }
        if (g->tiles[i].obj == O_BIOGAS && f > 0) anyBiogas = true;
    }
    // charge / discharge
    float hours = dt * 24.f / DAY_SECONDS;
    g->powerGen = g->powerUse = g->powerStored = g->powerCap = 0;
    g->powered = false;
    // machines that matter most get power first when supply runs short
    auto priority = [](int i) {
        switch (g->tiles[i].obj) { case O_FRIDGE: return 0; case O_PUMP: return 1; case O_LAMP: return 2; default: return 3; }
    };
    for (auto& n : nets) {
        float budget = n.gen + (hours > 0 ? n.stored / hours : 0);   // kW available this tick
        std::sort(n.loads.begin(), n.loads.end(), [&](auto& a, auto& b) { return priority(a.first) < priority(b.first); });
        float used = 0;
        for (auto& [tile, draw] : n.loads)
            if (draw > 0 && used + draw <= budget + 1e-6f) { used += draw; running[tile] = 1; }
        // idle machines (lamp by day, heater when warm) are "on" as long as the network has anything
        bool live = budget > 1e-6f;
        for (int i : linked)
            if (netOf[i] >= 0 && &nets[netOf[i]] == &n && isPowerLoad(g->tiles[i].obj) && powerFlowAt(i % WW, i / WW) == 0) running[i] = live;
        n.stored = clampf(n.stored + (n.gen - used) * hours, 0, n.cap);
        for (int b : n.banks) g->tiles[b].timer = n.stored / n.banks.size();
        g->powerGen += n.gen; g->powerUse += n.load; g->powerStored += n.stored; g->powerCap += n.cap;
        g->powered = g->powered || used > 0;
    }
}

bool poweredAt(int x, int y) {
    if (!inb(x, y) || running.empty()) return false;
    return running[IDX(x, y)] != 0;
}

int powerLink(int i) { return linkOf.empty() ? -1 : linkOf[i]; }

bool powerNetInfo(int x, int y, float& gen, float& load, float& stored, float& cap) {
    if (!inb(x, y) || netOf.empty()) return false;
    int n = netOf[IDX(x, y)];
    if (n < 0 || n >= (int)nets.size()) return false;
    gen = nets[n].gen; load = nets[n].load; stored = nets[n].stored; cap = nets[n].cap;
    return true;
}

// ---------------------------------------------------------------- irrigation
// Wells (1.5 bar) and electric pumps beside water (4.5 bar, need battery power) pressurise a pipe
// network; sources within PIPE tiles of each other share it. Each sprinkler/drip line within PIPE tiles
// of a source joins it. Every device that is scheduled on uses some pressure; a device only sprays if
// the pressure left on its network is at least its minimum. Too many devices starve the whole line.
static const float PIPE = 8.f;
struct WaterNet { float supply = 0, draw = 0, drawAll = 0; };
static std::vector<WaterNet> wnets;
static std::vector<int> wLink, wNet, wLinked;
static std::vector<uint8_t> spraying, covered;

static bool isWaterSource(uint8_t o) { return o == O_WELL || o == O_PUMP; }
static bool isWaterUser(uint8_t o) { return o == O_SPRINKLER || o == O_DRIP || o == O_WATERFALL; }
bool isWaterObj(uint8_t o) { return isWaterSource(o) || isWaterUser(o); }
static float waterDraw(uint8_t o) { return o == O_SPRINKLER ? 1.2f : o == O_WATERFALL ? 1.0f : 0.25f; }
float waterMinPressure(uint8_t o) { return o == O_SPRINKLER ? 2.0f : o == O_WATERFALL ? 1.5f : 0.6f; }
static int waterRadius(uint8_t o) { return o == O_SPRINKLER ? 3 : o == O_WATERFALL ? 0 : 1; }

static const char* TIMER_NAMES[] = {"Always on", "Mornings 05-08", "Evenings 18-21", "Morning + evening", "Off"};
const char* timerName(int mode) { return TIMER_NAMES[std::clamp(mode, 0, 4)]; }
bool timerActive(int mode) {
    float h = g->tod * 24;
    bool morning = h >= 5 && h < 8, evening = h >= 18 && h < 21;
    switch (mode) { case 0: return true; case 1: return morning; case 2: return evening; case 3: return morning || evening; default: return false; }
}
static int deviceMode(int i) { return std::clamp((int)g->tiles[i].timer, 0, 4); }

static bool pumpInWater(int x, int y) {
    for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) {
        if (!inb(x + j, y + k)) continue;
        if (isWater(x + j, y + k) || g->tiles[IDX(x + j, y + k)].obj == O_POND) return true;
    }
    return false;
}

static void updateWater(float dt) {
    int N = WW * WH;
    if (wLink.empty()) { wLink.assign(N, -1); wNet.assign(N, -1); spraying.assign(N, 0); covered.assign(N, 0); pumpWanted.assign(N, 0); }
    for (int i : wLinked) { wLink[i] = -1; wNet[i] = -1; spraying[i] = 0; pumpWanted[i] = 0; }
    wLinked.clear();
    std::fill(covered.begin(), covered.end(), 0);
    wnets.clear();
    std::vector<int> sources, users;
    for (int i = 0; i < N; i++) {
        uint8_t o = g->tiles[i].obj;
        if (isWaterSource(o)) sources.push_back(i);
        else if (isWaterUser(o)) users.push_back(i);
    }
    std::vector<int> parent(sources.size());
    for (size_t a = 0; a < sources.size(); a++) parent[a] = (int)a;
    std::function<int(int)> root = [&](int a) { while (parent[a] != a) a = parent[a] = parent[parent[a]]; return a; };
    for (size_t a = 0; a < sources.size(); a++)
        for (size_t b = a + 1; b < sources.size(); b++)
            if (dist(sources[a] % WW, sources[a] / WW, sources[b] % WW, sources[b] / WW) <= PIPE) parent[root((int)a)] = root((int)b);
    std::vector<int> netFor(sources.size(), -1);
    for (size_t a = 0; a < sources.size(); a++) {
        int r = root((int)a);
        if (netFor[r] < 0) { netFor[r] = (int)wnets.size(); wnets.emplace_back(); }
        int i = sources[a];
        wNet[i] = netFor[r]; wLink[i] = i; wLinked.push_back(i);
    }
    for (int i : users) {
        int x = i % WW, y = i / WW, best = -1;
        float bd = PIPE + 0.01f;
        for (int s : sources) { float d = dist(x, y, s % WW, s / WW); if (d < bd) { bd = d; best = s; } }
        if (best < 0) continue;
        wLink[i] = best; wNet[i] = wNet[best]; wLinked.push_back(i);
        uint8_t o = g->tiles[i].obj;
        int mode = deviceMode(i);
        if (lineBroken(g->tiles[i])) {   // a chewed line leaks constantly
            wnets[wNet[i]].draw += 1.f; wnets[wNet[i]].drawAll += 1.f;
            for (int s : sources) if (wNet[s] == wNet[i] && g->tiles[s].obj == O_PUMP) pumpWanted[s] = 1;
            continue;
        }
        if (mode != 4) wnets[wNet[i]].drawAll += waterDraw(o);
        if (timerActive(mode)) {
            wnets[wNet[i]].draw += waterDraw(o);
            for (int s : sources) if (wNet[s] == wNet[i] && g->tiles[s].obj == O_PUMP) pumpWanted[s] = 1;
        }
    }
    for (int s : sources) {
        int x = s % WW, y = s / WW;
        float p = 0;
        if (g->tiles[s].obj == O_WELL) p = 1.5f;
        else if (pumpInWater(x, y) && poweredAt(x, y)) p = 4.5f;
        wnets[wNet[s]].supply += p;
    }
    for (int i : users) {
        if (wNet[i] < 0) continue;
        const WaterNet& n = wnets[wNet[i]];
        uint8_t o = g->tiles[i].obj;
        int x = i % WW, y = i / WW, R = waterRadius(o);
        if (lineBroken(g->tiles[i])) { if (n.supply > 0) g->moist[i] = 1; continue; }   // a muddy puddle at the break
        float minP = waterMinPressure(o);
        bool works = n.supply - n.draw >= minP;
        bool canWork = n.supply - n.drawAll >= minP || works;     // would work on its schedule
        spraying[i] = timerActive(deviceMode(i)) && works;
        if (deviceMode(i) != 4 && canWork && o != O_WATERFALL)
            for (int yy = y - R; yy <= y + R; yy++) for (int xx = x - R; xx <= x + R; xx++) if (inb(xx, yy)) covered[IDX(xx, yy)] = 1;
        if (!spraying[i]) continue;
        float rate = o == O_SPRINKLER ? 0.12f : 0.15f;
        for (int yy = y - R; yy <= y + R; yy++)
            for (int xx = x - R; xx <= x + R; xx++)
                if (inb(xx, yy)) g->moist[IDX(xx, yy)] = std::min(1.f, g->moist[IDX(xx, yy)] + rate * dt);
    }
}

void breakLine(int x, int y) {
    Tile& t = g->tiles[IDX(x, y)];
    if (t.obj != O_SPRINKLER && t.obj != O_DRIP) return;
    t.var |= VAR_BROKEN;
    static float lastMsg = -1000;
    if (g->totalTime - lastMsg > 30 || g->totalTime < lastMsg) {
        lastMsg = g->totalTime;
        logMsg(std::string("Coyotes chewed through a ") + OBJS[t.obj].name + "! Water is spraying out - right-click it to repair.", Color{230, 190, 120, 255});
    }
}

void repairLine(int x, int y) {
    g->tiles[IDX(x, y)].var &= (uint8_t)~VAR_BROKEN;
    burst(x + 0.5f, y + 0.5f, Color{150, 200, 255, 255}, 6);
}

bool irrigating(int x, int y) { return inb(x, y) && !spraying.empty() && spraying[IDX(x, y)]; }
bool leaking(int x, int y) {
    if (!inb(x, y) || wNet.empty() || wNet[IDX(x, y)] < 0 || !lineBroken(g->tiles[IDX(x, y)])) return false;
    return wnets[wNet[IDX(x, y)]].supply > 0;
}
bool irrigationCovers(int x, int y) { return inb(x, y) && !covered.empty() && covered[IDX(x, y)]; }
int waterLink(int i) { return wLink.empty() ? -1 : wLink[i]; }
bool waterNetInfo(int x, int y, float& supply, float& draw, float& pressure) {
    if (!inb(x, y) || wNet.empty() || wNet[IDX(x, y)] < 0) return false;
    const WaterNet& n = wnets[wNet[IDX(x, y)]];
    supply = n.supply; draw = n.draw; pressure = n.supply - n.draw;
    return true;
}

// ---------------------------------------------------------------- tribes & roads
void checkRoads() {
    // BFS across road/village tiles from each friendly village; connected if it reaches a trading post
    for (auto& t : g->tribes) {
        if (t.hostile) continue;
        bool was = t.road;
        std::vector<uint8_t> vis(WW * WH, 0);
        std::vector<int> q;
        q.push_back(IDX(t.x, t.y)); vis[q[0]] = 1;
        bool ok = false;
        for (size_t h = 0; h < q.size() && !ok; h++) {
            int i = q[h], x = i % WW, y = i / WW;
            for (int k = 0; k < 4; k++) {
                int nx = x + (k == 0) - (k == 1), ny = y + (k == 2) - (k == 3);
                if (!inb(nx, ny)) continue;
                int j = IDX(nx, ny);
                if (g->tiles[j].obj == O_TRADEPOST) { ok = true; break; }
                if (vis[j]) continue;
                uint8_t gr = g->ground[j];
                if (gr == G_ROAD || gr == G_VILLAGE || (g->tiles[j].obj == O_DAM)) { vis[j] = 1; q.push_back(j); }
            }
        }
        t.road = ok;
        if (ok && !was) logMsg("Trade route established with " + t.name + "! Trade from your trading post at better prices.", Color{130, 220, 255, 255});
    }
}

static void tribeDaily() {
    for (size_t k = 0; k < g->tribes.size(); k++) {
        Tribe& t = g->tribes[k];
        if (!t.hostile) {
            if (g->day % DAYS_PER_SEASON == 0) {
                auto add = [&](int id, int n) { for (auto& s : t.stock) if (s.id == id) { s.n = (int16_t)std::min(60, s.n + n); return; } t.stock.push_back(Stack{(uint8_t)id, (int16_t)n, 1}); };
                if (k == 0) { add(I_GLASS, 6); add(I_COPPER, 3); add(I_BEANS, 8); add(I_CABBAGESEED, 6); add(I_WHEATSEED, 8); add(I_BREAD, 4); add(I_CLOTH, 2); }
                else { add(I_COPPER, 5); add(I_ORE, 8); add(I_LEATHER, 3); add(I_WOOL, 6); add(I_POTATO, 8); add(I_JERKY, 4); add(I_BRICK, 8); }
                t.recruits = std::min(4, t.recruits + 1);
            }
            continue;
        }
        // raiders only come once there is something worth taking
        if (countStructures() < 6 || g->day < DAYS_PER_SEASON * 2) { if (t.nextRaid <= g->day + 1) t.nextRaid = g->day + 2; continue; }
        if (g->day == t.nextRaid - 1 && !t.raidWarned) {
            t.raidWarned = true;
            logMsg("Smoke signals rise over the " + t.name + " camp. A raid is coming soon!", Color{255, 90, 70, 255});
        }
    }
}

// ---------------------------------------------------------------- daily cycle
static void spawnWolfPack() {
    int s = g->season();
    float chance = s == 3 ? 0.85f : (s == 2 ? 0.55f : 0.35f);
    if (g->day < 2) chance *= 0.3f;
    if (rnd() > chance) return;
    for (int attempt = 0; attempt < 60; attempt++) {
        float a = rndr(0, 6.283f), d = rndr(26, 40);
        int x = (int)(g->pl.x + std::cos(a) * d), y = (int)(g->pl.y + std::sin(a) * d);
        if (!inb(x, y) || solidFor(x, y, 1) || isWater(x, y)) continue;
        int n = rndi(2, 3) + std::min(3, g->day / 14) + (s == 3 ? 1 : 0);
        for (int k = 0; k < n; k++) {
            Ent& w = spawnEnt(E_WOLF, x + rndr(-1, 1), y + rndr(-1, 1));
            w.state = 1;  // hunting
            w.homeX = (float)x; w.homeY = (float)y;
        }
        if (s == 3) logMsg("Wolves howl in the cold night...", Color{180, 180, 220, 255});
        return;
    }
}

static void spawnCoyotes() {
    int lines = 0, birds = 0;
    for (auto& t : g->tiles) if (t.obj == O_DRIP || t.obj == O_SPRINKLER) lines++;
    for (auto& e : g->ents) if (e.alive && e.tame && (e.kind == E_DUCK || e.kind == E_FOWL)) birds++;
    if (!lines && !birds) return;
    float chance = std::min(0.5f, 0.15f + lines * 0.015f) * (g->season() == 3 ? 0.6f : 1.f);
    if (rnd() > chance) return;
    for (int attempt = 0; attempt < 60; attempt++) {
        float a = rndr(0, 6.283f), d = rndr(24, 36);
        int x = (int)(g->pl.x + std::cos(a) * d), y = (int)(g->pl.y + std::sin(a) * d);
        if (!inb(x, y) || solidFor(x, y, 1) || isWater(x, y)) continue;
        int n = rndi(1, 2) + (lines > 20 ? 1 : 0);
        for (int k = 0; k < n; k++) {
            Ent& c = spawnEnt(E_COYOTE, x + rndr(-1, 1), y + rndr(-1, 1));
            c.state = 1; c.homeX = (float)x; c.homeY = (float)y;
        }
        return;
    }
}

static void spawnPorcupine() {
    int alive = 0;
    for (auto& e : g->ents) if (e.alive && e.kind == E_PORCUPINE) alive++;
    if (alive >= 2 || rnd() > (g->season() >= 2 ? 0.35f : 0.1f)) return;
    for (int attempt = 0; attempt < 80; attempt++) {
        float a = rndr(0, 6.283f), d = rndr(18, 32);
        int x = (int)(g->pl.x + std::cos(a) * d), y = (int)(g->pl.y + std::sin(a) * d);
        if (!inb(x, y) || solidFor(x, y, 1) || g->ground[IDX(x, y)] != G_FOREST) continue;
        Ent& p = spawnEnt(E_PORCUPINE, x + 0.5f, y + 0.5f);
        p.homeX = x + 0.5f; p.homeY = y + 0.5f;
        return;
    }
}

static void dawn() {
    spawnPorcupine();
    g->stats.nightsSurvived++;
    for (auto& e : g->ents) if (e.alive && (e.kind == E_WOLF || e.kind == E_COYOTE)) e.state = 2;  // retreat
    float bx, by;
    if (baseCentre(bx, by)) { /* autosave handled in main loop via flag */ }
    g->autosaveMsg = -1;  // request autosave
}

void newDay() {
    g->day++;
    g->stats.lastFoodMade = g->stats.foodMade;
    g->stats.lastFoodEaten = g->stats.foodEaten;
    g->stats.foodMade = g->stats.foodEaten = 0;
    g->dayTempOffset = rndr(-3, 3);
    int s = g->season();
    int dos = g->dayOfSeason();
    if (dos == 0 && g->day > 0) logMsg("~ Week " + std::to_string(g->week()) + " begins ~", Color{255, 230, 150, 255});
    // soil
    float evap = s == 1 ? 0.3f : (s == 3 ? 0.08f : 0.18f);
    if (g->drought) evap *= 1.8f;
    if (g->heatwave) evap *= 1.5f;
    for (int i = 0; i < WW * WH; i++) {
        if (g->ground[i] != G_GREENHOUSE || !g->indoor[i]) g->moist[i] = clampf(g->moist[i] - evap, 0, 1);
        if (g->ground[i] == G_TILLED && !g->crops[i].type && rnd() < 0.04f) {   // slow recovery while fallow
            if (g->fert[i] < 0.6f) g->fert[i] += 0.02f;
            if (g->phos[i] < 0.6f) g->phos[i] += 0.015f;
            if (g->pota[i] < 0.6f) g->pota[i] += 0.015f;
        }
        if (g->ground[i] == G_BURNT && rnd() < 0.12f && s != 3) g->ground[i] = G_GRASS;
    }
    // water table: soil within 4 tiles of open water or a pond stays damp
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            int i = IDX(x, y);
            if (g->ground[i] != G_TILLED || g->moist[i] >= 0.3f) continue;
            bool near = false;
            for (int k = -4; k <= 4 && !near; k++)
                for (int j = -4; j <= 4; j++)
                    if (inb(x + j, y + k) && (isWater(x + j, y + k) || g->tiles[IDX(x + j, y + k)].obj == O_POND) && j * j + k * k <= 16) { near = true; break; }
            if (near) g->moist[i] = 0.3f;
        }
    // weekly leaf drop: deciduous trees feed the soil around them
    if (dos == 0)
        for (int y = 2; y < WH - 2; y++)
            for (int x = 2; x < WW - 2; x++) {
                uint8_t o = g->tiles[IDX(x, y)].obj;
                if (!isDeciduous(o)) continue;
                for (int k = -2; k <= 2; k++)
                    for (int j = -2; j <= 2; j++) {
                        int i = IDX(x + j, y + k);
                        if (j * j + k * k <= 5 && !isWater(x + j, y + k)) addNutrients(i, 0.01f, 0.005f, 0.02f);   // leaf litter is rich in potassium
                    }
            }
    // fruit: mature plants ripen every cycle (trees 7 days, vines and canes 4); ripe fruit waits to be picked.
    // fruitState: 1 = ripe, otherwise counts days since the last harvest (or since the plant matured)
    {
        int ripened = 0;
        for (int i = 0; i < WW * WH; i++) {
            uint8_t o = g->tiles[i].obj;
            if (!isFruitPlant(o)) continue;
            uint8_t& st = g->fruitState[i];
            if (st == 1) continue;
            int cycle = isFruitTree(o) ? DAYS_PER_SEASON : 4;
            st = (uint8_t)std::min(250, std::max(2, (int)st) + 1);
            if (st >= 2 + cycle) { st = 1; ripened++; }
        }
        if (ripened) logMsg("Fruit is ripe in your orchard and hedgerows.", Color{240, 170, 90, 255});
    }
    // vegetation
    for (int y = 1; y < WH - 1; y++)
        for (int x = 1; x < WW - 1; x++) {
            int i = IDX(x, y);
            Tile& t = g->tiles[i];
            if (t.obj == O_SAPLING) {
                t.timer += g->moist[i] > 0.5f ? 2 : 1;
                if (t.timer >= DAYS_PER_SEASON) {   // fully grown after one cycle
                    int grow = (t.var >> 2) & 7;   // species planted (0 = wild oak or pine, 5 = fruit variety)
                    static const int kinds[] = {0, O_MAPLE, O_MTNASH, O_ASPEN, O_SERVICEBERRY};
                    int gene = g->fruitGene[i];
                    int o = grow > 0 && grow < 5 ? kinds[grow] : (rnd() < 0.5f ? O_OAK : O_PINE);
                    if (gene >= 0) { static const int fk[] = {O_APPLE, O_PEAR, O_APRICOT}; o = fk[g->genes[gene].kind]; }
                    bool guard = t.var & VAR_GUARD;
                    removeObject(x, y, false);
                    placeObject(x, y, o);
                    g->fruitGene[i] = gene;
                    g->fruitState[i] = 2;   // first fruit one cycle after maturing
                    if (guard) t.var |= VAR_GUARD;
                }
            }
            else if (t.obj == O_BUSH && t.var >= 4 && (s == 0 || s == 1)) { if (rnd() < 0.4f) t.var -= 4; }
            else if (t.obj == O_ASH && rnd() < 0.15f) removeObject(x, y, false);
            else if (t.obj == O_NONE && s != 3 && !g->crops[i].type) {
                uint8_t gr = g->ground[i];
                if ((gr == G_GRASS || gr == G_DIRT) && rnd() < 0.006f) placeObject(x, y, O_TALLGRASS);
                if (gr == G_FOREST && rnd() < 0.004f) placeObject(x, y, O_SAPLING);
                if (gr == G_FOREST && rnd() < 0.003f) placeObject(x, y, O_BRANCH);
            }
        }
    // floods recede
    if (g->floodActive) {
        for (int i = 0; i < WW * WH; i++)
            if (g->flooded[i]) { g->ground[i] = g->origGround[i]; g->flooded[i] = 0; g->moist[i] = 1; addNutrients(i, 0.1f, 0.12f, 0.12f); }
        g->floodActive = false;
        logMsg("The flood waters recede.", Color{150, 200, 255, 255});
    }
    // disasters
    g->heatwave = false;
    if (s == 1 && g->dryDays >= 3 && !g->drought) { g->drought = true; logMsg("DROUGHT: no rain for days. Soil dries fast and fire spreads easily.", Color{255, 190, 90, 255}); }
    if (rnd() < 0.08f) { g->heatwave = true; logMsg("HEATWAVE: scorching day ahead. Drink water and stay in shade.", Color{255, 150, 60, 255}); }
    if (rnd() < 0.08f) {
        logMsg("A swarm of insects descends on the fields!", Color{230, 200, 90, 255});
        for (auto& c : g->crops) if (c.type && !c.dead) c.pest = std::min(1.f, c.pest + 0.35f);
    }
    if (s == 3 && rnd() < 0.25f) { g->weather = WX_BLIZZARD; g->weatherTimer = 150; logMsg("BLIZZARD! Extreme cold today.", Color{200, 230, 255, 255}); }
    if (g->stormYesterday && g->day > DAYS_PER_SEASON && rnd() < 0.4f) {
        int dams = 0;
        for (auto& t : g->tiles) if (t.obj == O_DAM) dams++;
        if (dams > 0) logMsg("Storm runoff swells the river - your dam holds back the flood.", Color{150, 200, 255, 255});
        else {
            g->origGround = g->ground;
            int n = 0;
            for (int y = 0; y < WH; y++)
                for (int x = 0; x < WW; x++) {
                    int i = IDX(x, y);
                    if (isWater(x, y) || g->height[i] > 0.5f) continue;
                    bool nearRiver = false;
                    for (int k = -3; k <= 3 && !nearRiver; k++)
                        for (int j = -3; j <= 3; j++)
                            if (inb(x + j, y + k) && g->river[IDX(x + j, y + k)]) { nearRiver = true; break; }
                    if (!nearRiver) continue;
                    uint8_t o = g->tiles[i].obj;
                    if (o && OBJS[o].solid) continue;
                    g->flooded[i] = 1; g->ground[i] = G_WATER; n++;
                    if (g->crops[i].type) { g->crops[i] = Crop{}; g->stats.cropsLost++; }
                    if (o && !OBJS[o].structure) removeObject(x, y, false);
                }
            g->floodActive = n > 0;
            if (n) logMsg("FLOOD! Yesterday's storm has flooded the riverbanks. A dam would control it.", Color{120, 170, 255, 255});
        }
    }
    // frost warning for tomorrow night
    if (!g->frostWarned && s == 2) {
        float low = tempAtTime(g->day + 1, 0.12f, 0) - 3;
        bool crops = false;
        for (int i = 0; i < WW * WH && !crops; i++) if (g->crops[i].type && !g->crops[i].dead && !CROPS[g->crops[i].type].hardy && g->ground[i] != G_GREENHOUSE) crops = true;
        if (low < 1 && crops) {
            g->frostWarned = true;
            logMsg("Geese fly south. The first frost is near - harvest your crops!", Color{170, 210, 255, 255});
        }
    }
    g->stormYesterday = false;
    dailyBees();
    dailyCrops();
    tribeDaily();
    spawnWildlife(false);
    // daily breeding & animal produce of tame animals handled in entities; workers mood
    checkRoads();
}

// ---------------------------------------------------------------- goals
static bool anyObj(int o) { for (auto& t : g->tiles) if (t.obj == o) return true; return false; }
static bool anyFruit() {
    for (int i = 0; i < WW * WH; i++) if (isFruitPlant(g->tiles[i].obj) || (g->tiles[i].obj == O_SAPLING && g->fruitGene[i] >= 0)) return true;
    return false;
}
static bool anyBees() {
    for (auto& t : g->tiles) if (t.obj == O_BEEHIVE && (t.var & VAR_BEES)) return true;
    return false;
}
static bool anyIrrigating() {
    for (int i = 0; i < WW * WH; i++) if ((g->tiles[i].obj == O_SPRINKLER || g->tiles[i].obj == O_DRIP) && irrigating(i % WW, i / WW)) return true;
    return false;
}
static bool anyPowered(int o) {
    for (int i = 0; i < WW * WH; i++) if (g->tiles[i].obj == o && poweredAt(i % WW, i / WW)) return true;
    return false;
}

void initGoals() {
    if (g->goals.empty()) {
        g->goals = {
            {"Craft a stone axe", "Gather sticks, flint and fiber, then open crafting (Tab)."},
            {"Light a campfire", "Fire keeps you warm, cooks food and scares wolves."},
            {"Build a shelter", "Enclose a bed with walls and a door. Indoors is warmer."},
            {"Plant a crop", "Till grass with a hoe, then plant seeds. Wild seeds come from tall grass."},
            {"Tame an animal", "Hold food an animal likes and right-click it repeatedly."},
            {"Plant an orchard tree", "Plant an apple seed or fruit sapling. It matures in a week, then fruits weekly."},
            {"Store food properly", "Granaries and root cellars keep food from rotting."},
            {"Get a guard dog", "Tame a dog with meat. Dogs fight off wolves."},
            {"Dig a well", "Workers need water. A well provides it."},
            {"Start composting", "Put manure, leaves or moldy food in a compost bin."},
            {"Recruit a worker", "Trade with a friendly village. Have a free bed ready."},
            {"Open a trade route", "Build road from a village to a trading post."},
            {"Thrive for four weeks", "Keep everyone fed, watered and housed for 28 days."},
            {"Generate electricity", "Solar panel or water wheel wired to a battery bank (within 8 tiles)."},
            {"Keep bees", "Build a hive near flowers; a wild swarm will move in."},
            {"Automate irrigation", "Pipe sprinklers or drip lines to a well or pump."},
            {"Refrigerated storage", "A powered cold store nearly stops spoilage."},
            {"Run a biogas digester", "Turn manure into power and compost."},
            {"Generate 5 kW of clean power", "Scale up wheels, panels and digesters on your battery network."},
            {"A working farm: 5 workers", "Every worker needs a bed, food and water."},
            {"Self-sustaining", "With 3+ workers, produce more food than everyone eats in a day."},
        };
    }
}

void updateGoals() {
    initGoals();
    int workers = 0, tameAny = 0, tameDog = 0;
    for (auto& e : g->ents) {
        if (!e.alive) continue;
        if (e.kind == E_WORKER) workers++;
        if (e.tame) { tameAny++; if (e.kind == E_DOG) tameDog++; }
    }
    bool bedIndoor = false, anyCrop = false;
    for (int i = 0; i < WW * WH; i++) {
        if (g->tiles[i].obj == O_BED && g->indoor[i]) bedIndoor = true;
        if (g->crops[i].type) anyCrop = true;
    }
    bool road = false;
    for (auto& t : g->tribes) road |= t.road;
    bool checks[] = {
        countItem(g->pl.inv, INV_SLOTS, I_AXE) > 0 || g->goals[0].done,
        anyObj(O_CAMPFIRE),
        bedIndoor,
        anyCrop || g->goals[3].done,
        tameAny > 0,
        anyFruit(),
        g->stats.usedGranary,
        tameDog > 0,
        anyObj(O_WELL),
        g->stats.composted > 0,
        workers > 0,
        road,
        g->day >= 28,
        g->powerGen > 0.05f,
        anyBees(),
        anyIrrigating(),
        anyPowered(O_FRIDGE),
        anyBiogas,
        g->powerGen >= 5,
        workers >= 5,
        workers >= 3 && g->day >= DAYS_PER_SEASON && g->stats.lastFoodMade > g->stats.lastFoodEaten && g->stats.lastFoodEaten > 0,
    };
    for (size_t k = 0; k < g->goals.size() && k < sizeof(checks) / sizeof(checks[0]); k++) {
        if (!g->goals[k].done && checks[k]) {
            g->goals[k].done = true;
            logMsg(std::string("Milestone: ") + g->goals[k].title, Color{255, 220, 100, 255});
        }
    }
}

// ---------------------------------------------------------------- main sim tick
static float goalTimer = 0, slowTimer = 0, roadTimer = 0;
static bool duskDone = false, dawnDone = false;

void simUpdate(float dt) {
    float daysPerSec = 1.f / DAY_SECONDS;
    float prevTod = g->tod;
    g->tod += dt * daysPerSec;
    g->totalTime += dt;
    if (g->tod >= 1.f) { g->tod -= 1.f; newDay(); duskDone = dawnDone = false; }
    if (!dawnDone && prevTod < 0.27f && g->tod >= 0.27f) { dawnDone = true; dawn(); }
    if (!duskDone && prevTod < 0.80f && g->tod >= 0.80f) { duskDone = true; spawnWolfPack(); spawnCoyotes(); }
    // raids launch at dusk
    if (prevTod < 0.76f && g->tod >= 0.76f)
        for (size_t k = 0; k < g->tribes.size(); k++) {
            Tribe& t = g->tribes[k];
            if (t.hostile && g->day >= t.nextRaid && countStructures() >= 6) {
                spawnRaid((int)k);
                t.nextRaid = g->day + rndi(6, 10) - std::min(3, g->day / 20);
                t.raidWarned = false;
            }
        }

    // weather
    g->weatherTimer -= dt;
    if (g->weatherTimer <= 0) pickWeather();
    if (raining()) g->dryDays = 0;
    if (g->tod < prevTod || (int)(prevTod * 1) != (int)(g->tod * 1)) {}
    static float dryAcc = 0;
    dryAcc += dt;
    if (dryAcc > DAY_SECONDS) { dryAcc = 0; if (!raining()) g->dryDays++; }
    if (raining() && g->drought) { g->drought = false; logMsg("Rain breaks the drought.", Color{140, 200, 255, 255}); }
    float air = airTemp();
    if ((g->weather == WX_SNOW || g->weather == WX_BLIZZARD) && air < 1) g->snowCover = std::min(1.f, g->snowCover + dt * 0.01f);
    else if (air > 2) g->snowCover = std::max(0.f, g->snowCover - dt * 0.004f * (air - 1));
    if (g->weather == WX_STORM) {
        g->stormYesterday = true;
        g->lightningTimer -= dt;
        if (g->lightningTimer <= 0) {
            g->lightningTimer = rndr(15, 40);
            g->flash = 1.f;
            g->shake = 0.4f;
            int x = (int)g->pl.x + rndi(-30, 30), y = (int)g->pl.y + rndi(-22, 22);
            if (inb(x, y)) {
                uint8_t o = g->tiles[IDX(x, y)].obj;
                float ch = g->drought ? 0.7f : (g->season() == 1 ? 0.35f : 0.12f);
                if (isTreeObj(o) && rnd() < ch) {
                    startFire(x, y);
                    logMsg("Lightning struck a tree - FIRE!", Color{255, 130, 60, 255});
                }
            }
        }
    }
    g->flash = std::max(0.f, g->flash - dt * 3);

    // continuous soil effects
    slowTimer += dt;
    if (slowTimer >= 1.f) {
        float sdt = slowTimer;
        slowTimer = 0;
        float sdtDays = sdt * daysPerSec;
        if (raining()) for (int i = 0; i < WW * WH; i++) if (!g->indoor[i] && g->ground[i] != G_GREENHOUSE) g->moist[i] = std::min(1.f, g->moist[i] + 0.012f * sdt);
        if (g->weather == WX_SNOW && g->snowCover > 0.3f) for (int i = 0; i < WW * WH; i++) g->moist[i] = std::max(g->moist[i], 0.4f);
        updateCrops(sdtDays);
        updateSpoilage(sdtDays);
        updateProcessors(sdtDays);
        updatePower(sdt);
        updateWater(sdt);
        // bushes regrow berries in summer/autumn
        for (auto& t : g->tiles)
            if (t.obj == O_BUSH && t.var >= 4 && g->season() != 3) { t.timer += sdtDays; if (t.timer > 2.5f) { t.var -= 4; t.timer = 0; } }
    }
    if (g->indoorDirty) computeIndoor();
    updateFire(dt);
    goalTimer += dt;
    if (goalTimer > 1.5f) { goalTimer = 0; updateGoals(); }
    roadTimer += dt;
    if (roadTimer > 5.f) { roadTimer = 0; checkRoads(); }

    // particles
    for (auto& p : g->parts) {
        p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
        if (p.kind == 0) p.vy += 4 * dt;
    }
    g->parts.erase(std::remove_if(g->parts.begin(), g->parts.end(), [](const Particle& p) { return p.life <= 0; }), g->parts.end());
    for (auto& f : g->floats) { f.y -= dt * 0.8f; f.life -= dt; }
    g->floats.erase(std::remove_if(g->floats.begin(), g->floats.end(), [](const Floater& f) { return f.life <= 0; }), g->floats.end());
    for (auto& l : g->log) l.t += dt;
}

// ---------------------------------------------------------------- dams
// A dam backs the river up into a reservoir: low land upstream is drowned for good.
// The river is carved top to bottom, so upstream means smaller y.
int fillReservoir(int dx, int dy) {
    int n = 0, lost = 0;
    float damH = g->height[IDX(dx, dy)];
    for (int y = dy - 12; y < dy; y++)
        for (int x = dx - 8; x <= dx + 8; x++) {
            if (!inb(x, y) || isWater(x, y)) continue;
            int i = IDX(x, y);
            if (dist(x, y, dx, dy) > 12 || g->height[i] > std::max(damH, 0.42f) + 0.06f) continue;
            bool nearRiver = false;
            for (int k = -3; k <= 3 && !nearRiver; k++)
                for (int j = -3; j <= 3; j++)
                    if (inb(x + j, y + k) && g->river[IDX(x + j, y + k)]) { nearRiver = true; break; }
            if (!nearRiver) continue;
            uint8_t o = g->tiles[i].obj;
            if (o && (o == O_TENT || o == O_HUT || o == O_BANNER || o == O_TOTEM || o == O_SKULLPOLE || o == O_BONFIRE)) continue;
            if (o) { if (OBJS[o].structure) lost++; removeObject(x, y, false); }
            if (g->crops[i].type) { g->crops[i] = Crop{}; g->stats.cropsLost++; lost++; }
            g->ground[i] = G_WATER;
            g->reservoir[i] = 1;
            g->moist[i] = 1;
            n++;
        }
    if (n) logMsg("The dam's reservoir flooded " + std::to_string(n) + " tiles upstream" + (lost ? " - drowning " + std::to_string(lost) + " of your crops/buildings." : "."),
                  Color{120, 170, 255, 255});
    return n;
}

void drainReservoir(int dx, int dy) {
    int n = 0;
    for (int y = dy - 12; y < dy; y++)
        for (int x = dx - 8; x <= dx + 8; x++) {
            if (!inb(x, y)) continue;
            int i = IDX(x, y);
            if (!g->reservoir[i]) continue;
            // keep water that another dam still holds back
            bool held = false;
            for (int yy = y + 1; yy <= y + 12 && !held; yy++)
                for (int xx = x - 8; xx <= x + 8; xx++)
                    if (inb(xx, yy) && !(xx == dx && yy == dy) && g->tiles[IDX(xx, yy)].obj == O_DAM && dist(x, y, xx, yy) <= 12) { held = true; break; }
            if (held) continue;
            g->reservoir[i] = 0;
            g->ground[i] = G_DIRT;   // rich silt left behind
            addNutrients(i, 0.3f, 0.3f, 0.3f);
            n++;
        }
    if (n) logMsg("The reservoir drains, leaving " + std::to_string(n) + " tiles of fertile silt.", Color{170, 140, 100, 255});
}

// ---------------------------------------------------------------- beauty & morale
static float beautyCache = 0, beautyT = -100;

static bool nearBuilding(int i) {
    int x = i % WW, y = i / WW;
    for (int yy = y - 4; yy <= y + 4; yy++)
        for (int xx = x - 4; xx <= x + 4; xx++)
            if (inb(xx, yy)) { uint8_t o = g->tiles[IDX(xx, yy)].obj; if (o && OBJS[o].structure && !isFruitPlant(o) && o != O_FLOWERS) return true; }
    return false;
}

static float tileBeauty(int i) {
    const Tile& t = g->tiles[i];
    float b = 0;
    int s = g->season();
    if (isTreeObj(t.obj) && !isFruitTree(t.obj) && !nearBuilding(i)) return 0;   // wild woods aren't landscaping
    switch (t.obj) {
        case O_MAPLE: case O_MTNASH: case O_SERVICEBERRY: b = 3; break;
        case O_ASPEN: case O_OAK: case O_BIRCH: b = 2; break;
        case O_PINE: b = 1; break;
        case O_POND: b = 4; break;
        case O_FLOWERS: b = s <= 1 ? 2.5f : (s == 2 ? 1.f : 0.3f); break;
        case O_WATERFALL: b = irrigating(i % WW, i / WW) ? 8.f : 1.f; break;
        case O_BEEHIVE: b = 1; break;
        case O_LAMP: case O_TORCHPOST: case O_BIRDHOUSE: b = 0.5f + (t.obj == O_BIRDHOUSE ? 0.5f : 0); break;
        case O_APPLE: case O_PEAR: case O_APRICOT: case O_GRAPEVINE: b = 2; break;
        case O_MANURE: b = -1.5f; break;
        case O_ASH: b = -2; break;
        case O_STUMP: b = -1; break;
        case O_SPRINKLER: case O_DRIP: if (t.var & VAR_BROKEN) b = -2; break;
        default: break;
    }
    if (g->crops[i].type && g->crops[i].dead) b -= 1;
    if (g->ground[i] == G_BURNT) b -= 0.5f;
    return b;
}

float farmBeauty() {
    if (g->totalTime - beautyT < 3 && beautyT <= g->totalTime) return beautyCache;
    beautyT = g->totalTime;
    float bx, by;
    if (!baseCentre(bx, by)) { beautyCache = 0; return 0; }
    float sum = 0;
    for (int y = (int)by - 16; y <= (int)by + 16; y++)
        for (int x = (int)bx - 16; x <= (int)bx + 16; x++)
            if (inb(x, y)) sum += tileBeauty(IDX(x, y));
    beautyCache = clampf(sum, -40, 100);
    return beautyCache;
}

// How hard a worker works: a pretty farm and good morale speed everything up.
float workRate(const Ent& e) {
    float r = 1.f + clampf(farmBeauty(), -20, 60) / 150.f;
    if (e.mood >= 70) r *= 1.05f;
    else if (e.mood < 35) r *= 0.85f;
    return r;
}

int saplingFor(int o) {
    switch (o) {
        case O_MAPLE: return I_SAP_MAPLE;
        case O_MTNASH: return I_SAP_ASH;
        case O_ASPEN: return I_SAP_ASPEN;
        case O_SERVICEBERRY: return I_SAP_SERVICE;
        default: return I_SAPLING;
    }
}
int treeForSapling(int item) {
    switch (item) {
        case I_APPLESEED: case I_SAP_PEAR: case I_SAP_APRICOT: return 5;
        case I_SAP_MAPLE: return 1;
        case I_SAP_ASH: return 2;
        case I_SAP_ASPEN: return 3;
        case I_SAP_SERVICE: return 4;
        default: return 0;
    }
}

// ---------------------------------------------------------------- orchard
static const char* FRUIT_COLORS[] = {"green", "red", "yellow", "russet", "striped red"};

int newGene(int kind, bool seedling) {
    FruitGenes gn;
    gn.kind = (uint8_t)kind;
    if (seedling) {
        // apples don't come true from seed: most seedlings are small and sour
        gn.size = rnd();
        gn.nutrition = std::pow(rnd(), 1.6f);
        gn.color = (uint8_t)rndi(0, 4);
    } else {
        gn.size = rndr(0.5f, 0.7f); gn.nutrition = rndr(0.55f, 0.75f); gn.color = kind == FK_APPLE ? 1 : 2;
    }
    g->genes.push_back(gn);
    return (int)g->genes.size() - 1;
}

std::string geneDesc(int id) {
    if (id < 0 || id >= (int)g->genes.size()) return "unknown variety";
    const FruitGenes& gn = g->genes[id];
    static const char* kinds[] = {"apple", "pear", "apricot"};
    const char* size = gn.size < 0.2f ? "tiny" : gn.size < 0.4f ? "small" : gn.size < 0.65f ? "medium" : gn.size < 0.85f ? "large" : "huge";
    const char* taste = gn.nutrition < 0.33f ? "sour spitter" : gn.nutrition < 0.75f ? "decent" : "sweet, nourishing";
    return TextFormat("%s %s %s, %s (#%d)", size, gn.kind == FK_APPLE ? FRUIT_COLORS[gn.color % 5] : "", kinds[gn.kind % 3], taste, id);
}

Color fruitColor(int i) {
    switch (g->tiles[i].obj) {
        case O_APPLE: {
            int gid = g->fruitGene[i];
            int c = gid >= 0 && gid < (int)g->genes.size() ? g->genes[gid].color : 1;
            static const Color cols[] = {{140, 200, 70, 255}, {210, 40, 40, 255}, {240, 210, 60, 255}, {170, 120, 60, 255}, {220, 80, 60, 255}};
            return cols[c % 5];
        }
        case O_PEAR: return Color{200, 210, 80, 255};
        case O_APRICOT: return Color{245, 150, 50, 255};
        case O_GRAPEVINE: return Color{110, 50, 130, 255};
        case O_BLACKBERRY: return Color{40, 20, 40, 255};
        default: return Color{220, 40, 70, 255};
    }
}

bool harvestFruit(int x, int y, bool byWorker, float wx, float wy) {
    int i = IDX(x, y);
    uint8_t o = g->tiles[i].obj;
    if (!isFruitPlant(o) || g->fruitState[i] != 1) return false;
    g->fruitState[i] = 2;
    int id = I_BERRIES, n = 3;
    int gid = g->fruitGene[i];
    FruitGenes gn = gid >= 0 && gid < (int)g->genes.size() ? g->genes[gid] : FruitGenes{};
    switch (o) {
        case O_APPLE:
            id = gn.nutrition < 0.33f ? I_APPLE_SPITTER : gn.nutrition < 0.75f ? I_APPLE : I_APPLE_PRIZE;
            n = (int)std::round(3 + gn.size * 7);
            break;
        case O_PEAR: id = I_PEAR; n = (int)std::round(3 + gn.size * 5); break;
        case O_APRICOT: id = I_APRICOT; n = (int)std::round(3 + gn.size * 5); break;
        case O_GRAPEVINE: id = I_GRAPES; n = 4; break;
        case O_BLACKBERRY: id = I_BLACKBERRY; n = 4; break;
        case O_RASPBERRY: id = I_RASPBERRY; n = 3; break;
    }
    if (beesNear(x, y)) n = (int)std::round(n * 1.35f);
    if (o == O_APPLE && rnd() < 0.5f) { if (byWorker) storeAnywhere(I_APPLESEED, 2, 1, wx, wy); else addToPlayer(I_APPLESEED, 2); }
    if (byWorker) { if (!storeAnywhere(id, n, 1, wx, wy)) warnStorageFull(ITEMS[id].name); }
    else { addToPlayer(id, n); floatText(x + 0.5f, y - 1, "+" + std::to_string(n) + " " + ITEMS[id].name, Color{255, 200, 140, 255}); }
    g->stats.foodMade += ITEMS[id].food * n;
    burst(x + 0.5f, y, fruitColor(i), 5);
    return true;
}
