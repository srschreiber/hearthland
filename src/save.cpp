// Binary save files: magic + version header, every read bounds-checked.
// A load builds a fresh Game and only replaces the running one if the whole file parsed.
#include "game.h"
#include <cstring>
#include <type_traits>

static const char MAGIC[4] = {'H', 'R', 'T', 'H'};
static const uint32_t VERSION = 2;

struct Writer {
    std::vector<uint8_t> buf;
    template <class T> void put(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        buf.insert(buf.end(), p, p + sizeof(T));
    }
    void str(const std::string& s) { uint16_t n = (uint16_t)std::min<size_t>(s.size(), 64); put(n); buf.insert(buf.end(), s.begin(), s.begin() + n); }
    template <class T> void vec(const std::vector<T>& v) { put((uint32_t)v.size()); for (auto& x : v) put(x); }
};

struct Reader {
    const uint8_t* p; size_t n, off = 0; bool bad = false;
    template <class T> T get() {
        static_assert(std::is_trivially_copyable_v<T>);
        T v{};
        if (bad || off + sizeof(T) > n) { bad = true; return v; }
        std::memcpy(&v, p + off, sizeof(T));
        off += sizeof(T);
        return v;
    }
    std::string str() {
        uint16_t len = get<uint16_t>();
        if (bad || len > 64 || off + len > n) { bad = true; return ""; }
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
    template <class T> bool vec(std::vector<T>& v, size_t expect) {
        uint32_t len = get<uint32_t>();
        if (bad || len != expect) { bad = true; return false; }
        v.resize(len);
        for (auto& x : v) x = get<T>();
        return !bad;
    }
};

static void putStack(Writer& w, const Stack& s) { w.put(s.id); w.put(s.n); w.put(s.fresh); }
static Stack getStack(Reader& r) {
    Stack s;
    s.id = r.get<uint8_t>(); s.n = r.get<int16_t>(); s.fresh = r.get<float>();
    if (s.id >= I_COUNT || s.n < 0 || s.n > 999) { r.bad = true; return Stack{}; }
    if (s.n == 0) s.id = 0;
    return s;
}

bool saveGame(const char* path) {
    Writer w;
    w.buf.insert(w.buf.end(), MAGIC, MAGIC + 4);
    w.put(VERSION);
    w.put(g->seed); w.put(g->day); w.put(g->tod); w.put(g->totalTime);
    w.put(g->weather); w.put(g->weatherTimer); w.put(g->dayTempOffset); w.put(g->dryDays);
    w.put(g->drought); w.put(g->heatwave); w.put(g->frostHappened); w.put(g->frostWarned);
    w.put(g->snowCover); w.put(g->floodActive); w.put(g->powerStored); w.put(g->nextId);
    w.vec(g->ground);
    w.put((uint8_t)(g->origGround.size() == g->ground.size()));
    if (g->origGround.size() == g->ground.size()) w.vec(g->origGround);
    w.put((uint32_t)g->tiles.size());
    for (auto& t : g->tiles) { w.put(t.obj); w.put(t.hp); w.put(t.cont); w.put(t.var); w.put(t.timer); }
    w.put((uint32_t)g->crops.size());
    for (auto& c : g->crops) { w.put(c.type); w.put(c.growth); w.put(c.pest); w.put(c.dead); }
    w.vec(g->moist); w.vec(g->fert); w.vec(g->fire); w.vec(g->height);
    w.vec(g->river); w.vec(g->flooded); w.vec(g->seen); w.vec(g->reservoir);
    w.put((uint32_t)g->conts.size());
    for (auto& c : g->conts) {
        w.put(c.kind); w.put(c.x); w.put(c.y); w.put(c.alive);
        w.put((uint32_t)c.slots.size());
        for (auto& s : c.slots) putStack(w, s);
    }
    uint32_t alive = 0;
    for (auto& e : g->ents) alive += e.alive;
    w.put(alive);
    for (auto& e : g->ents) {
        if (!e.alive) continue;
        w.put(e.id); w.put(e.kind); w.put(e.x); w.put(e.y); w.put(e.hp); w.put(e.maxhp); w.put(e.face);
        w.put(e.state); w.put(e.tame); w.put(e.follow); w.put(e.trust); w.put(e.hunger); w.put(e.homeX); w.put(e.homeY);
        w.put(e.produce); w.put(e.baby); w.put(e.age); w.put(e.sleeping); w.put(e.job); w.put(e.thirst); w.put(e.mood);
        w.put(e.bed); putStack(w, e.carry); w.put(e.tribe); w.str(e.name); w.put(e.look); w.put(e.torch);
    }
    w.put((uint32_t)g->tribes.size());
    for (auto& t : g->tribes) {
        w.str(t.name); w.put(t.hostile); w.put(t.x); w.put(t.y); w.put(t.col);
        w.put((uint32_t)t.stock.size());
        for (auto& s : t.stock) putStack(w, s);
        w.put(t.credit); w.put(t.road); w.put(t.nextRaid); w.put(t.raidWarned); w.put(t.recruits);
    }
    Player& p = g->pl;
    w.put(p.x); w.put(p.y); w.put(p.hp); w.put(p.food); w.put(p.water); w.put(p.body); w.put(p.sick); w.put(p.wet);
    for (auto& s : p.inv) putStack(w, s);
    for (auto& s : p.wear) putStack(w, s);
    w.put(p.sel); w.put(p.riding);
    w.put(g->stats);
    w.put((uint32_t)g->goals.size());
    for (auto& gl : g->goals) w.put(gl.done);
    // write to a temp file first so a crash never corrupts the old save
    std::string tmp = std::string(path) + ".tmp";
    if (!SaveFileData(tmp.c_str(), w.buf.data(), (int)w.buf.size())) return false;
    std::remove(path);
    return std::rename(tmp.c_str(), path) == 0;
}

bool loadGame(const char* path) {
    if (!FileExists(path)) return false;
    int size = 0;
    unsigned char* data = LoadFileData(path, &size);
    if (!data || size < 8) { if (data) UnloadFileData(data); logMsg("Save file is unreadable.", RED); return false; }
    Reader r{data, (size_t)size};
    bool ok = std::memcmp(data, MAGIC, 4) == 0;
    r.off = 4;
    if (ok && r.get<uint32_t>() != VERSION) ok = false;
    Game* old = g;
    Game* ng = new Game;
    g = ng;  // helpers below write into the new game
    if (ok) {
        const size_t N = WW * WH;
        ng->seed = r.get<uint32_t>(); ng->day = r.get<int>(); ng->tod = r.get<float>(); ng->totalTime = r.get<float>();
        ng->weather = r.get<uint8_t>(); ng->weatherTimer = r.get<float>(); ng->dayTempOffset = r.get<float>(); ng->dryDays = r.get<int>();
        ng->drought = r.get<bool>(); ng->heatwave = r.get<bool>(); ng->frostHappened = r.get<bool>(); ng->frostWarned = r.get<bool>();
        ng->snowCover = r.get<float>(); ng->floodActive = r.get<bool>(); ng->powerStored = r.get<float>(); ng->nextId = r.get<int>();
        if (ng->weather > WX_FOG || ng->day < 0 || ng->tod < 0 || ng->tod >= 1.0001f) r.bad = true;
        r.vec(ng->ground, N);
        for (auto gr : ng->ground) if (gr >= G_COUNT) r.bad = true;
        if (r.get<uint8_t>()) { r.vec(ng->origGround, N); for (auto gr : ng->origGround) if (gr >= G_COUNT) r.bad = true; }
        if (r.get<uint32_t>() != N) r.bad = true;
        ng->tiles.resize(N);
        for (auto& t : ng->tiles) {
            t.obj = r.get<uint8_t>(); t.hp = r.get<int16_t>(); t.cont = r.get<int16_t>(); t.var = r.get<uint8_t>(); t.timer = r.get<float>();
            if (t.obj >= O_COUNT) r.bad = true;
            if (r.bad) break;
        }
        if (r.get<uint32_t>() != N) r.bad = true;
        ng->crops.resize(N);
        for (auto& c : ng->crops) {
            c.type = r.get<uint8_t>(); c.growth = r.get<float>(); c.pest = r.get<float>(); c.dead = r.get<bool>();
            if (c.type >= CR_COUNT) r.bad = true;
            if (r.bad) break;
        }
        r.vec(ng->moist, N); r.vec(ng->fert, N); r.vec(ng->fire, N); r.vec(ng->height, N);
        r.vec(ng->river, N); r.vec(ng->flooded, N); r.vec(ng->seen, N); r.vec(ng->reservoir, N);
        uint32_t nc = r.get<uint32_t>();
        if (nc > N) r.bad = true;
        for (uint32_t k = 0; k < nc && !r.bad; k++) {
            Container c;
            c.kind = r.get<uint8_t>(); c.x = r.get<int>(); c.y = r.get<int>(); c.alive = r.get<bool>();
            uint32_t ns = r.get<uint32_t>();
            if (ns > 64 || c.kind > C_BIOGAS || !inb(c.x, c.y)) { r.bad = true; break; }
            c.slots.resize(ns);
            for (auto& s : c.slots) s = getStack(r);
            ng->conts.push_back(std::move(c));
        }
        for (auto& t : ng->tiles) if (t.cont >= (int)ng->conts.size() || t.cont < -1) { r.bad = true; break; }
        uint32_t ne = r.get<uint32_t>();
        if (ne > 4000) r.bad = true;
        ng->ents.reserve(4096);
        for (uint32_t k = 0; k < ne && !r.bad; k++) {
            Ent e;
            e.id = r.get<int>(); e.kind = r.get<uint8_t>(); e.x = r.get<float>(); e.y = r.get<float>(); e.hp = r.get<float>(); e.maxhp = r.get<float>(); e.face = r.get<float>();
            e.state = r.get<int>(); e.tame = r.get<bool>(); e.follow = r.get<bool>(); e.trust = r.get<float>(); e.hunger = r.get<float>(); e.homeX = r.get<float>(); e.homeY = r.get<float>();
            e.produce = r.get<float>(); e.baby = r.get<bool>(); e.age = r.get<float>(); e.sleeping = r.get<bool>(); e.job = r.get<uint8_t>(); e.thirst = r.get<float>(); e.mood = r.get<float>();
            e.bed = r.get<int>(); e.carry = getStack(r); e.tribe = r.get<int>(); e.name = r.str(); e.look = r.get<uint8_t>(); e.torch = r.get<bool>();
            if (e.kind >= E_KCOUNT || e.job >= J_COUNT || !inb((int)e.x, (int)e.y) || e.bed < -1 || e.bed >= (int)N || e.tribe < -1 || e.tribe > 8) { r.bad = true; break; }
            e.look &= 3;
            ng->ents.push_back(std::move(e));
        }
        uint32_t nt = r.get<uint32_t>();
        if (nt > 8) r.bad = true;
        for (uint32_t k = 0; k < nt && !r.bad; k++) {
            Tribe t;
            t.name = r.str(); t.hostile = r.get<bool>(); t.x = r.get<int>(); t.y = r.get<int>(); t.col = r.get<Color>();
            uint32_t ns = r.get<uint32_t>();
            if (ns > 64 || !inb(t.x, t.y)) { r.bad = true; break; }
            for (uint32_t s = 0; s < ns; s++) t.stock.push_back(getStack(r));
            t.credit = r.get<int>(); t.road = r.get<bool>(); t.nextRaid = r.get<int>(); t.raidWarned = r.get<bool>(); t.recruits = r.get<int>();
            ng->tribes.push_back(std::move(t));
        }
        for (auto& e : ng->ents) if (e.tribe >= (int)ng->tribes.size()) r.bad = true;
        Player& p = ng->pl;
        p.x = r.get<float>(); p.y = r.get<float>(); p.hp = r.get<float>(); p.food = r.get<float>(); p.water = r.get<float>();
        p.body = r.get<float>(); p.sick = r.get<float>(); p.wet = r.get<float>();
        for (auto& s : p.inv) s = getStack(r);
        for (auto& s : p.wear) s = getStack(r);
        p.sel = r.get<int>(); p.riding = r.get<int>();
        if (p.sel < 0 || p.sel >= HOTBAR || !inb((int)p.x, (int)p.y)) r.bad = true;
        ng->stats = r.get<Stats>();
        initGoals();
        uint32_t ng_ = r.get<uint32_t>();
        if (ng_ != ng->goals.size()) r.bad = true;
        for (auto& gl : ng->goals) gl.done = r.get<bool>();
        ok = !r.bad;
    }
    UnloadFileData(data);
    if (!ok) {
        delete ng;
        g = old;
        logMsg("Save file is corrupt or from another version - not loaded.", RED);
        return false;
    }
    ng->indoor.assign(WW * WH, 0);
    ng->predator.assign(WW * WH, 0);
    for (int i = 0; i < WW * WH; i++) if (ng->fire[i] > 0) ng->fireTiles.push_back(i);
    computeIndoor();
    ng->panel = P_NONE;
    ng->zoom = old ? old->zoom : 3.f;
    delete old;
    checkRoads();
    return true;
}

void newGame(uint32_t seed) {
    Game* old = g;
    g = new Game;
    g->zoom = old ? old->zoom : 3.f;
    delete old;
    generateWorld(seed);
    g->tod = 0.28f;
    g->day = 0;
    g->dayTempOffset = 2;
    g->weather = WX_CLEAR;
    g->weatherTimer = 120;
    updateGoals();
    logMsg("You wake with nothing. Spring has come. Gather sticks, flint and fiber.", Color{255, 230, 170, 255});
    logMsg("Press H for help. Winter will come - prepare.", Color{200, 220, 255, 255});
}
