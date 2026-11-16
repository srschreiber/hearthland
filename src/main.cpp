// Hearthland entry point and main loop.
#include "game.h"
#include <ctime>

static void handleWorldInput() {
    Player& p = g->pl;
    // hotbar
    for (int k = 0; k < HOTBAR; k++)
        if (IsKeyPressed(KEY_ONE + k) || (k == 9 && IsKeyPressed(KEY_ZERO))) p.sel = k == 9 && IsKeyPressed(KEY_ZERO) ? 9 : k;
    float wheel = GetMouseWheelMove();
    if (wheel != 0) p.sel = (p.sel - (wheel > 0 ? 1 : -1) + HOTBAR) % HOTBAR;
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) g->zoom = std::min(5.f, g->zoom + 1);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) g->zoom = std::max(2.f, g->zoom - 1);

    Vector2 wm = GetScreenToWorld2D(GetMousePosition(), g->cam);
    int tx = (int)std::floor(wm.x / TS), ty = (int)std::floor(wm.y / TS);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) playerUseAt(tx, ty, wm);
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) playerInteractAt(tx, ty, wm);
}

// ---------------------------------------------------------------- developer harness
// Builds a small working farm around the player so automation can be exercised.
static void buildTestFarm() {
    Player& p = g->pl;
    int cx = (int)p.x, cy = (int)p.y;
    for (int y = cy - 12; y <= cy + 12; y++)
        for (int x = cx - 12; x <= cx + 12; x++)
            if (inb(x, y)) { removeObject(x, y, false); if (isWater(x, y)) g->ground[IDX(x, y)] = G_GRASS; }
    // a 7x5 house with a door and three beds
    int hx = cx - 3, hy = cy - 8;
    for (int x = hx; x < hx + 7; x++) { placeObject(x, hy, O_WALL); placeObject(x, hy + 4, x == hx + 3 ? O_DOOR : O_WALL); }
    for (int y = hy; y < hy + 5; y++) { placeObject(hx, y, O_WALL); placeObject(hx + 6, y, O_WALL); }
    for (int x = hx + 1; x < hx + 6; x++) for (int y = hy + 1; y < hy + 4; y++) g->ground[IDX(x, y)] = G_FLOOR;
    placeObject(hx + 1, hy + 1, O_BED); placeObject(hx + 2, hy + 1, O_BED); placeObject(hx + 3, hy + 1, O_BED); placeObject(hx + 4, hy + 1, O_BED);
    placeObject(hx + 5, hy + 1, O_GRANARY); placeObject(hx + 5, hy + 2, O_CHEST);
    placeObject(cx + 2, cy - 2, O_CAMPFIRE); placeObject(cx - 4, cy - 2, O_WELL); placeObject(cx + 4, cy - 2, O_WORKBENCH);
    placeObject(cx + 6, cy + 4, O_TROUGH); placeObject(cx + 6, cy + 6, O_COMPOST); placeObject(cx - 7, cy + 3, O_POND); placeObject(cx - 7, cy + 7, O_LEAFPILE);
    placeObject(cx + 8, cy - 6, O_KENNEL);
    for (int y = cy + 2; y < cy + 8; y++) for (int x = cx - 4; x < cx + 4; x++) g->ground[IDX(x, y)] = G_TILLED;
    placeObject(cx - 6, cy - 3, O_BATTERY); g->tiles[IDX(cx - 6, cy - 3)].timer = 6;
    placeObject(cx - 9, cy - 5, O_SOLAR); placeObject(cx - 8, cy - 5, O_SOLAR); placeObject(cx - 7, cy - 5, O_SOLAR);
    placeObject(cx - 2, cy - 1, O_LAMP); placeObject(cx - 8, cy - 1, O_SPRINKLER);
    for (int x = cx - 4; x < cx + 4; x += 2) placeObject(x, cy + 8, O_DRIP);
    breakLine(cx, cy + 8);
    // orchard: apple seedlings (random varieties) and a grafted pear
    for (int k = 0; k < 5; k++) {
        int x = cx - 10 + k * 2, y = cy + 10;
        g->ground[IDX(x, y)] = G_GRASS;
        placeObject(x, y, O_SAPLING);
        g->tiles[IDX(x, y)].var = (uint8_t)(5 << 2);
        g->fruitGene[IDX(x, y)] = newGene(FK_APPLE, true);
        g->tiles[IDX(x, y)].timer = 5;
    }
    placeObject(cx + 2, cy + 10, O_SAPLING); g->tiles[IDX(cx + 2, cy + 10)].var = (uint8_t)(5 << 2); g->fruitGene[IDX(cx + 2, cy + 10)] = newGene(FK_PEAR, false);
    placeObject(cx + 4, cy + 10, O_RASPBERRY); placeObject(cx + 5, cy + 10, O_GRAPEVINE);
    placeObject(cx + 9, cy - 2, O_BIRDHOUSE); placeObject(cx - 5, cy + 1, O_FLOWERS); placeObject(cx + 5, cy + 1, O_FLOWERS);
    placeObject(cx + 10, cy - 8, O_MINE);
    for (int k = 0; k < 4; k++) placeObject(hx + 7 + k, hy + 5, O_CHEST);
    placeObject(cx - 6, cy + 1, O_BEEHIVE);
    for (int k = 0; k < 2; k++) {
        Ent& w = spawnEnt(E_WORKER, cx + 0.5f, cy + 1.5f);
        w.name = k ? "Fisher" : "Miner"; w.job = (uint8_t)(k ? J_FISHER : J_MINER); w.hunger = w.thirst = 90; w.mood = 80;
    }
    placeObject(hx + 1, hy + 2, O_BED); placeObject(hx + 2, hy + 2, O_BED);
    computeIndoor();
    Container& gran = g->conts[g->tiles[IDX(hx + 5, hy + 1)].cont];
    addItem(gran.slots.data(), (int)gran.slots.size(), I_WHEATSEED, 40);
    addItem(gran.slots.data(), (int)gran.slots.size(), I_POTATO, 30);
    addItem(gran.slots.data(), (int)gran.slots.size(), I_BEANS, 20);
    addItem(gran.slots.data(), (int)gran.slots.size(), I_CABBAGESEED, 20);
    addItem(gran.slots.data(), (int)gran.slots.size(), I_BREAD, 20);
    addItem(gran.slots.data(), (int)gran.slots.size(), I_JERKY, 30);
    Container& ch = g->conts[g->tiles[IDX(hx + 5, hy + 2)].cont];
    addItem(ch.slots.data(), (int)ch.slots.size(), I_HAY, 50);
    addItem(ch.slots.data(), (int)ch.slots.size(), I_SHEARS, 1);
    addItem(ch.slots.data(), (int)ch.slots.size(), I_RAWMEAT, 10);
    int jobs[4] = {J_FARMER, J_WOODCUTTER, J_HERDER, J_COOK};
    for (int k = 0; k < 4; k++) {
        Ent& w = spawnEnt(E_WORKER, cx + 0.5f + k, cy + 0.5f);
        w.name = std::string("Tester") + char('A' + k); w.job = (uint8_t)jobs[k]; w.hunger = w.thirst = 90; w.mood = 80;
    }
    int kinds[] = {E_SHEEP, E_SHEEP, E_SHEEP, E_BUFFALO, E_BUFFALO, E_DUCK, E_DUCK, E_FOWL, E_DOG};
    for (int k : kinds) {
        Ent& a = spawnEnt(k, cx + 5 + rndr(0, 2), cy + 5 + rndr(0, 2));
        a.tame = true; a.trust = 100;
        a.homeX = cx + 6.f; a.homeY = cy + 5.f;
    }
    addToPlayer(I_AXE, 1); addToPlayer(I_HOE, 1); addToPlayer(I_TORCH, 1); addToPlayer(I_WHEATSEED, 10); addToPlayer(I_P_WALL, 10);
    addToPlayer(I_P_SOLAR, 2); addToPlayer(I_P_LAMP, 2); addToPlayer(I_BUCKET, 1);
}

extern bool g_echoLog;
static void soak(int days) {
    g_echoLog = true;
    buildTestFarm();
    for (int r = 1, done = 0; r < 60 && !done; r++)
        for (int y = (int)g->pl.y - r; y <= (int)g->pl.y + r && !done; y++)
            for (int x = (int)g->pl.x - r; x <= (int)g->pl.x + r && !done; x++)
                if (inb(x, y) && g->river[IDX(x, y)] && g->ground[IDX(x, y)] == G_WATER) {
                    placeObject(x, y, O_DAM);
                    int n = fillReservoir(x, y);
                    printf("dam at %d,%d flooded %d tiles\n", x, y, n);
                    done = 1;
                }
    int lastDay = -1;
    long steps = (long)(days * DAY_SECONDS / 0.05f);
    for (long s = 0; s < steps; s++) {
        g->pl.hp = 100; g->pl.food = 100; g->pl.water = 100; g->pl.body = 37;
        if (g->panel == P_DEAD) g->panel = P_NONE;
        simUpdate(0.05f);
        updatePlayer(0.05f);
        updateEnts(0.05f);
        if (false) {
            for (auto& e : g->ents) if (e.alive && e.kind == E_WORKER && e.job == J_HERDER)
                printf("  t=%d %02d:00 herder task %d stage %d carry %d x%d at %.0f,%.0f hay %d wheat %d sleeping %d\n", g->day, (int)(g->tod * 24), e.task, e.taskStage, e.carry.id, e.carry.n, e.x, e.y, storageCount(I_HAY), storageCount(I_WHEAT), e.sleeping);
        }
        if (g->day != lastDay) {
            lastDay = g->day;
            int workers = 0, tame = 0, wild = 0, wolves = 0, raiders = 0, growing = 0, ripe = 0, dead = 0, tilled = 0, alive = 0;
            for (auto& e : g->ents) if (e.alive) { alive++; if (e.kind == E_WORKER) workers++; else if (e.tame) tame++; else if (e.kind == E_WOLF) wolves++; else if (e.kind == E_RAIDER) raiders++; else wild++; }
            for (int i = 0; i < WW * WH; i++) { auto& c = g->crops[i]; if (c.type) { if (c.dead) dead++; else if (c.growth >= 1) ripe++; else growing++; } if (g->ground[i] == G_TILLED) tilled++; }
            int food = 0;
            for (int id = 1; id < I_COUNT; id++) if (ITEMS[id].food > 0) food += storageCount(id);
            {
                int variety = 0, nw = 0;
                for (auto& e : g->ents) if (e.alive && e.kind == E_WORKER) { variety += dietVariety(e); nw++; }
                int fruit = 0;
                for (int id : {I_APPLE, I_APPLE_SPITTER, I_APPLE_PRIZE, I_PEAR, I_GRAPES, I_RASPBERRY}) fruit += storageCount(id);
                Ent* any = nullptr;
                for (auto& e : g->ents) if (e.alive && e.kind == E_WORKER) { any = &e; break; }
                printf("        fruit %d fish %d ironore %d quartz %d | diet variety avg %.1f | beauty %.0f pace %.2f\n", fruit, storageCount(I_FISH) + storageCount(I_COOKEDFISH),
                       storageCount(I_IRONORE), storageCount(I_QUARTZ), nw ? variety / (float)nw : 0.f, farmBeauty(), any ? workRate(*any) : 1.f);
            }
            printf("day %2d %-6s %5.1fC wx=%d | workers %d tame %d wild %d wolves %d raiders %d ents %d | tilled %d grow %d ripe %d dead %d | stored food %d wood %d hay %d compost %d | fire %zu | made %.0f ate %.0f\n",
                   g->day, "Grow", airTemp(), g->weather, workers, tame, wild, wolves, raiders, alive, tilled, growing, ripe, dead,
                   food, storageCount(I_WOOD), storageCount(I_HAY), storageCount(I_COMPOST), g->fireTiles.size(), g->stats.lastFoodMade, g->stats.lastFoodEaten);
            if (false) for (auto& e : g->ents) if (e.alive && e.tame && e.kind != E_WORKER) printf("   %s hunger %.0f hp %.0f at %.1f,%.1f home %.1f,%.1f follow %d baby %d state %d\n", SPECIES[e.kind].name, e.hunger, e.hp, e.x, e.y, e.homeX, e.homeY, e.follow, e.baby, e.state);
            if (false) for (auto& c : g->conts) if (c.alive && c.kind == C_TROUGH) { int n = 0; for (auto& st : c.slots) n += st.n; printf("   trough at %d,%d holds %d\n", c.x, c.y, n); }
            if (false) for (auto& e : g->ents) if (e.alive && e.kind == E_WORKER) printf("   %s job %d task %d stage %d carry %d x%d at %.0f,%.0f hunger %.0f thirst %.0f\n", e.name.c_str(), e.job, e.task, e.taskStage, e.carry.id, e.carry.n, e.x, e.y, e.hunger, e.thirst);
            fflush(stdout);
        }
    }
    printf("--- log ---\n");
    for (auto& l : g->log) printf("%s\n", l.text.c_str());
    int done = 0;
    for (auto& gl : g->goals) { if (gl.done) { done++; printf("[x] %s\n", gl.title); } }
    printf("milestones %d/%zu\n", done, g->goals.size());
    // save/load round trip
    auto summary = []() {
        int ents = 0, objs = 0, crops = 0; double fert = 0;
        for (auto& e : g->ents) ents += e.alive;
        for (auto& t : g->tiles) objs += t.obj != 0;
        for (int i = 0; i < WW * WH; i++) { crops += g->crops[i].type != 0; fert += g->fert[i]; }
        return std::string(TextFormat("day %d tod %.3f ents %d objs %d crops %d fert %.2f inv0 %d hp %.0f conts %zu", g->day, g->tod, ents, objs, crops, fert, g->pl.inv[0].id, g->pl.hp, g->conts.size()));
    };
    std::string before = summary();
    bool saved = saveGame("roundtrip.sav");
    bool loaded = loadGame("roundtrip.sav");
    std::string after = summary();
    printf("save %d load %d\nbefore: %s\nafter:  %s\nroundtrip %s\n", saved, loaded, before.c_str(), after.c_str(), before == after ? "OK" : "MISMATCH");
    // corrupt file must be rejected without touching the running game
    int sz = 0;
    unsigned char* d = LoadFileData("roundtrip.sav", &sz);
    SaveFileData("corrupt.sav", d, sz / 2);
    d[10] ^= 0xFF;
    UnloadFileData(d);
    printf("truncated load rejected: %s, game intact: %s\n", loadGame("corrupt.sav") ? "NO" : "yes", summary() == after ? "yes" : "NO");
}

// Wires panels and machines to one bank and prints the charge curve for a summer and a winter day.
static void powerTest() {
    int cx = (int)g->pl.x, cy = (int)g->pl.y;
    for (int y = cy - 6; y <= cy + 6; y++) for (int x = cx - 12; x <= cx + 12; x++) if (inb(x, y)) { removeObject(x, y, false); g->ground[IDX(x, y)] = G_GRASS; }
    placeObject(cx, cy, O_BATTERY);
    for (int k = 0; k < 4; k++) placeObject(cx - 3 + k, cy - 3, O_SOLAR);
    placeObject(cx + 3, cy, O_HEATER);
    placeObject(cx + 2, cy + 2, O_LAMP);
    placeObject(cx + 11, cy, O_LAMP);   // out of cable range
    placeObject(cx - 2, cy + 2, O_FRIDGE);
    for (int d : {DAYS_PER_SEASON + 2, DAYS_PER_SEASON * 3 + 2}) {
        g->day = d; g->tod = 0; g->weather = WX_CLEAR; g->weatherTimer = 1e9f; g->dayTempOffset = 0;
        printf("day %d:\n", d);
        for (int h = 0; h < 24; h++) {
            for (int s = 0; s < (int)(DAY_SECONDS / 24 / 0.05f); s++) { g->weatherTimer = 1e9f; simUpdate(0.05f); }
            printf("  %02d:00 solar %.2f kW | gen %.2f load %.2f | battery %5.2f kWh | heater %s lamp %s fridge %s far-lamp %s | air %.0fC\n",
                   h + 1, powerFlowAt(cx - 3, cy - 3), g->powerGen, g->powerUse, g->powerStored,
                   poweredAt(cx + 3, cy) ? "on " : "OFF", poweredAt(cx + 2, cy + 2) ? "on " : "OFF", poweredAt(cx - 2, cy + 2) ? "on " : "OFF", poweredAt(cx + 11, cy) ? "on" : "OFF", airTemp());
        }
    }
}

// Irrigation: pressure budget on a well, a leak from a chewed line, then coyotes vs. a repairer over a few nights.
static void waterTest() {
    int cx = (int)g->pl.x, cy = (int)g->pl.y;
    for (int y = cy - 10; y <= cy + 10; y++) for (int x = cx - 12; x <= cx + 12; x++) if (inb(x, y)) { removeObject(x, y, false); g->ground[IDX(x, y)] = G_GRASS; }
    g->day = DAYS_PER_SEASON + 1; g->tod = 0.25f; g->weather = WX_CLEAR; g->weatherTimer = 1e9f;
    placeObject(cx, cy, O_WELL);
    int dx[3] = {cx + 2, cx + 4, cx + 6};
    for (int k = 0; k < 3; k++) placeObject(dx[k], cy + 3, O_DRIP);
    placeObject(cx - 3, cy + 3, O_SPRINKLER);
    auto report = [&](const char* label) {
        float sup, dr, pr;
        waterNetInfo(cx, cy, sup, dr, pr);
        printf("%-34s supply %.2f draw %.2f left %.2f | drips %d%d%d sprinkler %d | moist at drip0 %.2f\n", label, sup, dr, pr,
               irrigating(dx[0], cy + 3), irrigating(dx[1], cy + 3), irrigating(dx[2], cy + 3), irrigating(cx - 3, cy + 3), g->moist[IDX(dx[0], cy + 4)]);
    };
    auto run = [&](float secs) { for (float t = 0; t < secs; t += 0.05f) { g->weatherTimer = 1e9f; simUpdate(0.05f); updateEnts(0.05f); } };
    g->moist[IDX(dx[0], cy + 4)] = 0.1f;
    run(1.1f); report("well, 3 drips + 1 sprinkler:");
    removeObject(cx - 3, cy + 3, false); run(1.1f); report("sprinkler removed:");
    breakLine(dx[1], cy + 3); run(1.1f); report("middle drip chewed (leaking):");
    repairLine(dx[1], cy + 3); run(1.1f); report("repaired:");
    // a pump beside water with power
    placeObject(cx - 6, cy - 6, O_POND);
    placeObject(cx - 6, cy - 5, O_PUMP);
    placeObject(cx - 8, cy - 5, O_BATTERY); g->tiles[IDX(cx - 8, cy - 5)].timer = 10;
    placeObject(cx - 3, cy + 3, O_SPRINKLER); placeObject(cx - 3, cy - 1, O_SPRINKLER);
    run(1.1f); report("+ powered pump, 2 sprinklers:");
    g->tiles[IDX(cx - 8, cy - 5)].timer = 0; run(1.1f); report("battery dead (pump off):");
    g->tiles[IDX(cx - 8, cy - 5)].timer = 10; g->tiles[IDX(cx - 3, cy + 3)].timer = 1; run(1.1f); report("one sprinkler on morning timer:");
    // nights of coyotes with and without a repairer
    for (int k = 0; k < 8; k++) placeObject(cx - 4 + k, cy + 6, O_DRIP);
    extern bool g_echoLog; g_echoLog = true;
    for (int round = 0; round < 2; round++) {
        if (round == 1) { Ent& w = spawnEnt(E_WORKER, cx + 1.5f, cy + 1.5f); w.name = "Fixer"; w.job = J_REPAIR; w.hunger = w.thirst = 100; }
        int broken = 0, coyotes = 0;
        for (int d = 0; d < 4; d++) {
            g->tod = 0.79f;
            for (float t = 0; t < DAY_SECONDS * 0.6f; t += 0.05f) {
                g->weatherTimer = 1e9f; g->pl.hp = 100; g->pl.food = g->pl.water = 100; g->pl.body = 37;
                simUpdate(0.05f); updateEnts(0.05f);
                g->pl.x = cx - 30.f; g->pl.y = cy + 0.5f;   // stay away so coyotes come close
                for (auto& e : g->ents) if (e.alive && e.kind == E_COYOTE) coyotes = std::max(coyotes, 1);
            }
        }
        for (int y = cy - 10; y <= cy + 10; y++) for (int x = cx - 12; x <= cx + 12; x++) if (inb(x, y) && lineBroken(g->tiles[IDX(x, y)])) broken++;
        printf("%s: after 4 nights, %d lines broken (coyotes seen: %s)\n", round ? "WITH repairer" : "no repairer", broken, coyotes ? "yes" : "no");
        for (int y = cy - 10; y <= cy + 10; y++) for (int x = cx - 12; x <= cx + 12; x++) if (inb(x, y) && lineBroken(g->tiles[IDX(x, y)])) repairLine(x, y);
    }
}

int main(int argc, char** argv) {
    const char* shotPath = nullptr;
    int soakDays = 0, shotFrames = 90;
    float shotTod = -1;
    bool testFarm = false, powerTestArg = false, waterTestArg = false;
    uint32_t seedArg = 0;
    int dayArg = -1;
    std::string panelArg;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--shot" && i + 1 < argc) shotPath = argv[++i];
        else if (a == "--frames" && i + 1 < argc) shotFrames = atoi(argv[++i]);
        else if (a == "--tod" && i + 1 < argc) shotTod = (float)atof(argv[++i]);
        else if (a == "--soak" && i + 1 < argc) soakDays = atoi(argv[++i]);
        else if (a == "--farm") testFarm = true;
        else if (a == "--powertest") powerTestArg = true;
        else if (a == "--watertest") waterTestArg = true;
        else if (a == "--day" && i + 1 < argc) dayArg = atoi(argv[++i]);
        else if (a == "--panel" && i + 1 < argc) panelArg = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seedArg = (uint32_t)atoll(argv[++i]);
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    if (soakDays > 0 || shotPath || powerTestArg || waterTestArg) SetTraceLogLevel(LOG_WARNING);
    InitWindow(1600, 900, "Hearthland");
    SetWindowMinSize(1280, 720);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    SetRandomSeed((unsigned)time(nullptr));

    initData();
    buildAtlas();
    initRender();
    newGame(seedArg ? seedArg : (uint32_t)GetRandomValue(1, 1 << 30));
    g->panel = P_TITLE;
    if (soakDays > 0) { soak(soakDays); CloseWindow(); return 0; }
    if (powerTestArg) { powerTest(); CloseWindow(); return 0; }
    if (waterTestArg) { waterTest(); CloseWindow(); return 0; }
    if (shotPath) {
        g->panel = P_NONE;
        if (testFarm) buildTestFarm();
        if (shotTod >= 0) g->tod = shotTod;
        if (dayArg >= 0) { g->day = dayArg; if (g->season() == 3) { g->snowCover = 1; g->weather = WX_SNOW; } }
        if (panelArg == "inv") g->panel = P_INV;
        else if (panelArg == "goals") g->panel = P_GOALS;
        else if (panelArg == "help") g->panel = P_HELP;
        else if (panelArg == "title") g->panel = P_TITLE;
        else if (panelArg == "trade") { g->openTribe = 0; g->tribes[0].credit = 80; g->panel = P_TRADE; }
    }
    int frame = 0;

    while (!WindowShouldClose()) {
        float dt = std::min(GetFrameTime(), 0.05f);
        bool title = g->panel == P_TITLE;
        if (!title) {
            bool uiBlock = uiHandleInput();
            if (!uiBlock && g->panel == P_NONE) handleWorldInput();
            if (IsKeyPressed(KEY_F5)) { if (saveGame("hearthland.sav")) logMsg("Game saved.", GREEN); }
            if (IsKeyPressed(KEY_F9)) { if (loadGame("hearthland.sav")) logMsg("Game loaded.", GREEN); }
            bool paused = g->panel == P_PAUSE || g->panel == P_DEAD || g->panel == P_HELP || g->panel == P_TITLE;
            if (!paused) {
                // sleeping fast-forwards time
                float scale = g->pl.sleeping ? 24.f : 1.f;
                g->sleepFade = g->pl.sleeping ? std::min(1.f, g->sleepFade + dt * 2) : std::max(0.f, g->sleepFade - dt * 2);
                float simDt = dt * scale;
                while (simDt > 0) {
                    float step = std::min(simDt, 0.05f);
                    simUpdate(step);
                    updatePlayer(step);
                    updateEnts(step);
                    simDt -= step;
                    if (g->panel == P_DEAD) break;
                }
                if (g->autosaveMsg < 0) {
                    g->autosaveMsg = 0;
                    if (saveGame("hearthland.sav")) logMsg("Autosaved at dawn.", Color{160, 160, 160, 255});
                }
            }
        } else {
            g->totalTime += dt;
            uiHandleInput();
        }

        BeginDrawing();
        ClearBackground(Color{20, 30, 40, 255});
        renderWorld();
        drawUI();
        EndDrawing();
        if (shotPath && ++frame == shotFrames) { TakeScreenshot(shotPath); break; }
    }
    unloadRender();
    unloadAtlas();
    CloseWindow();
    return 0;
}
