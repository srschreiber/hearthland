// World simulation: time, seasons, weather, temperature, farming, pests, spoilage,
// fire, power, natural disasters, tribes and goals.
#include "game.h"
#include <algorithm>

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
        case C_FRIDGE: return food ? (g->powered ? 4.f : 0.5f) : 0.2f;
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
float sunLevel() {
    // sunrise ~0.25, sunset ~0.8; winter days shorter
    int s = g->season();
    float rise = s == 3 ? 0.30f : (s == 1 ? 0.22f : 0.26f);
    float set = s == 3 ? 0.74f : (s == 1 ? 0.84f : 0.79f);
    float t = g->tod;
    if (t < rise - 0.04f || t > set + 0.04f) return 0;
    if (t < rise + 0.04f) return (t - (rise - 0.04f)) / 0.08f;
    if (t > set - 0.04f) return 1 - (t - (set - 0.04f)) / 0.08f;
    return 1;
}

static float tempAtTime(int day, float tod, float offset) {
    static const float base[4] = {12, 24, 13, -8};
    int s = (day / DAYS_PER_SEASON) % 4;
    float f = ((day % DAYS_PER_SEASON) + tod) / DAYS_PER_SEASON;
    f = f * f * (3 - 2 * f);
    float t = base[s] + (base[(s + 1) % 4] - base[s]) * f * 0.7f;
    t -= std::cos((tod - 0.12f) * 6.2832f) * 6.f;
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
    if (g->ground[IDX(x, y)] == G_GREENHOUSE) t += 16;
    float bonus = 0;
    for (int yy = y - 5; yy <= y + 5; yy++)
        for (int xx = x - 5; xx <= x + 5; xx++) {
            if (!inb(xx, yy)) continue;
            uint8_t o = g->tiles[IDX(xx, yy)].obj;
            float d = dist(x, y, xx, yy);
            if (o == O_CAMPFIRE || o == O_BONFIRE) bonus = std::max(bonus, 20.f * clampf(1 - d / 4.5f, 0, 1));
            else if (o == O_HEATER && g->powered) bonus = std::max(bonus, (g->indoor[IDX(xx, yy)] ? 26.f : 14.f) * clampf(1 - d / 6.f, 0, 1));
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
    if (s == 0) w = r < 0.35f ? WX_CLEAR : r < 0.6f ? WX_CLOUDY : r < 0.88f ? WX_RAIN : r < 0.94f ? WX_STORM : WX_FOG;
    else if (s == 1) w = r < 0.55f ? WX_CLEAR : r < 0.72f ? WX_CLOUDY : r < 0.85f ? WX_RAIN : WX_STORM;
    else if (s == 2) w = r < 0.3f ? WX_CLEAR : r < 0.58f ? WX_CLOUDY : r < 0.86f ? WX_RAIN : WX_FOG;
    else w = r < 0.3f ? WX_CLEAR : r < 0.6f ? WX_CLOUDY : r < 0.95f ? WX_SNOW : WX_BLIZZARD;
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
            float burnTime = (t.obj == O_OAK || t.obj == O_PINE || t.obj == O_BIRCH) ? 9.f : (t.obj && OBJS[t.obj].structure ? 7.f : 3.f);
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
                    if (o == O_OAK || o == O_PINE || o == O_BIRCH) chance *= 1.6f;
                    if (o == O_PINE) chance *= 1.3f;
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

// ---------------------------------------------------------------- crops
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
    float f = g->fert[i];
    int n = std::max(1, (int)std::round(d.yield * (0.35f + f * 0.9f) * (1 - c.pest * 0.6f)));
    int seeds = c.type == CR_WHEAT ? 2 : 0;
    auto give = [&](int id, int cnt) {
        if (cnt <= 0 || !id) return;
        if (byWorker) storeAnywhere(id, cnt, 1.f, wx, wy);
        else addToPlayer(id, cnt);
        if (isFood(id)) g->stats.foodMade += ITEMS[id].food * cnt;
    };
    give(d.produce, n);
    give(d.extra, d.extraN);
    give(I_WHEATSEED, seeds);
    if (!byWorker) floatText(x + 0.5f, y, "+" + std::to_string(n) + " " + ITEMS[d.produce].name, Color{200, 255, 150, 255});
    g->fert[i] = clampf(g->fert[i] - d.drain, 0.05f, 1.4f);
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
        }
    for (auto& e : g->ents)
        if (e.alive && e.tame && (e.kind == E_DUCK || e.kind == E_FOWL)) addRadius((int)e.x, (int)e.y, 5, 0.25f);
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
        float t = air + (gh ? 16.f : 0.f);
        if (frost && !gh && !CROPS[c.type].hardy) { c.dead = true; killed++; continue; }
        if (c.growth >= 1.f) continue;
        float rate = 1.f / CROPS[c.type].days;
        float tf = t < 3 ? 0.f : (t < 11 ? 0.5f : (t > 34 ? 0.4f : 1.f));
        if (CROPS[c.type].hardy && t >= 0 && t < 11) tf = 0.8f;
        float mf = g->moist[i] > 0.22f ? 1.f : 0.12f;
        float pf = c.pest > 0.6f ? 0.5f : 1.f;
        c.growth = std::min(1.f, c.growth + rate * tf * mf * pf * dtDays);
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
            case C_FRIDGE: m = g->powered ? 0.04f : tf * 1.1f; break;
            case C_TROUGH: m *= indoor ? 1.f : (raining() ? 2.f : 1.2f); break;
            default: continue;
        }
        if (g->flooded[i]) m *= 4.f;
        spoilStacks(c.slots.data(), (int)c.slots.size(), m, dtDays, true);
    }
}

// ---------------------------------------------------------------- compost & biogas
static float biogasOutput = 0;
static void updateProcessors(float dtDays) {
    biogasOutput = 0;
    for (auto& c : g->conts) {
        if (!c.alive) continue;
        if (c.kind == C_COMPOST || c.kind == C_BIOGAS) {
            Tile& t = g->tiles[IDX(c.x, c.y)];
            int inputs = countItem(c.slots.data(), (int)c.slots.size(), I_MANURE) + countItem(c.slots.data(), (int)c.slots.size(), I_LEAVES) +
                         countItem(c.slots.data(), (int)c.slots.size(), I_MOLDY);
            if (c.kind == C_BIOGAS && countItem(c.slots.data(), (int)c.slots.size(), I_MANURE) > 0) biogasOutput += 7.f;
            if (inputs >= 2) {
                t.timer += dtDays;
                float need = c.kind == C_BIOGAS ? 0.12f : 0.2f;
                if (t.timer >= need) {
                    t.timer = 0;
                    int took = 0;
                    for (int id : {I_MANURE, I_MOLDY, I_LEAVES})
                        while (took < 2 && takeItem(c.slots.data(), (int)c.slots.size(), id, 1)) took++;
                    // finished compost goes to storage where farmers can find it
                    if (!storeAnywhere(I_COMPOST, 1, 1, c.x, c.y)) addItem(c.slots.data(), (int)c.slots.size(), I_COMPOST, 1);
                    g->stats.composted++;
                }
            }
        }
    }
}

// ---------------------------------------------------------------- power
static void updatePower(float dt) {
    float gen = 0, use = 0, cap = 0;
    int s = g->season();
    float sun = sunLevel() * (s == 0 ? 0.9f : s == 1 ? 1.f : s == 2 ? 0.7f : 0.45f);
    switch (g->weather) {
        case WX_CLOUDY: case WX_FOG: sun *= 0.5f; break;
        case WX_RAIN: case WX_SNOW: sun *= 0.3f; break;
        case WX_STORM: sun *= 0.2f; break;
        case WX_BLIZZARD: sun *= 0.1f; break;
        default: break;
    }
    if (g->snowCover > 0.6f) sun *= 0.7f;
    bool night = sunLevel() < 0.3f;
    bool cold = airTemp() < 16;
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (!o) continue;
            switch (o) {
                case O_WHEEL: {
                    bool dam = false, water = false;
                    for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++) {
                        if (!inb(x + j, y + k)) continue;
                        if (g->tiles[IDX(x + j, y + k)].obj == O_DAM) dam = true;
                        if (g->river[IDX(x + j, y + k)] && isWater(x + j, y + k)) water = true;
                    }
                    if (dam && water) gen += airTemp() < -6 ? 3.f : 10.f;
                    else if (water) gen += 2.f;
                    break;
                }
                case O_SOLAR: gen += 6.f * sun; break;
                case O_BATTERY: cap += 2000; break;
                case O_LAMP: if (night) use += 1; break;
                case O_HEATER: if (cold) use += 4; break;
                case O_SPRINKLER: use += 1.5f; break;
                case O_FRIDGE: use += 2; break;
                default: break;
            }
        }
    gen += biogasOutput;
    g->powerGen = gen; g->powerUse = use; g->powerCap = cap;
    float net = gen - use;
    g->powerStored = clampf(g->powerStored + net * dt, 0, cap);
    g->powered = use > 0 && (net >= 0 || g->powerStored > 0);
}

static void updateSprinklers(float dt) {
    if (!g->powered) return;
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            if (g->tiles[IDX(x, y)].obj != O_SPRINKLER) continue;
            for (int yy = y - 3; yy <= y + 3; yy++)
                for (int xx = x - 3; xx <= x + 3; xx++)
                    if (inb(xx, yy)) g->moist[IDX(xx, yy)] = std::min(1.f, g->moist[IDX(xx, yy)] + 0.05f * dt);
            if (rnd() < dt * 4) {
                float a = rndr(0, 6.283f);
                g->parts.push_back({x + 0.5f, y + 0.4f, std::cos(a) * 2.f, std::sin(a) * 2.f - 1, 0.5f, 0.5f, Color{140, 200, 255, 200}, 1.5f, 3});
            }
        }
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

static void dawn() {
    g->stats.nightsSurvived++;
    for (auto& e : g->ents) if (e.alive && e.kind == E_WOLF) e.state = 2;  // retreat
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
    if (dos == 0) {
        logMsg(std::string("~ ") + SEASON_NAMES[s] + " of year " + std::to_string(g->year()) + " begins ~", Color{255, 230, 150, 255});
        if (s == 0) {
            g->frostHappened = false; g->frostWarned = false;
            if (g->day > 1) g->stats.wintersSurvived++;
        }
    }
    // soil
    float evap = s == 1 ? 0.3f : (s == 3 ? 0.08f : 0.18f);
    if (g->drought) evap *= 1.8f;
    if (g->heatwave) evap *= 1.5f;
    for (int i = 0; i < WW * WH; i++) {
        if (g->ground[i] != G_GREENHOUSE || !g->indoor[i]) g->moist[i] = clampf(g->moist[i] - evap, 0, 1);
        if (g->ground[i] == G_TILLED && !g->crops[i].type && rnd() < 0.04f) g->fert[i] = std::min(0.6f, g->fert[i] + 0.02f);
        if (g->ground[i] == G_BURNT && rnd() < 0.12f && s != 3) g->ground[i] = G_GRASS;
    }
    // vegetation
    for (int y = 1; y < WH - 1; y++)
        for (int x = 1; x < WW - 1; x++) {
            int i = IDX(x, y);
            Tile& t = g->tiles[i];
            if (t.obj == O_SAPLING) { t.timer += 1; if (t.timer >= 6 && s != 3) { removeObject(x, y, false); placeObject(x, y, rnd() < 0.5f ? O_OAK : O_PINE); } }
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
            if (g->flooded[i]) { g->ground[i] = g->origGround[i]; g->flooded[i] = 0; g->moist[i] = 1; }
        g->floodActive = false;
        logMsg("The flood waters recede.", Color{150, 200, 255, 255});
    }
    // disasters
    g->heatwave = false;
    if (s == 1 && g->dryDays >= 3 && !g->drought) { g->drought = true; logMsg("DROUGHT: no rain for days. Soil dries fast and fire spreads easily.", Color{255, 190, 90, 255}); }
    if (s == 1 && rnd() < 0.2f) { g->heatwave = true; logMsg("HEATWAVE: scorching day ahead. Drink water and stay in shade.", Color{255, 150, 60, 255}); }
    if (s == 1 && rnd() < 0.15f) {
        logMsg("A swarm of insects descends on the fields!", Color{230, 200, 90, 255});
        for (auto& c : g->crops) if (c.type && !c.dead) c.pest = std::min(1.f, c.pest + 0.35f);
    }
    if (s == 3 && rnd() < 0.25f) { g->weather = WX_BLIZZARD; g->weatherTimer = 150; logMsg("BLIZZARD! Extreme cold today.", Color{200, 230, 255, 255}); }
    if (s == 0 && dos == 1 && g->day > DAYS_PER_SEASON * 4 && rnd() < 0.55f) {
        int dams = 0;
        for (auto& t : g->tiles) if (t.obj == O_DAM) dams++;
        if (dams > 0) logMsg("Snowmelt swells the river - your dam holds back the flood.", Color{150, 200, 255, 255});
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
            if (n) logMsg("FLOOD! Spring snowmelt has flooded the riverbanks. A dam would control it.", Color{120, 170, 255, 255});
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
    dailyCrops();
    tribeDaily();
    spawnWildlife(false);
    // daily breeding & animal produce of tame animals handled in entities; workers mood
    checkRoads();
}

// ---------------------------------------------------------------- goals
static bool anyObj(int o) { for (auto& t : g->tiles) if (t.obj == o) return true; return false; }

void initGoals() {
    if (g->goals.empty()) {
        g->goals = {
            {"Craft a stone axe", "Gather sticks, flint and fiber, then open crafting (Tab)."},
            {"Light a campfire", "Fire keeps you warm, cooks food and scares wolves."},
            {"Build a shelter", "Enclose a bed with walls and a door. Indoors is warmer."},
            {"Plant a crop", "Till grass with a hoe, then plant seeds. Wild seeds come from tall grass."},
            {"Tame an animal", "Hold food an animal likes and right-click it repeatedly."},
            {"Dress for winter", "Wear a fur coat or better. Cold kills."},
            {"Store food properly", "Granaries and root cellars keep food from rotting."},
            {"Get a guard dog", "Tame a dog with meat. Dogs fight off wolves."},
            {"Dig a well", "Workers need water. A well provides it."},
            {"Start composting", "Put manure, leaves or moldy food in a compost bin."},
            {"Recruit a worker", "Trade with a friendly village. Have a free bed ready."},
            {"Open a trade route", "Build road from a village to a trading post."},
            {"Survive the first winter", "Hay for livestock, warm clothes, stored food."},
            {"Generate electricity", "Water wheel by a dam, or solar panels."},
            {"Grow food under glass", "Greenhouse frames on tilled soil grow year-round."},
            {"Automate irrigation", "Powered sprinklers water crops for you."},
            {"Refrigerated storage", "A powered cold store nearly stops spoilage."},
            {"Run a biogas digester", "Turn manure into power and compost."},
            {"Generate 40W of clean power", "Scale up wheels, panels and digesters."},
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
    float insul = 0;
    for (auto& w : g->pl.wear) if (w.id) insul += ITEMS[w.id].insul;
    bool bedIndoor = false, ghCrop = false, anyCrop = false;
    for (int i = 0; i < WW * WH; i++) {
        if (g->tiles[i].obj == O_BED && g->indoor[i]) bedIndoor = true;
        if (g->crops[i].type) { anyCrop = true; if (g->ground[i] == G_GREENHOUSE) ghCrop = true; }
    }
    bool road = false;
    for (auto& t : g->tribes) road |= t.road;
    bool checks[] = {
        countItem(g->pl.inv, INV_SLOTS, I_AXE) > 0 || g->goals[0].done,
        anyObj(O_CAMPFIRE),
        bedIndoor,
        anyCrop || g->goals[3].done,
        tameAny > 0,
        insul >= 10,
        g->stats.usedGranary,
        tameDog > 0,
        anyObj(O_WELL),
        g->stats.composted > 0,
        workers > 0,
        road,
        g->stats.wintersSurvived > 0,
        g->powerGen > 0.5f,
        ghCrop,
        anyObj(O_SPRINKLER) && g->powered,
        anyObj(O_FRIDGE) && g->powered,
        biogasOutput > 0,
        g->powerGen >= 40,
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
    if (!duskDone && prevTod < 0.80f && g->tod >= 0.80f) { duskDone = true; spawnWolfPack(); }
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
        g->lightningTimer -= dt;
        if (g->lightningTimer <= 0) {
            g->lightningTimer = rndr(15, 40);
            g->flash = 1.f;
            g->shake = 0.4f;
            int x = (int)g->pl.x + rndi(-30, 30), y = (int)g->pl.y + rndi(-22, 22);
            if (inb(x, y)) {
                uint8_t o = g->tiles[IDX(x, y)].obj;
                float ch = g->drought ? 0.7f : (g->season() == 1 ? 0.35f : 0.12f);
                if ((o == O_OAK || o == O_PINE || o == O_BIRCH) && rnd() < ch) {
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
        updateSprinklers(sdt);
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
            g->fert[i] = std::min(1.2f, g->fert[i] + 0.4f);
            n++;
        }
    if (n) logMsg("The reservoir drains, leaving " + std::to_string(n) + " tiles of fertile silt.", Color{170, 140, 100, 255});
}
