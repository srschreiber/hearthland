// Hearthland - a top-down survival homestead game.
// Shared declarations for every module.
#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <deque>

constexpr int WW = 192, WH = 192, TS = 16;
constexpr float DAY_SECONDS = 480.f;   // one in-game day = 8 real minutes
constexpr int DAYS_PER_SEASON = 7;
constexpr int INV_SLOTS = 30, HOTBAR = 10, CHEST_SLOTS = 18;

inline int IDX(int x, int y) { return y * WW + x; }
inline bool inb(int x, int y) { return x >= 0 && y >= 0 && x < WW && y < WH; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float dist(float ax, float ay, float bx, float by) { return std::sqrt((ax - bx) * (ax - bx) + (ay - by) * (ay - by)); }

// ---------------------------------------------------------------- ground
enum Ground : uint8_t {
    G_DEEP, G_WATER, G_SAND, G_GRASS, G_DIRT, G_FOREST, G_STONE, G_TILLED,
    G_ROAD, G_BURNT, G_FLOOR, G_GREENHOUSE, G_VILLAGE, G_COUNT
};

// ---------------------------------------------------------------- items
enum Item : uint8_t {
    I_NONE,
    I_WOOD, I_STONE, I_FIBER, I_STICK, I_FLINT, I_LEAVES, I_BERRIES, I_RAWMEAT, I_COOKEDMEAT,
    I_JERKY, I_HIDE, I_LEATHER, I_WOOL, I_CLOTH, I_CLAY, I_BRICK, I_SAND, I_GLASS, I_ORE,
    I_COPPER, I_WHEATSEED, I_WHEAT, I_BREAD, I_POTATO, I_CABBAGESEED, I_CABBAGE, I_BEANS, I_STEW,
    I_EGG, I_MILK, I_MANURE, I_COMPOST, I_HAY, I_MOLDY, I_SAPLING,
    // tools
    I_AXE, I_PICK, I_HOE, I_SPEAR, I_SHEARS, I_SICKLE, I_BUCKET, I_WATERBUCKET, I_TORCH,
    // clothing
    I_FURCOAT, I_WOOLCOAT, I_WOOLHAT, I_FURBOOTS,
    // placeables
    I_P_CAMPFIRE, I_P_TORCHPOST, I_P_WALL, I_P_STONEWALL, I_P_DOOR, I_P_FLOOR, I_P_BED, I_P_CHEST,
    I_P_GRANARY, I_P_CELLAR, I_P_WORKBENCH, I_P_KILN, I_P_LOOM, I_P_TANNER, I_P_DRYRACK, I_P_WELL,
    I_P_FENCE, I_P_GATE, I_P_TROUGH, I_P_KENNEL, I_P_COOP, I_P_POND, I_P_LEAFPILE, I_P_COMPOST,
    I_P_DAM, I_P_WHEEL, I_P_SOLAR, I_P_BATTERY, I_P_LAMP, I_P_HEATER, I_P_SPRINKLER, I_P_FRIDGE,
    I_P_BIOGAS, I_P_GREENHOUSE, I_P_ROAD, I_P_TRADEPOST, I_P_PUMP, I_P_DRIP,
    I_SAP_MAPLE, I_SAP_ASH, I_SAP_ASPEN, I_SAP_SERVICE, I_P_FLOWERS, I_P_WATERFALL,
    I_APPLE, I_APPLE_SPITTER, I_APPLE_PRIZE, I_PEAR, I_APRICOT, I_GRAPES, I_BLACKBERRY, I_RASPBERRY,
    I_APPLESEED, I_SCION, I_SAP_PEAR, I_SAP_APRICOT, I_GRAPECUTTING, I_BLACKCANE, I_RASPCANE, I_TREEGUARD,
    I_IRONORE, I_IRON, I_QUARTZ, I_SILICON, I_TIMER, I_IRONAXE, I_IRONPICK,
    I_TOMATOSEED, I_TOMATO, I_SQUASHSEED, I_SQUASH, I_GREENBEANSEED, I_GREENBEANS, I_ROD, I_FISH, I_COOKEDFISH, I_P_MINE, I_P_BIRDHOUSE, I_BUCKWHEAT, I_MUSTARDSEED, I_MUSTARDGREENS, I_HONEY, I_P_BEEHIVE,
    I_COUNT
};

enum ToolKind : uint8_t { T_NONE, T_AXE, T_PICK, T_HOE, T_SPEAR, T_SHEARS, T_SICKLE, T_BUCKET, T_WBUCKET, T_TORCH, T_ROD };
enum WearSlot : uint8_t { W_NONE, W_HEAD, W_BODY, W_FEET };
enum IconStyle : uint8_t { IC_BLOB, IC_LOG, IC_ROCK, IC_SEED, IC_MEAT, IC_BAR, IC_SHEET, IC_TOOL, IC_BUCKET, IC_COAT, IC_HAT, IC_BOOTS, IC_PILE, IC_EGG, IC_JAR, IC_STICK, IC_BREAD, IC_PLANT };

struct ItemDef {
    const char* name;
    int stack;
    float food, drink;      // nutrition when eaten
    float spoilDays;        // 0 = never spoils
    uint8_t tool;           // ToolKind
    float insul;            // clothing insulation (deg C)
    uint8_t wear;           // WearSlot
    int placeObj;           // object placed, -1 none
    int placeGround;        // ground placed, -1 none
    int value;              // barter value
    IconStyle icon;
    Color c1, c2;
    const char* desc;
};

// ---------------------------------------------------------------- objects
enum Obj : uint8_t {
    O_NONE,
    O_OAK, O_PINE, O_BIRCH, O_SAPLING, O_STUMP, O_BUSH, O_ROCK, O_ORE, O_CLAY, O_TALLGRASS, O_REEDS,
    O_BRANCH, O_FLINT, O_MANURE, O_EGG, O_ASH,
    O_CAMPFIRE, O_TORCHPOST, O_WALL, O_STONEWALL, O_DOOR, O_BED, O_CHEST, O_GRANARY, O_CELLAR,
    O_WORKBENCH, O_KILN, O_LOOM, O_TANNER, O_DRYRACK, O_WELL, O_FENCE, O_GATE, O_TROUGH, O_KENNEL,
    O_COOP, O_POND, O_LEAFPILE, O_COMPOST, O_DAM, O_WHEEL, O_SOLAR, O_BATTERY, O_LAMP, O_HEATER,
    O_SPRINKLER, O_FRIDGE, O_BIOGAS, O_TRADEPOST,
    // village decor
    O_TENT, O_HUT, O_BANNER, O_TOTEM, O_SKULLPOLE, O_BONFIRE,
    O_PUMP, O_DRIP,
    O_MAPLE, O_MTNASH, O_ASPEN, O_SERVICEBERRY, O_FLOWERS, O_WATERFALL,
    O_APPLE, O_PEAR, O_APRICOT, O_GRAPEVINE, O_BLACKBERRY, O_RASPBERRY, O_IRONORE, O_QUARTZ,
    O_WILDPOTATO, O_MINE, O_BIRDHOUSE, O_BEEHIVE,
    O_COUNT
};

struct ObjDef {
    const char* name;
    bool solid;         // blocks movement
    bool doorlike;      // people may pass (doors/gates)
    bool flammable;
    bool structure;     // built by player (counts as base)
    bool wallLike;      // encloses rooms
    int hp;
    uint8_t tool;       // tool that speeds harvesting
    float light;        // light radius in tiles
    Color lightCol;
    int slots;          // container slots
    int dropItem;       // item returned on deconstruct
    bool tall;          // sprite is 16x32
};

struct Stack { uint8_t id = 0; int16_t n = 0; float fresh = 1.f; };

enum ContKind : uint8_t { C_CHEST, C_GRANARY, C_CELLAR, C_FRIDGE, C_TROUGH, C_COMPOST, C_BIOGAS };
struct Container {
    uint8_t kind = C_CHEST;
    int x = 0, y = 0;
    bool alive = true;
    std::vector<Stack> slots;
};

struct Tile {
    uint8_t obj = O_NONE;
    int16_t hp = 0;
    int16_t cont = -1;      // container index
    uint8_t var = 0;        // sprite variation / state
    float timer = 0;        // berries regrow, egg age, etc
};

inline bool isFruitTree(uint8_t o) { return o == O_APPLE || o == O_PEAR || o == O_APRICOT; }
inline bool isFruitPlant(uint8_t o) { return isFruitTree(o) || o == O_GRAPEVINE || o == O_BLACKBERRY || o == O_RASPBERRY; }
inline bool isDeciduous(uint8_t o) { return o == O_OAK || o == O_BIRCH || o == O_MAPLE || o == O_MTNASH || o == O_ASPEN || o == O_SERVICEBERRY || isFruitTree(o); }
// Tile::var bits: 0-1 sprite variant, 2-4 sapling species, 0x20 timer fitted, 0x40 wire tree guard, 0x80 broken line
constexpr uint8_t VAR_TIMER = 0x20, VAR_GUARD = 0x40, VAR_BEES = 0x04;   // VAR_BEES: hive is occupied

// fruit varieties: apple seedlings are random; grafting copies a variety exactly
enum FruitKind : uint8_t { FK_APPLE, FK_PEAR, FK_APRICOT };
struct FruitGenes { uint8_t kind = FK_APPLE; float size = 0.5f, nutrition = 0.5f; uint8_t color = 0; };
inline bool isTreeObj(uint8_t o) { return o == O_PINE || isDeciduous(o); }

// ---------------------------------------------------------------- crops
enum CropType : uint8_t { CR_NONE, CR_WHEAT, CR_POTATO, CR_CABBAGE, CR_BEANS, CR_TOMATO, CR_SQUASH, CR_GREENBEAN, CR_BUCKWHEAT, CR_MUSTARD, CR_COUNT };
struct CropDef {
    const char* name; int seed; int produce; int yield; float days; bool hardy; float drain; int extra; int extraN;
    Color col;
};
struct Crop { uint8_t type = CR_NONE; float growth = 0; float pest = 0; bool dead = false; bool manure = false; /* grown to till in */ };

// ---------------------------------------------------------------- recipes
enum Station : uint8_t { ST_NONE, ST_WORKBENCH, ST_CAMPFIRE, ST_KILN, ST_LOOM, ST_TANNER, ST_DRYRACK };
struct Ingredient { int item; int n; };
struct Recipe { int out; int n; Ingredient in[4]; uint8_t station; const char* cat; };

// ---------------------------------------------------------------- entities
enum EKind : uint8_t {
    E_SHEEP, E_DOG, E_WOLF, E_BEAR, E_DUCK, E_FOWL, E_BUFFALO, E_HORSE, E_FROG, E_SPIDER, E_COYOTE, E_PORCUPINE,
    E_WORKER, E_VILLAGER, E_RAIDER, E_KCOUNT
};
struct SpeciesDef {
    const char* name; float hp; float speed; float dmg; bool herbivore; bool livestock;
    int tameFood[3]; int meat; int hide; float size; Color c1, c2;
};

enum Job : uint8_t { J_IDLE, J_FARMER, J_WOODCUTTER, J_HERDER, J_COOK, J_GUARD, J_REPAIR, J_MINER, J_FISHER, J_COUNT };

struct PathPt { int16_t x, y; };

struct Ent {
    int id = 0;
    uint8_t kind = E_SHEEP;
    bool alive = true;
    float x = 0, y = 0, vx = 0, vy = 0;
    float hp = 10, maxhp = 10;
    float face = 1;             // -1 left, +1 right
    float anim = 0;
    float timer = 0, timer2 = 0;
    int state = 0;
    int target = -1;            // entity id
    int tx = -1, ty = -1;       // target tile
    // animals
    bool tame = false, follow = false;
    float trust = 0;
    float hunger = 100;
    float homeX = 0, homeY = 0;
    float produce = 0;          // 0..1 product readiness
    bool baby = false;
    float age = 0;
    float hurtFlash = 0;
    bool sleeping = false;
    // humans
    uint8_t job = J_IDLE;
    float thirst = 100, mood = 100;
    int bed = -1;               // tile index of bed
    Stack carry;
    int tribe = -1;
    std::string name;
    std::vector<PathPt> path;
    int pathI = 0;
    int task = 0, taskStage = 0, state2 = 0;
    float repath = 0;
    float blocked = 0;          // workers: pause after a target turned out unreachable
    uint8_t look = 0;           // appearance variant
    uint8_t diet[12] = {};      // workers: last meals eaten (item ids), for diet variety
    uint8_t dietI = 0;
    bool hidden = false;        // miners underground
    std::vector<Stack> haul;    // extra items a miner brings up
    bool torch = false;
};

struct Tribe {
    std::string name;
    bool hostile = false;
    int x = 0, y = 0;           // village centre
    Color col;
    std::vector<Stack> stock;
    int credit = 0;
    bool road = false;
    int nextRaid = 0;
    bool raidWarned = false;
    int recruits = 3;
};

// ---------------------------------------------------------------- weather & events
enum Weather : uint8_t { WX_CLEAR, WX_CLOUDY, WX_RAIN, WX_STORM, WX_SNOW, WX_BLIZZARD, WX_FOG };

struct Particle { float x, y, vx, vy, life, max; Color c; float size; uint8_t kind; };
struct Floater { float x, y, life; std::string text; Color c; };
struct LogMsg { std::string text; Color c; float t; };

enum Panel : uint8_t { P_NONE, P_INV, P_CONTAINER, P_TRADE, P_WORKER, P_GOALS, P_MAP, P_HELP, P_PAUSE, P_DEAD, P_TITLE };

struct Player {
    float x = 0, y = 0;
    float hp = 100, food = 100, water = 100, body = 37.f;
    float sick = 0, wet = 0;
    float face = 1, anim = 0, atkCD = 0, useAnim = 0;
    Stack inv[INV_SLOTS];
    Stack wear[4];
    int sel = 0;
    int riding = -1;
    bool sleeping = false;
    float hurtFlash = 0;
    // fishing: cast -> wait -> bite (click quickly) -> catch
    bool fishing = false;
    float fishTimer = 0, biteTimer = 0;
    int bobX = 0, bobY = 0;
};

struct Goal { const char* title; const char* hint; bool done = false; };

struct Stats {
    float foodMade = 0, foodEaten = 0;    // today (in hunger points)
    float lastFoodMade = 0, lastFoodEaten = 0;
    int livestockLost = 0, cropsLost = 0;
    int nightsSurvived = 0, wintersSurvived = 0;
    int composted = 0;
    bool usedGranary = false;
};

// ---------------------------------------------------------------- the game
struct Game {
    uint32_t seed = 1;
    // world
    std::vector<uint8_t> ground, origGround;
    std::vector<Tile> tiles;
    std::vector<Crop> crops;
    std::vector<float> moist, fert, phos, pota, fire, predator, height;
    std::vector<float> litter;              // manure lying on the surface; feeds the soil once tilled in   // fert = soil nitrogen; phos, pota = phosphorus, potassium
    std::vector<uint8_t> indoor, river, flooded, seen, reservoir;
    std::vector<Container> conts;
    std::vector<FruitGenes> genes;          // registry of fruit varieties
    std::vector<int> fruitGene;             // per tile: variety of a fruit tree or grafted sapling, -1 none
    std::vector<uint8_t> fruitState;        // per tile: 0 growing, 1 ripe, 2 picked this year
    std::vector<Ent> ents;
    int nextId = 1;
    std::vector<Tribe> tribes;
    Player pl;
    // time
    float tod = 0.3f;           // 0..1, 0 = midnight
    int day = 0;
    float totalTime = 0;
    int season() const { return 1; }   // one endless growing season
    int dayOfSeason() const { return day % DAYS_PER_SEASON; }
    int week() const { return day / DAYS_PER_SEASON + 1; }
    // weather
    uint8_t weather = WX_CLEAR;
    float weatherTimer = 60;
    float dayTempOffset = 0;
    int dryDays = 0;
    bool drought = false, heatwave = false;
    float heatwaveTimer = 0;
    float lightningTimer = 10;
    float flash = 0;
    bool frostHappened = false, frostWarned = false;
    float snowCover = 0;
    bool floodActive = false;
    bool stormYesterday = false;
    // power
    float powerGen = 0, powerUse = 0, powerStored = 0, powerCap = 0;   // kW, kW, kWh, kWh across all networks
    bool powered = false;                                              // any network powered
    // bookkeeping
    bool indoorDirty = true;
    float simAcc = 0;
    Stats stats;
    std::vector<Goal> goals;
    std::deque<LogMsg> log;
    std::vector<Particle> parts;
    std::vector<Floater> floats;
    std::vector<int> fireTiles;
    // ui
    Panel panel = P_TITLE;
    int openCont = -1, openTribe = -1, openWorker = -1;
    bool tradeRemote = false;
    Stack cursor;               // item held by mouse in panels
    int craftScroll = 0, craftCat = 0;
    float sleepFade = 0;
    Camera2D cam{};
    float zoom = 3.f;
    float shake = 0;
    float autosaveMsg = 0;
    bool showMinimap = true;
};

extern Game* g;
extern ItemDef ITEMS[I_COUNT];
extern ObjDef OBJS[O_COUNT];
extern CropDef CROPS[CR_COUNT];
extern SpeciesDef SPECIES[E_KCOUNT];
extern std::vector<Recipe> RECIPES;
extern const char* SEASON_NAMES[4];
extern const char* JOB_NAMES[J_COUNT];
extern const char* STATION_NAMES[7];

// ---------------------------------------------------------------- rng / noise
uint32_t hash32(uint32_t x);
float rnd();                    // [0,1)
float rndr(float a, float b);
int rndi(int a, int b);         // inclusive
float noise2(float x, float y, uint32_t seed);
float fbm(float x, float y, uint32_t seed, int oct);

// data.cpp
void initData();

// world.cpp
void generateWorld(uint32_t seed);
bool solidFor(int x, int y, int who);      // who: 0 player, 1 animal, 2 human npc, 3 raider
bool isWater(int x, int y);
void computeIndoor();
bool findPath(int sx, int sy, int tx, int ty, int who, std::vector<PathPt>& out, int maxNodes = 5000, bool adjacentOk = true);
int newContainer(uint8_t kind, int x, int y, int slots);
void placeObject(int x, int y, int obj);
void removeObject(int x, int y, bool dropContents);
Ent& spawnEnt(int kind, float x, float y);
Ent* entById(int id);
bool baseCentre(float& bx, float& by);
int countStructures();
bool nearObj(float x, float y, int obj, float r, int* ox = nullptr, int* oy = nullptr);

// inventory helpers (sim.cpp)
int addItem(Stack* slots, int n, int id, int count, float fresh = 1.f);   // returns leftover
int countItem(Stack* slots, int n, int id);
bool takeItem(Stack* slots, int n, int id, int count);
int addToPlayer(int id, int count, float fresh = 1.f);   // drops leftover as message
bool storeAnywhere(int id, int count, float fresh, float nearX, float nearY);
bool takeFromStorage(int id, int count, float nearX, float nearY, float* freshOut = nullptr);
int storageCount(int id);
bool isFood(int id);

// sim.cpp
void simUpdate(float dt);
float airTemp();
float tempAt(int x, int y);
float sunLevel();
float solarFactor();
float powerFlowAt(int x, int y);
bool poweredAt(int x, int y);
bool isPowerObj(uint8_t o);
int powerLink(int i);
// irrigation: wells and pumps pressurise pipes; sprinklers and drip lines draw pressure
bool isWaterObj(uint8_t o);
int waterLink(int i);
bool irrigating(int x, int y);           // device is spraying right now
bool irrigationCovers(int x, int y);
bool leaking(int x, int y);               // a broken line is spraying water at the break     // tile is watered by a working, scheduled device
bool waterNetInfo(int x, int y, float& supply, float& draw, float& pressure);
float waterMinPressure(uint8_t o);
const char* timerName(int mode);
bool timerActive(int mode);
// irrigation devices chewed by coyotes leak until repaired (flag kept in Tile::var)
constexpr uint8_t VAR_BROKEN = 0x80;
inline bool lineBroken(const Tile& t) { return (t.obj == O_SPRINKLER || t.obj == O_DRIP) && (t.var & VAR_BROKEN); }
void breakLine(int x, int y);
bool compostParts(int id, float& c, float& n);
float compostRatio(const Container& c, int* items);
float compostQuality(float ratio);
// landscaping: beauty around the base lifts worker morale and pace
float farmBeauty();
float workRate(const Ent& e);
int saplingFor(int treeObj);     // sapling item a felled tree drops
int treeForSapling(int item);    // tree a sapling item grows into (0 = oak or pine, 5 = fruit by genes)
int newGene(int kind, bool seedling);
std::string geneDesc(int id);
Color fruitColor(int tile);
bool harvestFruit(int x, int y, bool byWorker, float wx, float wy);
int dietVariety(const Ent& e);
void repairLine(int x, int y);
bool powerNetInfo(int x, int y, float& gen, float& load, float& stored, float& cap);
void logMsg(const std::string& s, Color c = RAYWHITE);
void floatText(float x, float y, const std::string& s, Color c);
void burst(float x, float y, Color c, int n, float spd = 1.f);
void startFire(int x, int y);
void harvestCrop(int x, int y, bool byWorker, float wx, float wy);
// soil nutrients (N, P, K)
void addNutrients(int i, float n, float p, float k);
float soilFactor(int i, int crop);     // 0..~1.4, limited by the scarcest nutrient the crop needs
float soilMin(int i);
void plowUnder(int x, int y);          // hoe a crop (green manure or dead) into the soil
void tillInLitter(int i);              // work surface manure into the soil
std::string soilAdvice(int i);
bool beesNear(int x, int y);          // an occupied hive within 10 tiles pollinates here
int collectHoney(int x, int y);       // takes ready honey from a hive, returns jars
void updateGoals();
void warnStorageFull(const char* what);
void initGoals();
void newDay();
void checkRoads();
int fillReservoir(int dx, int dy);
void drainReservoir(int dx, int dy);

// entities.cpp
void updatePlayer(float dt);
void updateEnts(float dt);
void damageEnt(Ent& e, float dmg, float fromX, float fromY);
void hurtPlayer(float dmg, const char* why);
void playerUseAt(int tx, int ty, Vector2 worldMouse);
void playerInteractAt(int tx, int ty, Vector2 worldMouse);
Ent* entAt(Vector2 worldPx, float r);
void spawnWildlife(bool initial);
void spawnRaid(int tribe);
void recruitWorker(int tribe);
int freeBed();

// sprites.cpp
void buildAtlas();
void unloadAtlas();
Rectangle groundSrc(int g, int season, int var);
Rectangle objSrc(int obj, int season, int var);
Rectangle itemSrc(int item);
Rectangle entSrc(int kind, int frame, int look);
extern Texture2D atlas;

// render.cpp
void renderWorld();
void initRender();
void unloadRender();

// ui.cpp
void drawUI();
bool uiHandleInput();   // returns true if UI consumed the mouse
void drawItemIcon(int id, float x, float y, float size);
void openPanel(Panel p);

// save.cpp
bool saveGame(const char* path);
bool loadGame(const char* path);
void newGame(uint32_t seed);
