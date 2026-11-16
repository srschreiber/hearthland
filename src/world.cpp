// World generation, tile queries, indoor detection and pathfinding.
#include "game.h"
#include <queue>
#include <algorithm>

Game* g = nullptr;

// ---------------------------------------------------------------- rng / noise
static uint32_t rngState = 0x12345678u;
uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
float rnd() {
    rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
    return (rngState & 0xFFFFFF) / float(0x1000000);
}
float rndr(float a, float b) { return a + (b - a) * rnd(); }
int rndi(int a, int b) { return a + int(rnd() * (b - a + 1)) % (b - a + 1); }

static float lattice(int x, int y, uint32_t seed) {
    return (hash32(uint32_t(x) * 73856093u ^ uint32_t(y) * 19349663u ^ seed * 83492791u) & 0xFFFF) / 65535.f;
}
float noise2(float x, float y, uint32_t seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    float a = lattice(xi, yi, seed), b = lattice(xi + 1, yi, seed);
    float c = lattice(xi, yi + 1, seed), d = lattice(xi + 1, yi + 1, seed);
    return (a + (b - a) * u) * (1 - v) + (c + (d - c) * u) * v;
}
float fbm(float x, float y, uint32_t seed, int oct) {
    float s = 0, amp = 0.5f, tot = 0;
    for (int i = 0; i < oct; i++) {
        s += noise2(x, y, seed + i * 101) * amp;
        tot += amp; amp *= 0.5f; x *= 2.03f; y *= 2.03f;
    }
    return s / tot;
}

// ---------------------------------------------------------------- queries
bool isWater(int x, int y) {
    if (!inb(x, y)) return false;
    uint8_t gr = g->ground[IDX(x, y)];
    return gr == G_WATER || gr == G_DEEP;
}

bool solidFor(int x, int y, int who) {
    if (!inb(x, y)) return true;
    int i = IDX(x, y);
    if (g->ground[i] == G_DEEP) return true;
    uint8_t o = g->tiles[i].obj;
    if (o == O_NONE) return false;
    const ObjDef& d = OBJS[o];
    if (!d.solid) return false;
    if (d.doorlike && (who == 0 || who == 2)) return false;
    return true;
}

int newContainer(uint8_t kind, int x, int y, int slots) {
    for (size_t i = 0; i < g->conts.size(); i++) {
        if (!g->conts[i].alive) {
            g->conts[i] = Container{kind, x, y, true, std::vector<Stack>(slots)};
            return (int)i;
        }
    }
    g->conts.push_back(Container{kind, x, y, true, std::vector<Stack>(slots)});
    return (int)g->conts.size() - 1;
}

static uint8_t contKindFor(int obj) {
    switch (obj) {
        case O_GRANARY: return C_GRANARY;
        case O_CELLAR: return C_CELLAR;
        case O_FRIDGE: return C_FRIDGE;
        case O_TROUGH: return C_TROUGH;
        case O_COMPOST: return C_COMPOST;
        case O_BIOGAS: return C_BIOGAS;
        default: return C_CHEST;
    }
}

void placeObject(int x, int y, int obj) {
    if (!inb(x, y)) return;
    Tile& t = g->tiles[IDX(x, y)];
    t.obj = (uint8_t)obj;
    t.hp = (int16_t)OBJS[obj].hp;
    t.var = (uint8_t)(hash32(IDX(x, y) * 31 + obj) & 3);
    t.timer = 0;
    t.cont = -1;
    if (OBJS[obj].slots > 0) t.cont = (int16_t)newContainer(contKindFor(obj), x, y, OBJS[obj].slots);
    if (OBJS[obj].wallLike) g->indoorDirty = true;
}

void removeObject(int x, int y, bool dropContents) {
    if (!inb(x, y)) return;
    Tile& t = g->tiles[IDX(x, y)];
    if (t.cont >= 0 && t.cont < (int)g->conts.size()) {
        Container& c = g->conts[t.cont];
        if (dropContents)
            for (auto& s : c.slots) if (s.id && s.n > 0) addToPlayer(s.id, s.n, s.fresh);
        c.alive = false;
        c.slots.clear();
        if (g->openCont == t.cont) { g->openCont = -1; if (g->panel == P_CONTAINER) g->panel = P_NONE; }
    }
    if (OBJS[t.obj].wallLike) g->indoorDirty = true;
    t.obj = O_NONE; t.hp = 0; t.cont = -1; t.timer = 0;
    if (!g->fruitGene.empty()) { g->fruitGene[IDX(x, y)] = -1; g->fruitState[IDX(x, y)] = 0; }
}

Ent& spawnEnt(int kind, float x, float y) {
    // reuse dead slots so references stay stable-ish and the vector never grows unbounded
    Ent* slot = nullptr;
    for (auto& e : g->ents) if (!e.alive) { slot = &e; break; }
    if (!slot) {
        // never reallocate: callers iterate ents by reference while spawning
        if (g->ents.size() >= g->ents.capacity()) { static Ent dummy; dummy = Ent{}; dummy.alive = false; return dummy; }
        g->ents.emplace_back();
        slot = &g->ents.back();
    }
    *slot = Ent{};
    Ent& e = *slot;
    e.id = g->nextId++;
    e.kind = (uint8_t)kind;
    e.x = x; e.y = y;
    e.homeX = x; e.homeY = y;
    e.hp = e.maxhp = SPECIES[kind].hp;
    e.face = rnd() < 0.5f ? -1.f : 1.f;
    e.timer = rndr(0, 3);
    e.produce = rndr(0, 0.6f);
    e.look = (uint8_t)rndi(0, 3);
    return e;
}

Ent* entById(int id) {
    if (id <= 0) return nullptr;
    for (auto& e : g->ents) if (e.alive && e.id == id) return &e;
    return nullptr;
}

static float baseCacheT = -100;
static float baseX = 0, baseY = 0;
static bool baseOk = false;
static int baseCount = 0;
static void refreshBase() {
    if (g->totalTime - baseCacheT < 2.f && baseCacheT <= g->totalTime) return;
    baseCacheT = g->totalTime;
    double sx = 0, sy = 0; int n = 0;
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (o && OBJS[o].structure && o != O_DAM && o != O_WHEEL && o != O_TRADEPOST) { sx += x; sy += y; n++; }
        }
    baseCount = n;
    baseOk = n > 0;
    if (n) { baseX = float(sx / n) + 0.5f; baseY = float(sy / n) + 0.5f; }
}
bool baseCentre(float& bx, float& by) {
    refreshBase();
    if (!baseOk) { bx = g->pl.x; by = g->pl.y; return false; }
    bx = baseX; by = baseY;
    return true;
}
int countStructures() { refreshBase(); return baseCount; }

bool nearObj(float x, float y, int obj, float r, int* ox, int* oy) {
    int cx = (int)x, cy = (int)y, R = (int)std::ceil(r);
    float best = 1e9f;
    bool found = false;
    for (int yy = cy - R; yy <= cy + R; yy++)
        for (int xx = cx - R; xx <= cx + R; xx++) {
            if (!inb(xx, yy) || g->tiles[IDX(xx, yy)].obj != obj) continue;
            float d = dist(x, y, xx + 0.5f, yy + 0.5f);
            if (d <= r && d < best) {
                best = d; found = true;
                if (ox) *ox = xx;
                if (oy) *oy = yy;
            }
        }
    return found;
}

// ---------------------------------------------------------------- indoor detection
// A tile is indoors if it cannot be reached from the map border without crossing a wall or door.
void computeIndoor() {
    g->indoorDirty = false;
    std::vector<uint8_t> reach(WW * WH, 0);
    std::vector<int> q;
    q.reserve(WW * WH);
    auto blocks = [](int i) { uint8_t o = g->tiles[i].obj; return o && OBJS[o].wallLike; };
    for (int x = 0; x < WW; x++) {
        for (int y : {0, WH - 1}) { int i = IDX(x, y); if (!blocks(i) && !reach[i]) { reach[i] = 1; q.push_back(i); } }
    }
    for (int y = 0; y < WH; y++) {
        for (int x : {0, WW - 1}) { int i = IDX(x, y); if (!blocks(i) && !reach[i]) { reach[i] = 1; q.push_back(i); } }
    }
    for (size_t h = 0; h < q.size(); h++) {
        int i = q[h], x = i % WW, y = i / WW;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (!inb(nx, ny)) continue;
            int j = IDX(nx, ny);
            if (reach[j] || blocks(j)) continue;
            reach[j] = 1;
            q.push_back(j);
        }
    }
    for (int i = 0; i < WW * WH; i++) g->indoor[i] = (!reach[i] && !blocks(i)) ? 1 : 0;
}

// ---------------------------------------------------------------- A* pathfinding
static std::vector<float> gScore;
static std::vector<int> cameFrom;
static std::vector<uint32_t> stamp;
static uint32_t curStamp = 0;

bool findPath(int sx, int sy, int tx, int ty, int who, std::vector<PathPt>& out, int maxNodes, bool adjacentOk) {
    out.clear();
    if (!inb(sx, sy) || !inb(tx, ty)) return false;
    if (gScore.empty()) { gScore.resize(WW * WH); cameFrom.resize(WW * WH); stamp.assign(WW * WH, 0); }
    curStamp++;
    bool targetSolid = solidFor(tx, ty, who);
    using QN = std::pair<float, int>;
    std::priority_queue<QN, std::vector<QN>, std::greater<QN>> open;
    int s = IDX(sx, sy);
    stamp[s] = curStamp; gScore[s] = 0; cameFrom[s] = -1;
    auto h = [&](int x, int y) {
        float dx = std::fabs(float(x - tx)), dy = std::fabs(float(y - ty));
        return (dx + dy) + (1.414f - 2.f) * std::min(dx, dy);
    };
    open.push({h(sx, sy), s});
    int expanded = 0, endNode = -1;
    while (!open.empty() && expanded < maxNodes) {
        auto [f, cur] = open.top(); open.pop();
        int cx = cur % WW, cy = cur / WW;
        if (f - h(cx, cy) > gScore[cur] + 0.01f) continue;
        expanded++;
        if (cx == tx && cy == ty) { endNode = cur; break; }
        if (adjacentOk && targetSolid && std::abs(cx - tx) <= 1 && std::abs(cy - ty) <= 1) { endNode = cur; break; }
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (!dx && !dy) continue;
                int nx = cx + dx, ny = cy + dy;
                if (!inb(nx, ny)) continue;
                bool isTarget = (nx == tx && ny == ty);
                if (solidFor(nx, ny, who) && !(isTarget && !targetSolid)) continue;
                if (dx && dy && (solidFor(cx + dx, cy, who) || solidFor(cx, cy + dy, who))) continue;
                int n = IDX(nx, ny);
                float cost = (dx && dy) ? 1.414f : 1.f;
                if (g->ground[n] == G_WATER) cost *= 2.5f;
                if (g->ground[n] == G_ROAD) cost *= 0.7f;
                if (g->fire[n] > 0) cost += 20;
                float ng = gScore[cur] + cost;
                if (stamp[n] != curStamp || ng < gScore[n]) {
                    stamp[n] = curStamp; gScore[n] = ng; cameFrom[n] = cur;
                    open.push({ng + h(nx, ny), n});
                }
            }
    }
    if (endNode < 0) return false;
    for (int n = endNode; n != -1 && n != s; n = cameFrom[n]) out.push_back({(int16_t)(n % WW), (int16_t)(n / WW)});
    std::reverse(out.begin(), out.end());
    return true;
}

// ---------------------------------------------------------------- generation
static void carveRiver(uint32_t seed) {
    float x = WW * (0.25f + 0.5f * (hash32(seed) % 1000) / 1000.f);
    float dir = 0;
    for (int y = 0; y < WH; y++) {
        dir += (noise2(y * 0.08f, 0, seed + 7) - 0.5f) * 0.9f;
        dir = clampf(dir, -1.2f, 1.2f);
        x += dir;
        x = clampf(x, 8, WW - 9);
        int w = 2 + (noise2(y * 0.05f, 3, seed + 9) > 0.6f ? 1 : 0);
        for (int k = -w; k <= w; k++) {
            int xx = (int)x + k;
            if (!inb(xx, y)) continue;
            int i = IDX(xx, y);
            g->ground[i] = (std::abs(k) < w - 1) ? G_DEEP : G_WATER;
            if (std::abs(k) == w - 1 && w == 2) g->ground[i] = G_WATER;
            g->river[i] = 1;
        }
    }
}

static bool landAt(int x, int y) {
    if (!inb(x, y)) return false;
    uint8_t gr = g->ground[IDX(x, y)];
    return gr != G_DEEP && gr != G_WATER;
}

static void buildVillage(int ti) {
    Tribe& t = g->tribes[ti];
    int R = 6;
    for (int y = t.y - R; y <= t.y + R; y++)
        for (int x = t.x - R; x <= t.x + R; x++) {
            if (!inb(x, y)) continue;
            if (dist(x, y, t.x, t.y) > R + 0.5f) continue;
            int i = IDX(x, y);
            g->ground[i] = t.hostile ? G_BURNT : G_VILLAGE;
            g->river[i] = 0;
            removeObject(x, y, false);
        }
    placeObject(t.x, t.y, O_BONFIRE);
    const int ring[6][2] = {{-4, -3}, {0, -5}, {4, -3}, {-4, 3}, {4, 3}, {0, 5}};
    for (int k = 0; k < 6; k++) {
        int x = t.x + ring[k][0], y = t.y + ring[k][1];
        placeObject(x, y, t.hostile ? O_TENT : (k % 2 ? O_HUT : O_TENT));
        g->tiles[IDX(x, y)].var = (uint8_t)(t.hostile ? 1 : 0);
    }
    if (t.hostile) {
        placeObject(t.x - 2, t.y - 1, O_SKULLPOLE); placeObject(t.x + 2, t.y + 1, O_SKULLPOLE);
        placeObject(t.x + 2, t.y - 2, O_TOTEM); placeObject(t.x - 2, t.y + 2, O_TOTEM);
        for (int k = 0; k < 4; k++) {
            Ent& e = spawnEnt(E_RAIDER, t.x + rndr(-3, 3), t.y + rndr(-3, 3));
            e.tribe = ti; e.homeX = t.x + 0.5f; e.homeY = t.y + 0.5f; e.state = 0;
        }
    } else {
        placeObject(t.x - 1, t.y - 2, O_BANNER); placeObject(t.x + 1, t.y - 2, O_BANNER);
        g->tiles[IDX(t.x - 1, t.y - 2)].var = (uint8_t)ti;
        g->tiles[IDX(t.x + 1, t.y - 2)].var = (uint8_t)ti;
        // trader stands by the banners, villagers wander
        Ent& tr = spawnEnt(E_VILLAGER, t.x + 0.5f, t.y - 1.0f);
        tr.tribe = ti; tr.state = 1; tr.name = "Trader";
        for (int k = 0; k < 3; k++) {
            Ent& v = spawnEnt(E_VILLAGER, t.x + rndr(-3, 3), t.y + rndr(-3, 3));
            v.tribe = ti; v.state = 0;
        }
        // stub of road leading out of the village toward the player's start
        float dx = g->pl.x - t.x, dy = g->pl.y - t.y, L = std::sqrt(dx * dx + dy * dy);
        dx /= L; dy /= L;
        for (int s = 0; s <= R + 2; s++) {
            int x = (int)std::round(t.x + dx * s), y = (int)std::round(t.y + dy * s);
            if (inb(x, y) && landAt(x, y)) { g->ground[IDX(x, y)] = G_ROAD; if (g->tiles[IDX(x, y)].obj != O_BONFIRE) removeObject(x, y, false); }
        }
    }
}

void generateWorld(uint32_t seed) {
    g->seed = seed;
    rngState = hash32(seed) | 1;
    int N = WW * WH;
    g->ground.assign(N, G_GRASS);
    g->tiles.assign(N, Tile{});
    g->crops.assign(N, Crop{});
    g->moist.assign(N, 0.5f);
    g->fert.assign(N, 0.5f);
    g->phos.assign(N, 0.5f);
    g->litter.assign(N, 0.f);
    g->pota.assign(N, 0.5f);
    g->fire.assign(N, 0.f);
    g->predator.assign(N, 0.f);
    g->height.assign(N, 0.f);
    g->indoor.assign(N, 0);
    g->river.assign(N, 0);
    g->flooded.assign(N, 0);
    g->seen.assign(N, 0);
    g->reservoir.assign(N, 0);
    g->genes.clear();
    g->fruitGene.assign(N, -1);
    g->fruitState.assign(N, 0);
    g->conts.clear();
    g->ents.clear();
    g->ents.reserve(4096);
    g->tribes.clear();

    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            int i = IDX(x, y);
            float h = fbm(x / 38.f, y / 38.f, seed, 5);
            // push edges down a bit so the map is ringed by lakes
            float ex = std::min(std::min(x, WW - 1 - x), std::min(y, WH - 1 - y)) / 14.f;
            h -= (1 - clampf(ex, 0, 1)) * 0.18f;
            g->height[i] = h;
            float m = fbm(x / 26.f, y / 26.f, seed + 1000, 4);
            float r = fbm(x / 30.f, y / 30.f, seed + 2000, 4);
            uint8_t gr = G_GRASS;
            if (h < 0.30f) gr = G_DEEP;
            else if (h < 0.345f) gr = G_WATER;
            else if (h < 0.375f) gr = G_SAND;
            else if (r > 0.64f) gr = G_STONE;
            else if (m > 0.54f) gr = G_FOREST;
            else if (m < 0.36f && noise2(x / 7.f, y / 7.f, seed + 77) > 0.6f) gr = G_DIRT;
            g->ground[i] = gr;
            // soil starts fairly rich, with patches short of one nutrient or another
            float base = gr == G_FOREST ? 0.75f : (gr == G_GRASS ? 0.65f : 0.45f);
            g->fert[i] = clampf(base + (noise2(x / 9.f, y / 9.f, seed + 501) - 0.5f) * 0.4f, 0.2f, 1.f);
            g->phos[i] = clampf(base + (noise2(x / 11.f, y / 11.f, seed + 502) - 0.5f) * 0.5f, 0.2f, 1.f);
            g->pota[i] = clampf(base + (noise2(x / 10.f, y / 10.f, seed + 503) - 0.5f) * 0.5f, 0.2f, 1.f);
        }
    carveRiver(seed + 55);
    // sand banks next to deep river
    for (int y = 0; y < WH; y++)
        for (int x = 0; x < WW; x++) {
            int i = IDX(x, y);
            if (!g->river[i] && landAt(x, y)) {
                bool nearRiver = false;
                for (int k = -1; k <= 1; k++) for (int j = -1; j <= 1; j++)
                    if (inb(x + j, y + k) && g->river[IDX(x + j, y + k)]) nearRiver = true;
                if (nearRiver && rnd() < 0.5f) g->ground[i] = G_SAND;
            }
        }

    // scatter objects
    for (int y = 1; y < WH - 1; y++)
        for (int x = 1; x < WW - 1; x++) {
            int i = IDX(x, y);
            uint8_t gr = g->ground[i];
            float r = rnd();
            bool waterAdj = isWater(x + 1, y) || isWater(x - 1, y) || isWater(x, y + 1) || isWater(x, y - 1);
            if (gr == G_FOREST) {
                float pineBias = noise2(x / 20.f, y / 20.f, seed + 300);
                if (r < 0.42f) {
                    // Inland Northwest mix: pines on the dry slopes, natives in the moister woods
                    float t = rnd();
                    int o = pineBias > 0.55f ? O_PINE : t < 0.28f ? O_OAK : t < 0.42f ? O_BIRCH : t < 0.6f ? O_ASPEN : t < 0.76f ? O_MAPLE : t < 0.88f ? O_MTNASH : O_SERVICEBERRY;
                    placeObject(x, y, o);
                }
                else if (r < 0.46f) placeObject(x, y, O_BUSH);
                else if (r < 0.47f) { placeObject(x, y, rnd() < 0.5f ? O_RASPBERRY : O_BLACKBERRY); g->fruitState[i] = (uint8_t)rndi(2, 5); }
                else if (r < 0.52f) placeObject(x, y, O_BRANCH);
                else if (r < 0.56f) placeObject(x, y, O_TALLGRASS);
            } else if (gr == G_GRASS) {
                if (r < 0.003f) { placeObject(x, y, O_APPLE); g->fruitGene[i] = newGene(FK_APPLE, true); g->fruitState[i] = (uint8_t)rndi(2, 8); }   // feral apple from an old homestead
                else if (r < 0.035f) { float t = rnd(); placeObject(x, y, t < 0.35f ? O_OAK : t < 0.6f ? O_BIRCH : t < 0.8f ? O_ASPEN : O_SERVICEBERRY); }
                else if (r < 0.055f) placeObject(x, y, O_BUSH);
                else if (r < 0.19f) placeObject(x, y, O_TALLGRASS);
                else if (r < 0.205f) placeObject(x, y, O_ROCK);
                else if (r < 0.225f) placeObject(x, y, O_BRANCH);
                else if (r < 0.232f) placeObject(x, y, O_FLINT);
                else if (r < 0.236f) placeObject(x, y, O_WILDPOTATO);
                if (waterAdj && rnd() < 0.18f) { removeObject(x, y, false); placeObject(x, y, O_CLAY); }
            } else if (gr == G_STONE) {
                if (r < 0.22f) placeObject(x, y, O_ROCK);
                else if (r < 0.26f) placeObject(x, y, O_ORE);
                else if (r < 0.295f) placeObject(x, y, O_IRONORE);
                else if (r < 0.31f) placeObject(x, y, O_QUARTZ);
                else if (r < 0.33f) placeObject(x, y, O_FLINT);
                else if (r < 0.35f) placeObject(x, y, O_PINE);
            } else if (gr == G_SAND) {
                if (waterAdj && r < 0.16f) placeObject(x, y, O_CLAY);
                else if (r < 0.03f) placeObject(x, y, O_FLINT);
            } else if (gr == G_DIRT) {
                if (r < 0.04f) placeObject(x, y, O_ROCK);
                else if (r < 0.1f) placeObject(x, y, O_TALLGRASS);
            } else if (gr == G_WATER && !g->river[i]) {
                if (r < 0.12f && !isWater(x + 1, y) + !isWater(x - 1, y) + !isWater(x, y + 1) + !isWater(x, y - 1) > 0) placeObject(x, y, O_REEDS);
            }
        }

    // player spawn: a grassy spot near the middle with some forest nearby
    int bestX = WW / 2, bestY = WH / 2; float bestScore = -1e9f;
    for (int k = 0; k < 400; k++) {
        int x = rndi(WW / 2 - 35, WW / 2 + 35), y = rndi(WH / 2 - 35, WH / 2 + 35);
        if (g->ground[IDX(x, y)] != G_GRASS) continue;
        float score = 0; int forest = 0, water = 0, grass = 0;
        for (int yy = y - 10; yy <= y + 10; yy++)
            for (int xx = x - 10; xx <= x + 10; xx++) {
                if (!inb(xx, yy)) continue;
                uint8_t gr = g->ground[IDX(xx, yy)];
                forest += gr == G_FOREST; water += (gr == G_WATER || gr == G_DEEP); grass += gr == G_GRASS;
            }
        score = std::min(forest, 80) + std::min(water, 30) * 2 + std::min(grass, 200) * 0.5f - std::abs(x - WW / 2) * 0.3f;
        if (water == 0) score -= 100;
        if (score > bestScore) { bestScore = score; bestX = x; bestY = y; }
    }
    g->pl.x = bestX + 0.5f; g->pl.y = bestY + 0.5f;
    for (int y = bestY - 2; y <= bestY + 2; y++)
        for (int x = bestX - 2; x <= bestX + 2; x++) {
            uint8_t o = g->tiles[IDX(x, y)].obj;
            if (o && OBJS[o].solid) removeObject(x, y, false);
        }
    placeObject(bestX + 2, bestY - 1, O_BRANCH);
    placeObject(bestX - 2, bestY + 1, O_FLINT);
    placeObject(bestX + 1, bestY + 2, O_FLINT);

    // tribes: two friendly traders and one raider camp, spaced around the player
    struct TribeSeed { const char* name; bool hostile; Color col; };
    TribeSeed ts[3] = {{"Riverfolk", false, Color{70, 150, 220, 255}},
                       {"Hill Clan", false, Color{230, 190, 60, 255}},
                       {"Ashen Raiders", true, Color{170, 30, 30, 255}}};
    float baseAng = rndr(0, 6.283f);
    for (int k = 0; k < 3; k++) {
        Tribe t;
        t.name = ts[k].name; t.hostile = ts[k].hostile; t.col = ts[k].col;
        float distWanted = t.hostile ? 52.f : 42.f + k * 6;
        bool placed = false;
        for (int attempt = 0; attempt < 200 && !placed; attempt++) {
            float ang = baseAng + k * 2.094f + rndr(-0.5f, 0.5f);
            float dd = distWanted + rndr(-8, 12);
            int x = (int)(g->pl.x + std::cos(ang) * dd), y = (int)(g->pl.y + std::sin(ang) * dd);
            if (x < 10 || y < 10 || x > WW - 11 || y > WH - 11) continue;
            int land = 0;
            for (int yy = y - 6; yy <= y + 6; yy++) for (int xx = x - 6; xx <= x + 6; xx++) land += landAt(xx, yy);
            if (land < 150) continue;
            t.x = x; t.y = y; placed = true;
        }
        if (!placed) { t.x = (int)clampf(g->pl.x + (k - 1) * 40, 12, WW - 12); t.y = (int)clampf(g->pl.y + 40, 12, WH - 12); }
        // stock
        auto add = [&](int id, int n) { Stack s; s.id = (uint8_t)id; s.n = (int16_t)n; t.stock.push_back(s); };
        if (k == 0) { add(I_GLASS, 12); add(I_COPPER, 6); add(I_BEANS, 20); add(I_CABBAGESEED, 15); add(I_WHEATSEED, 20); add(I_CLOTH, 4); add(I_BUCKET, 2); add(I_SHEARS, 1); add(I_BREAD, 6); add(I_SAP_MAPLE, 4); add(I_SAP_ASH, 4); add(I_APPLESEED, 12); add(I_TOMATOSEED, 10); add(I_GREENBEANSEED, 15); add(I_BUCKWHEAT, 15); add(I_MUSTARDSEED, 12); add(I_SAP_PEAR, 3); add(I_GRAPECUTTING, 4); add(I_SILICON, 4); }
        else if (k == 1) { add(I_COPPER, 10); add(I_ORE, 20); add(I_LEATHER, 8); add(I_WOOL, 12); add(I_POTATO, 20); add(I_SICKLE, 1); add(I_JERKY, 10); add(I_FURCOAT, 1); add(I_BRICK, 20); add(I_SAP_ASPEN, 4); add(I_SAP_SERVICE, 4); add(I_SAP_APRICOT, 3); add(I_SQUASHSEED, 10); add(I_RASPCANE, 4); add(I_BLACKCANE, 4); add(I_IRON, 6); add(I_IRONORE, 10); add(I_TIMER, 2); }
        t.nextRaid = DAYS_PER_SEASON + rndi(2, 5);
        g->tribes.push_back(t);
    }
    for (int k = 0; k < 3; k++) buildVillage(k);

    computeIndoor();
    spawnWildlife(true);
}
