// Static game data: items, objects, crops, species, recipes.
#include "game.h"

ItemDef ITEMS[I_COUNT];
ObjDef OBJS[O_COUNT];
CropDef CROPS[CR_COUNT];
SpeciesDef SPECIES[E_KCOUNT];
std::vector<Recipe> RECIPES;
const char* SEASON_NAMES[4] = {"Spring", "Summer", "Autumn", "Winter"};
const char* JOB_NAMES[J_COUNT] = {"Idle", "Farmer", "Woodcutter", "Herder", "Cook", "Guard", "Repairer", "Miner", "Fisher"};
const char* STATION_NAMES[7] = {"Hand", "Workbench", "Campfire", "Kiln", "Loom", "Tanning rack", "Drying rack"};

static Color C(int r, int g, int b) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255}; }

static ItemDef& IT(int id, const char* name, int stack, IconStyle ic, Color c1, Color c2, int value, const char* desc) {
    ItemDef& d = ITEMS[id];
    d.name = name; d.stack = stack; d.icon = ic; d.c1 = c1; d.c2 = c2; d.value = value; d.desc = desc;
    d.food = 0; d.drink = 0; d.spoilDays = 0; d.tool = T_NONE; d.insul = 0; d.wear = W_NONE;
    d.placeObj = -1; d.placeGround = -1;
    return d;
}
static void FOOD(int id, float food, float drink, float spoil) { ITEMS[id].food = food; ITEMS[id].drink = drink; ITEMS[id].spoilDays = spoil; }
static void PLACE(int id, const char* name, int obj, int value, const char* desc) {
    ItemDef& d = IT(id, name, 20, IC_BLOB, WHITE, WHITE, value, desc);
    d.placeObj = obj;
}

static ObjDef& OB(int id, const char* name, bool solid, bool flam, int hp, int drop) {
    ObjDef& o = OBJS[id];
    o.name = name; o.solid = solid; o.flammable = flam; o.hp = hp; o.dropItem = drop;
    o.doorlike = false; o.structure = false; o.wallLike = false; o.tool = T_NONE;
    o.light = 0; o.lightCol = WHITE; o.slots = 0; o.tall = false;
    return o;
}
static ObjDef& ST(int id, const char* name, bool solid, bool flam, int hp, int drop) {
    ObjDef& o = OB(id, name, solid, flam, hp, drop);
    o.structure = true;
    return o;
}

static void R(int out, int n, uint8_t station, const char* cat, std::initializer_list<Ingredient> in) {
    Recipe r{};
    r.out = out; r.n = n; r.station = station; r.cat = cat;
    int i = 0;
    for (auto& g : in) if (i < 4) r.in[i++] = g;
    RECIPES.push_back(r);
}

void initData() {
    // ------------------------------------------------------------ items
    IT(I_WOOD, "Wood", 50, IC_LOG, C(139, 94, 52), C(96, 62, 34), 1, "Logs from felled trees.");
    IT(I_STONE, "Stone", 50, IC_ROCK, C(150, 150, 156), C(100, 100, 108), 1, "Rough stone.");
    IT(I_FIBER, "Plant fiber", 50, IC_PLANT, C(140, 180, 80), C(90, 130, 50), 1, "Twisted grass fiber.");
    IT(I_STICK, "Stick", 50, IC_STICK, C(150, 110, 60), C(110, 80, 40), 1, "A sturdy branch.");
    IT(I_FLINT, "Flint", 50, IC_ROCK, C(70, 70, 90), C(40, 40, 60), 2, "Sharp stone for tools.");
    IT(I_LEAVES, "Leaves", 50, IC_PILE, C(170, 140, 60), C(120, 90, 40), 1, "Carbon-rich compost brown. Spiders love leaf piles.");
    IT(I_BERRIES, "Berries", 30, IC_BLOB, C(180, 40, 70), C(120, 20, 50), 2, "Wild berries.");
    IT(I_RAWMEAT, "Raw meat", 20, IC_MEAT, C(200, 70, 70), C(150, 40, 40), 4, "Cook it or dry it. Raw meat can make you sick.");
    IT(I_COOKEDMEAT, "Cooked meat", 20, IC_MEAT, C(150, 90, 50), C(110, 60, 30), 6, "Hearty and warm.");
    IT(I_JERKY, "Jerky", 30, IC_MEAT, C(120, 60, 40), C(80, 40, 30), 8, "Dried meat. Keeps for a long time.");
    IT(I_HIDE, "Hide", 20, IC_SHEET, C(160, 120, 80), C(120, 90, 60), 5, "Raw animal hide.");
    IT(I_LEATHER, "Leather", 20, IC_SHEET, C(130, 80, 40), C(90, 55, 30), 10, "Tanned leather.");
    IT(I_WOOL, "Wool", 30, IC_BLOB, C(235, 230, 220), C(200, 195, 185), 6, "Sheared wool. Spin it on a loom.");
    IT(I_CLOTH, "Wool cloth", 20, IC_SHEET, C(200, 190, 230), C(150, 140, 190), 14, "Warm woven cloth.");
    IT(I_CLAY, "Clay", 50, IC_BLOB, C(180, 110, 80), C(140, 80, 60), 2, "Wet clay from riverbanks.");
    IT(I_BRICK, "Brick", 50, IC_BAR, C(170, 80, 60), C(120, 50, 40), 4, "Fired clay brick.");
    IT(I_SAND, "Sand", 50, IC_PILE, C(220, 200, 140), C(180, 160, 110), 1, "Beach sand. Melt it into glass.");
    IT(I_GLASS, "Glass", 30, IC_SHEET, C(170, 220, 240), C(110, 170, 200), 10, "Clear glass panes.");
    IT(I_ORE, "Copper ore", 50, IC_ROCK, C(80, 150, 120), C(160, 100, 60), 6, "Green-streaked ore. Smelt in a kiln.");
    IT(I_COPPER, "Copper", 30, IC_BAR, C(220, 130, 70), C(170, 90, 40), 16, "Copper ingot for wiring and machines.");
    IT(I_WHEATSEED, "Wheat seed", 50, IC_SEED, C(220, 190, 110), C(170, 140, 70), 2, "Plant in tilled soil in spring or summer.");
    IT(I_WHEAT, "Wheat", 50, IC_PLANT, C(230, 200, 100), C(190, 150, 60), 3, "Grain. Store dry in a granary.");
    IT(I_BREAD, "Bread", 20, IC_BREAD, C(200, 150, 80), C(150, 100, 50), 9, "Baked wheat bread.");
    IT(I_POTATO, "Potato", 40, IC_BLOB, C(190, 150, 90), C(140, 100, 60), 3, "Plant it or eat it. Keeps in a cellar.");
    IT(I_CABBAGESEED, "Cabbage seed", 50, IC_SEED, C(120, 170, 90), C(80, 120, 60), 3, "Frost-hardy crop.");
    IT(I_CABBAGE, "Cabbage", 30, IC_BLOB, C(130, 200, 100), C(90, 150, 70), 4, "Crunchy and frost-hardy.");
    IT(I_BEANS, "Beans", 50, IC_SEED, C(170, 120, 90), C(120, 80, 60), 3, "Legume: fixes nitrogen. Harvest, or hoe under as green manure.");
    IT(I_STEW, "Vegetable stew", 10, IC_JAR, C(180, 110, 60), C(120, 80, 50), 14, "Warm, filling stew.");
    IT(I_EGG, "Egg", 30, IC_EGG, C(240, 230, 210), C(200, 190, 170), 3, "Fresh egg.");
    IT(I_MILK, "Milk", 20, IC_JAR, C(245, 245, 240), C(200, 200, 200), 4, "Buffalo milk. Spoils fast without cold storage.");
    IT(I_MANURE, "Manure", 50, IC_PILE, C(110, 80, 40), C(70, 50, 30), 1, "Nitrogen-rich compost green. Usable as fertilizer (mostly N), better composted.");
    IT(I_COMPOST, "Compost", 50, IC_PILE, C(70, 55, 40), C(40, 30, 25), 3, "Feeds N, P and K - more if the pile had a good carbon:nitrogen balance.");
    IT(I_HAY, "Hay", 50, IC_PLANT, C(210, 190, 110), C(170, 150, 80), 1, "Winter feed for livestock.");
    IT(I_MOLDY, "Moldy food", 50, IC_BLOB, C(110, 130, 80), C(70, 90, 60), 0, "Spoiled. Toss it in the compost (a green) - don't eat it.");
    IT(I_SAPLING, "Sapling", 20, IC_PLANT, C(90, 160, 70), C(60, 110, 50), 2, "Plant to regrow forest.");

    { auto& d = IT(I_AXE, "Stone axe", 1, IC_TOOL, C(130, 130, 140), C(140, 100, 60), 10, "Fell trees faster."); d.tool = T_AXE; }
    { auto& d = IT(I_PICK, "Stone pickaxe", 1, IC_TOOL, C(160, 160, 170), C(140, 100, 60), 10, "Mine rock and ore."); d.tool = T_PICK; }
    { auto& d = IT(I_HOE, "Hoe", 1, IC_TOOL, C(120, 120, 120), C(140, 100, 60), 8, "Till grass/dirt into farmland. Dig sand."); d.tool = T_HOE; }
    { auto& d = IT(I_SPEAR, "Flint spear", 1, IC_TOOL, C(70, 70, 90), C(150, 110, 60), 12, "Hunt and defend."); d.tool = T_SPEAR; }
    { auto& d = IT(I_SHEARS, "Copper shears", 1, IC_TOOL, C(220, 130, 70), C(100, 100, 100), 20, "Shear tame sheep for wool."); d.tool = T_SHEARS; }
    { auto& d = IT(I_SICKLE, "Sickle", 1, IC_TOOL, C(220, 130, 70), C(140, 100, 60), 14, "Cut grass for hay. Harvest crops."); d.tool = T_SICKLE; }
    { auto& d = IT(I_BUCKET, "Bucket", 1, IC_BUCKET, C(140, 100, 60), C(90, 60, 30), 8, "Fill at water. Douse fires, water crops."); d.tool = T_BUCKET; }
    { auto& d = IT(I_WATERBUCKET, "Water bucket", 1, IC_BUCKET, C(140, 100, 60), C(70, 140, 220), 8, "Water crops, douse fire, or drink."); d.tool = T_WBUCKET; d.drink = 35; }
    { auto& d = IT(I_TORCH, "Hand torch", 1, IC_STICK, C(250, 180, 60), C(140, 100, 60), 3, "Light in the dark. Wolves fear fire."); d.tool = T_TORCH; }

    { auto& d = IT(I_FURCOAT, "Fur coat", 1, IC_COAT, C(140, 100, 70), C(100, 70, 50), 40, "Body: +10C warmth."); d.wear = W_BODY; d.insul = 10; }
    { auto& d = IT(I_WOOLCOAT, "Wool coat", 1, IC_COAT, C(200, 190, 230), C(130, 80, 40), 70, "Body: +18C warmth."); d.wear = W_BODY; d.insul = 18; }
    { auto& d = IT(I_WOOLHAT, "Wool hat", 1, IC_HAT, C(200, 80, 80), C(240, 230, 220), 24, "Head: +5C warmth."); d.wear = W_HEAD; d.insul = 5; }
    { auto& d = IT(I_FURBOOTS, "Fur boots", 1, IC_BOOTS, C(140, 100, 70), C(90, 60, 40), 22, "Feet: +5C warmth."); d.wear = W_FEET; d.insul = 5; }

    FOOD(I_BERRIES, 7, 3, 3); FOOD(I_RAWMEAT, 8, 0, 2); FOOD(I_COOKEDMEAT, 32, 0, 4); FOOD(I_JERKY, 22, -4, 45);
    FOOD(I_BREAD, 36, 0, 8); FOOD(I_POTATO, 10, 1, 24); FOOD(I_WHEAT, 3, 0, 70); FOOD(I_CABBAGE, 12, 5, 14);
    FOOD(I_BEANS, 6, 0, 60); FOOD(I_STEW, 50, 18, 5); FOOD(I_EGG, 10, 1, 9); FOOD(I_MILK, 12, 18, 2);
    FOOD(I_WHEATSEED, 0, 0, 0); FOOD(I_CABBAGESEED, 0, 0, 0);
    FOOD(I_HONEY, 14, 0, 0);
    FOOD(I_BUCKWHEAT, 6, 0, 60); FOOD(I_MUSTARDGREENS, 6, 2, 4);
    FOOD(I_TOMATO, 9, 5, 5); FOOD(I_SQUASH, 18, 2, 60); FOOD(I_GREENBEANS, 8, 2, 5); FOOD(I_FISH, 8, 0, 1.5f); FOOD(I_COOKEDFISH, 28, 0, 3);
    FOOD(I_APPLE, 11, 3, 14); FOOD(I_APPLE_SPITTER, 4, 2, 10); FOOD(I_APPLE_PRIZE, 20, 4, 18); FOOD(I_PEAR, 12, 4, 8);
    FOOD(I_APRICOT, 11, 3, 3); FOOD(I_GRAPES, 9, 4, 5); FOOD(I_BLACKBERRY, 7, 2, 3); FOOD(I_RASPBERRY, 7, 2, 2);

    PLACE(I_P_CAMPFIRE, "Campfire", O_CAMPFIRE, 8, "Warmth, light and cooking. Keeps wolves away.");
    PLACE(I_P_TORCHPOST, "Torch post", O_TORCHPOST, 4, "Standing light source.");
    PLACE(I_P_WALL, "Wood wall", O_WALL, 3, "Enclose a room to make it indoors.");
    PLACE(I_P_STONEWALL, "Stone wall", O_STONEWALL, 5, "Fireproof wall.");
    PLACE(I_P_DOOR, "Door", O_DOOR, 5, "People can pass, animals can't.");
    PLACE(I_P_FLOOR, "Wood floor", -1, 2, "Wooden flooring."); ITEMS[I_P_FLOOR].placeGround = G_FLOOR;
    PLACE(I_P_BED, "Bed", O_BED, 12, "Sleep through the night. Workers need one indoors.");
    PLACE(I_P_CHEST, "Chest", O_CHEST, 8, "Storage. Food rots faster if outdoors in the rain.");
    PLACE(I_P_GRANARY, "Granary", O_GRANARY, 20, "Dry storage. Grain and vegetables keep 3x longer.");
    PLACE(I_P_CELLAR, "Root cellar", O_CELLAR, 24, "Cool storage. Food keeps 4x longer.");
    PLACE(I_P_WORKBENCH, "Workbench", O_WORKBENCH, 10, "Crafting station for buildings.");
    PLACE(I_P_KILN, "Kiln", O_KILN, 20, "Fire clay, melt glass, smelt copper.");
    PLACE(I_P_LOOM, "Loom", O_LOOM, 16, "Weave wool into cloth.");
    PLACE(I_P_TANNER, "Tanning rack", O_TANNER, 10, "Turn hides into leather and furs.");
    PLACE(I_P_DRYRACK, "Drying rack", O_DRYRACK, 8, "Dry meat into jerky, grass into hay.");
    PLACE(I_P_WELL, "Well", O_WELL, 20, "Clean water for you and your workers.");
    PLACE(I_P_FENCE, "Fence", O_FENCE, 2, "Keeps livestock in and wolves out.");
    PLACE(I_P_GATE, "Gate", O_GATE, 4, "A fence people can walk through.");
    PLACE(I_P_TROUGH, "Feed trough", O_TROUGH, 6, "Fill with hay or wheat. Livestock eat from it in winter.");
    PLACE(I_P_KENNEL, "Kennel", O_KENNEL, 10, "Guard dogs protect the area around their kennel.");
    PLACE(I_P_COOP, "Coop", O_COOP, 10, "Home for ducks and fowl. They eat garden insects.");
    PLACE(I_P_POND, "Pond", O_POND, 12, "Attracts frogs that eat crop pests (radius 7).");
    PLACE(I_P_LEAFPILE, "Leaf pile", O_LEAFPILE, 3, "Attracts spiders that eat crop pests (radius 4).");
    PLACE(I_P_COMPOST, "Compost bin", O_COMPOST, 8, "Turns manure, leaves and moldy food into compost.");
    PLACE(I_P_DAM, "Dam", O_DAM, 25, "Build on river water. Stops spring floods and powers wheels, but its reservoir drowns low land upstream.");
    PLACE(I_P_WHEEL, "Water wheel", O_WHEEL, 40, "Beside a dam: 1.5 kW day and night (0.3 kW without a dam). Slows when the river freezes. Wire to a battery.");
    PLACE(I_P_SOLAR, "Solar panel", O_SOLAR, 50, "Silicon cells: up to 0.4 kW at midday sun. Weak at dawn, dusk, in clouds and winter; nothing at night. Wire to a battery.");
    PLACE(I_P_BATTERY, "Battery bank", O_BATTERY, 40, "Stores 10 kWh. Sources within 8 tiles charge it, machines within 8 tiles draw from it.");
    PLACE(I_P_LAMP, "Electric lamp", O_LAMP, 18, "Bright light at night. Draws 0.06 kW from a battery.");
    PLACE(I_P_HEATER, "Heater", O_HEATER, 30, "Heats nearby tiles when cold. Draws 1.5 kW from a battery.");
    PLACE(I_P_SPRINKLER, "Sprinkler", O_SPRINKLER, 25, "Waters radius 3. Uses 1.2 bar, needs 2.0 bar left on its line. Pipe to a well or pump (8 tiles). Right-click: timer.");
    PLACE(I_P_FRIDGE, "Cold store", O_FRIDGE, 60, "Refrigeration: food barely spoils. Draws 0.12 kW from a battery.");
    PLACE(I_P_BIOGAS, "Biogas digester", O_BIOGAS, 60, "Feed manure: 1 kW and compost. Wire to a battery.");
    PLACE(I_P_GREENHOUSE, "Greenhouse frame", -1, 14, "Place on tilled soil. Warm, frost-free, needs watering."); ITEMS[I_P_GREENHOUSE].placeGround = G_GREENHOUSE;
    PLACE(I_P_ROAD, "Road", -1, 2, "Connect to trading villages for better prices."); ITEMS[I_P_ROAD].placeGround = G_ROAD;
    PLACE(I_P_TRADEPOST, "Trading post", O_TRADEPOST, 30, "Trade remotely with villages your roads reach.");
    PLACE(I_P_PUMP, "Electric pump", O_PUMP, 40, "Beside water: 4.5 bar of pressure. Draws 0.4 kW from a battery while anything is watering.");
    IT(I_SAP_MAPLE, "Maple sapling", 20, IC_PLANT, C(200, 70, 50), C(80, 140, 60), 6, "Rocky Mountain maple. Native, blazing red in autumn. Beautiful.");
    IT(I_SAP_ASH, "Mountain ash sapling", 20, IC_PLANT, C(230, 110, 40), C(80, 140, 60), 6, "Western mountain ash. Native; its berries draw insect-eating birds.");
    IT(I_SAP_ASPEN, "Aspen sapling", 20, IC_PLANT, C(230, 200, 70), C(230, 230, 220), 5, "Quaking aspen. Native, golden in autumn.");
    IT(I_SAP_SERVICE, "Serviceberry sapling", 20, IC_PLANT, C(240, 240, 250), C(120, 60, 110), 6, "Native serviceberry. Spring blossom, summer berries.");
    PLACE(I_P_FLOWERS, "Wildflower bed", O_FLOWERS, 6, "Native wildflowers. Pretty, and pollinators boost crop yields within 4 tiles.");
    PLACE(I_P_WATERFALL, "Garden waterfall", O_WATERFALL, 40, "A cascade that lifts everyone's spirits. Needs ~1.5 bar - in practice a pump. Right-click: timer.");
    IT(I_APPLE, "Apple", 30, IC_BLOB, C(200, 50, 40), C(150, 30, 30), 4, "A good eating apple.");
    IT(I_APPLE_SPITTER, "Spitter apple", 30, IC_BLOB, C(150, 160, 70), C(110, 120, 50), 1, "Sour and mealy. Livestock and the compost heap like it.");
    IT(I_APPLE_PRIZE, "Prize apple", 30, IC_BLOB, C(230, 60, 50), C(250, 200, 80), 9, "Large, sweet and nourishing. Worth grafting!");
    IT(I_PEAR, "Pear", 30, IC_BLOB, C(200, 200, 80), C(150, 150, 60), 4, "Juicy pear.");
    IT(I_APRICOT, "Apricot", 30, IC_BLOB, C(240, 150, 60), C(200, 100, 40), 4, "Soft apricot. Spoils quickly.");
    IT(I_GRAPES, "Grapes", 30, IC_BLOB, C(110, 50, 120), C(70, 30, 80), 4, "A bunch of grapes.");
    IT(I_BLACKBERRY, "Blackberries", 30, IC_BLOB, C(50, 30, 50), C(20, 10, 20), 3, "Ripe blackberries.");
    IT(I_RASPBERRY, "Raspberries", 30, IC_BLOB, C(220, 40, 80), C(160, 20, 60), 3, "Delicate raspberries. Eat soon.");
    IT(I_APPLESEED, "Apple seeds", 50, IC_SEED, C(80, 50, 30), C(50, 30, 20), 1, "Plant one: every seedling is a random new variety.");
    { auto& d = IT(I_SCION, "Scion cutting", 1, IC_STICK, C(110, 80, 50), C(90, 150, 70), 0, "A twig from a chosen fruit tree. Use it on a sapling to graft that variety."); (void)d; }
    IT(I_SAP_PEAR, "Pear sapling", 20, IC_PLANT, C(240, 240, 250), C(90, 150, 70), 8, "A grafted pear. Fruits in autumn.");
    IT(I_SAP_APRICOT, "Apricot sapling", 20, IC_PLANT, C(250, 190, 210), C(90, 150, 70), 8, "A grafted apricot. Fruits in summer.");
    PLACE(I_GRAPECUTTING, "Grape cutting", O_GRAPEVINE, 6, "Plant on its trellis. Grapes in autumn.");
    PLACE(I_BLACKCANE, "Blackberry cane", O_BLACKBERRY, 3, "Thorny bramble. Berries in late summer.");
    PLACE(I_RASPCANE, "Raspberry cane", O_RASPBERRY, 3, "Raspberries in summer.");
    IT(I_TREEGUARD, "Wire tree guard", 20, IC_SHEET, C(170, 170, 180), C(110, 110, 120), 3, "Wrap a tree or sapling: porcupines can't strip it and woodcutters leave it alone.");
    IT(I_IRONORE, "Iron ore", 50, IC_ROCK, C(140, 80, 60), C(90, 50, 40), 4, "Rusty ore. Smelt in a kiln.");
    IT(I_IRON, "Iron ingot", 30, IC_BAR, C(170, 170, 180), C(110, 110, 120), 12, "For tools, frames and machines.");
    IT(I_QUARTZ, "Quartz", 50, IC_ROCK, C(240, 240, 250), C(190, 200, 220), 5, "Pure silica crystal. Refine in a kiln into silicon.");
    IT(I_SILICON, "Silicon", 30, IC_SHEET, C(70, 80, 110), C(40, 50, 70), 15, "For solar cells and timer electronics.");
    IT(I_TIMER, "Irrigation timer", 10, IC_JAR, C(90, 100, 120), C(120, 220, 140), 18, "Fit to a sprinkler, drip line or waterfall to schedule it.");
    { auto& d = IT(I_IRONAXE, "Iron axe", 1, IC_TOOL, C(190, 190, 200), C(140, 100, 60), 30, "Fells trees twice as fast."); d.tool = T_AXE; }
    { auto& d = IT(I_IRONPICK, "Iron pickaxe", 1, IC_TOOL, C(190, 190, 200), C(140, 100, 60), 30, "Mines twice as fast."); d.tool = T_PICK; }
    IT(I_TOMATOSEED, "Tomato seed", 50, IC_SEED, C(240, 220, 150), C(200, 170, 100), 3, "Warm-season crop. Frost kills it.");
    IT(I_TOMATO, "Tomato", 30, IC_BLOB, C(220, 50, 40), C(80, 140, 60), 4, "Ripe tomato.");
    IT(I_SQUASHSEED, "Winter squash seed", 50, IC_SEED, C(240, 230, 190), C(210, 190, 140), 3, "Slow to grow, but squash keeps for months.");
    IT(I_SQUASH, "Winter squash", 20, IC_BLOB, C(220, 130, 40), C(120, 90, 40), 6, "Keeps all winter in a cool cellar.");
    IT(I_GREENBEANSEED, "Green bean seed", 50, IC_SEED, C(200, 190, 160), C(150, 140, 110), 2, "Fast legume. Restores soil nitrogen.");
    IT(I_GREENBEANS, "Green beans", 30, IC_PLANT, C(90, 170, 70), C(60, 130, 50), 3, "Fresh green beans.");
    { auto& d = IT(I_ROD, "Fishing rod", 1, IC_STICK, C(150, 110, 60), C(220, 220, 230), 8, "Click water to cast. When a fish bites (!), click again quickly."); d.tool = T_ROD; }
    IT(I_FISH, "Raw fish", 20, IC_MEAT, C(150, 170, 190), C(100, 120, 150), 4, "Trout and whitefish. Cook it; spoils fast.");
    IT(I_COOKEDFISH, "Cooked fish", 20, IC_MEAT, C(210, 160, 100), C(160, 110, 70), 7, "Flaky and nourishing.");
    PLACE(I_P_MINE, "Mine entrance", O_MINE, 30, "Dig into rocky ground. Hire miners: they go below and surface with stone and ore.");
    PLACE(I_P_BIRDHOUSE, "Bird house", O_BIRDHOUSE, 6, "Songbirds nest here and eat garden pests within 6 tiles.");
    IT(I_BUCKWHEAT, "Buckwheat", 50, IC_SEED, C(120, 90, 70), C(170, 140, 110), 2, "Green manure: grow it, then hoe it under to release phosphorus and potassium. Groats are edible.");
    IT(I_MUSTARDSEED, "Mustard seed", 50, IC_SEED, C(200, 170, 60), C(150, 120, 40), 2, "Brassica green manure: scavenges nutrients and suppresses soil pests. Hoe it under.");
    IT(I_MUSTARDGREENS, "Mustard greens", 30, IC_PLANT, C(110, 170, 70), C(80, 130, 50), 2, "Peppery greens.");
    IT(I_HONEY, "Honey", 30, IC_JAR, C(240, 180, 40), C(200, 130, 30), 8, "Sweet honey. Never spoils.");
    PLACE(I_P_BEEHIVE, "Beehive", O_BEEHIVE, 16, "A wild swarm moves in when enough is flowering nearby. Bees boost seed and fruit yields within 10 tiles.");
    PLACE(I_P_DRIP, "Drip line", O_DRIP, 6, "Waters its 3x3 patch - crops, saplings or trees. Uses 0.25 bar, needs 0.6 bar. Right-click: timer.");

    // ------------------------------------------------------------ objects
    { auto& o = OB(O_OAK, "Oak tree", true, true, 6, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_PINE, "Pine tree", true, true, 6, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_BIRCH, "Birch tree", true, true, 5, I_WOOD); o.tool = T_AXE; o.tall = true; }
    OB(O_SAPLING, "Sapling", false, true, 1, I_SAPLING);
    { auto& o = OB(O_STUMP, "Stump", false, false, 3, I_WOOD); o.tool = T_AXE; }
    OB(O_BUSH, "Berry bush", true, true, 3, I_STICK);
    { auto& o = OB(O_ROCK, "Rock", true, false, 5, I_STONE); o.tool = T_PICK; }
    { auto& o = OB(O_ORE, "Copper vein", true, false, 8, I_ORE); o.tool = T_PICK; }
    OB(O_CLAY, "Clay deposit", false, false, 3, I_CLAY);
    { auto& o = OB(O_TALLGRASS, "Tall grass", false, true, 1, I_FIBER); o.tool = T_SICKLE; }
    OB(O_REEDS, "Reeds", false, true, 1, I_FIBER);
    OB(O_BRANCH, "Branch", false, true, 1, I_STICK);
    OB(O_FLINT, "Flint", false, false, 1, I_FLINT);
    OB(O_MANURE, "Manure", false, false, 1, I_MANURE);
    OB(O_EGG, "Egg", false, false, 1, I_EGG);
    OB(O_ASH, "Ash", false, false, 1, I_NONE);

    { auto& o = ST(O_CAMPFIRE, "Campfire", true, false, 4, I_P_CAMPFIRE); o.light = 7; o.lightCol = C(255, 170, 90); }
    { auto& o = ST(O_TORCHPOST, "Torch post", true, true, 2, I_P_TORCHPOST); o.light = 5; o.lightCol = C(255, 180, 100); }
    { auto& o = ST(O_WALL, "Wood wall", true, true, 8, I_P_WALL); o.wallLike = true; o.tall = true; }
    { auto& o = ST(O_STONEWALL, "Stone wall", true, false, 16, I_P_STONEWALL); o.wallLike = true; o.tall = true; }
    { auto& o = ST(O_DOOR, "Door", true, true, 6, I_P_DOOR); o.wallLike = true; o.doorlike = true; o.tall = true; }
    ST(O_BED, "Bed", false, true, 3, I_P_BED);
    { auto& o = ST(O_CHEST, "Chest", true, true, 4, I_P_CHEST); o.slots = CHEST_SLOTS; }
    { auto& o = ST(O_GRANARY, "Granary", true, true, 8, I_P_GRANARY); o.slots = CHEST_SLOTS; o.tall = true; }
    { auto& o = ST(O_CELLAR, "Root cellar", true, false, 10, I_P_CELLAR); o.slots = CHEST_SLOTS; }
    ST(O_WORKBENCH, "Workbench", true, true, 4, I_P_WORKBENCH);
    { auto& o = ST(O_KILN, "Kiln", true, false, 8, I_P_KILN); o.light = 3; o.lightCol = C(255, 120, 60); o.tall = true; }
    ST(O_LOOM, "Loom", true, true, 4, I_P_LOOM);
    ST(O_TANNER, "Tanning rack", true, true, 3, I_P_TANNER);
    ST(O_DRYRACK, "Drying rack", true, true, 3, I_P_DRYRACK);
    ST(O_WELL, "Well", true, false, 10, I_P_WELL);
    ST(O_FENCE, "Fence", true, true, 10, I_P_FENCE);
    { auto& o = ST(O_GATE, "Gate", true, true, 10, I_P_GATE); o.doorlike = true; }
    { auto& o = ST(O_TROUGH, "Feed trough", true, true, 3, I_P_TROUGH); o.slots = 6; }
    ST(O_KENNEL, "Kennel", true, true, 4, I_P_KENNEL);
    ST(O_COOP, "Coop", true, true, 4, I_P_COOP);
    ST(O_POND, "Pond", false, false, 6, I_P_POND);
    ST(O_LEAFPILE, "Leaf pile", false, true, 1, I_P_LEAFPILE);
    { auto& o = ST(O_COMPOST, "Compost bin", true, false, 4, I_P_COMPOST); o.slots = 6; }
    ST(O_DAM, "Dam", true, false, 20, I_P_DAM);
    ST(O_WHEEL, "Water wheel", true, true, 8, I_P_WHEEL);
    ST(O_SOLAR, "Solar panel", true, false, 6, I_P_SOLAR);
    ST(O_BATTERY, "Battery bank", true, false, 6, I_P_BATTERY);
    { auto& o = ST(O_LAMP, "Electric lamp", true, false, 3, I_P_LAMP); o.light = 9; o.lightCol = C(255, 245, 210); o.tall = true; }
    { auto& o = ST(O_HEATER, "Heater", true, false, 4, I_P_HEATER); o.light = 2; o.lightCol = C(255, 110, 60); }
    ST(O_SPRINKLER, "Sprinkler", false, false, 3, I_P_SPRINKLER);
    { auto& o = ST(O_FRIDGE, "Cold store", true, false, 8, I_P_FRIDGE); o.slots = CHEST_SLOTS; o.tall = true; }
    { auto& o = ST(O_BIOGAS, "Biogas digester", true, false, 10, I_P_BIOGAS); o.slots = 6; }
    { auto& o = ST(O_TRADEPOST, "Trading post", true, true, 8, I_P_TRADEPOST); o.tall = true; }
    { auto& o = OB(O_TENT, "Tent", true, false, 999, I_NONE); o.tall = true; }
    { auto& o = OB(O_HUT, "Hut", true, false, 999, I_NONE); o.tall = true; }
    { auto& o = OB(O_BANNER, "Trade banner", true, false, 999, I_NONE); o.tall = true; }
    { auto& o = OB(O_TOTEM, "Bone totem", true, false, 999, I_NONE); o.tall = true; }
    { auto& o = OB(O_SKULLPOLE, "Skull pole", true, false, 999, I_NONE); o.tall = true; }
    { auto& o = OB(O_BONFIRE, "Bonfire", true, false, 999, I_NONE); o.light = 8; o.lightCol = C(255, 120, 60); }
    ST(O_PUMP, "Electric pump", true, false, 6, I_P_PUMP);
    { auto& o = OB(O_MAPLE, "Rocky Mountain maple", true, true, 6, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_MTNASH, "Western mountain ash", true, true, 5, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_ASPEN, "Quaking aspen", true, true, 5, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_SERVICEBERRY, "Serviceberry", true, true, 4, I_WOOD); o.tool = T_AXE; o.tall = true; }
    ST(O_FLOWERS, "Wildflower bed", false, true, 1, I_P_FLOWERS);
    { auto& o = OB(O_APPLE, "Apple tree", true, true, 6, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_PEAR, "Pear tree", true, true, 6, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = OB(O_APRICOT, "Apricot tree", true, true, 5, I_WOOD); o.tool = T_AXE; o.tall = true; }
    { auto& o = ST(O_GRAPEVINE, "Grape vine", true, true, 3, I_GRAPECUTTING); o.tall = true; }
    ST(O_BLACKBERRY, "Blackberry bramble", true, true, 3, I_BLACKCANE);
    ST(O_RASPBERRY, "Raspberry canes", true, true, 3, I_RASPCANE);
    { auto& o = OB(O_IRONORE, "Iron vein", true, false, 9, I_IRONORE); o.tool = T_PICK; }
    { auto& o = OB(O_QUARTZ, "Quartz vein", true, false, 8, I_QUARTZ); o.tool = T_PICK; }
    OB(O_WILDPOTATO, "Wild potatoes", false, true, 1, I_POTATO);
    { auto& o = ST(O_BIRDHOUSE, "Bird house", true, true, 3, I_P_BIRDHOUSE); o.tall = true; }
    ST(O_BEEHIVE, "Beehive", true, true, 4, I_P_BEEHIVE);
    { auto& o = ST(O_MINE, "Mine entrance", true, false, 20, I_P_MINE); o.light = 2; o.lightCol = C(255, 170, 90); }
    { auto& o = ST(O_WATERFALL, "Garden waterfall", true, false, 10, I_P_WATERFALL); o.tall = true; }
    ST(O_DRIP, "Drip line", false, false, 2, I_P_DRIP);

    // ------------------------------------------------------------ crops
    CROPS[CR_WHEAT]   = {"Wheat", I_WHEATSEED, I_WHEAT, 4, 5.f, false, 0.22f, I_HAY, 2, C(220, 190, 90)};
    CROPS[CR_POTATO]  = {"Potato", I_POTATO, I_POTATO, 5, 4.f, false, 0.28f, I_NONE, 0, C(100, 150, 70)};
    CROPS[CR_CABBAGE] = {"Cabbage", I_CABBAGESEED, I_CABBAGE, 2, 3.f, true, 0.18f, I_CABBAGESEED, 1, C(120, 190, 100)};
    CROPS[CR_BEANS]   = {"Beans", I_BEANS, I_BEANS, 3, 4.f, false, -0.30f, I_NONE, 0, C(90, 160, 80)};
    CROPS[CR_TOMATO]  = {"Tomato", I_TOMATOSEED, I_TOMATO, 5, 5.f, false, 0.25f, I_TOMATOSEED, 1, C(220, 60, 40)};
    CROPS[CR_SQUASH]  = {"Winter squash", I_SQUASHSEED, I_SQUASH, 3, 6.f, false, 0.25f, I_SQUASHSEED, 1, C(220, 130, 40)};
    CROPS[CR_BUCKWHEAT] = {"Buckwheat", I_BUCKWHEAT, I_BUCKWHEAT, 4, 2.5f, false, 0.05f, I_NONE, 0, C(240, 230, 230)};
    CROPS[CR_MUSTARD] = {"Mustard", I_MUSTARDSEED, I_MUSTARDGREENS, 3, 2.5f, true, 0.05f, I_MUSTARDSEED, 2, C(230, 210, 60)};
    CROPS[CR_GREENBEAN] = {"Green beans", I_GREENBEANSEED, I_GREENBEANS, 4, 3.f, false, -0.2f, I_GREENBEANSEED, 1, C(90, 170, 70)};

    // ------------------------------------------------------------ species
    SPECIES[E_SHEEP]   = {"Sheep", 30, 1.7f, 0, true, true, {I_WHEAT, I_HAY, I_BERRIES}, 2, 1, 1.0f, C(235, 230, 220), C(60, 50, 50)};
    SPECIES[E_DOG]     = {"Dog", 55, 3.6f, 9, false, false, {I_RAWMEAT, I_COOKEDMEAT, I_JERKY}, 1, 1, 0.9f, C(170, 120, 70), C(90, 60, 40)};
    SPECIES[E_WOLF]    = {"Wolf", 45, 3.8f, 9, false, false, {0, 0, 0}, 2, 2, 1.0f, C(120, 120, 130), C(70, 70, 80)};
    SPECIES[E_BEAR]    = {"Bear", 190, 3.0f, 24, false, false, {0, 0, 0}, 6, 3, 1.6f, C(100, 70, 45), C(60, 40, 30)};
    SPECIES[E_DUCK]    = {"Duck", 12, 1.5f, 0, true, true, {I_WHEATSEED, I_WHEAT, I_BEANS}, 1, 0, 0.6f, C(240, 240, 230), C(240, 170, 40)};
    SPECIES[E_FOWL]    = {"Jungle fowl", 12, 2.6f, 0, true, true, {I_WHEATSEED, I_BERRIES, I_WHEAT}, 1, 0, 0.6f, C(190, 70, 40), C(40, 110, 70)};
    SPECIES[E_BUFFALO] = {"Buffalo", 130, 1.6f, 14, true, true, {I_HAY, I_WHEAT, I_CABBAGE}, 6, 3, 1.6f, C(70, 60, 60), C(200, 190, 170)};
    SPECIES[E_HORSE]   = {"Wild horse", 80, 4.4f, 6, true, true, {I_WHEAT, I_HAY, I_BERRIES}, 4, 2, 1.4f, C(150, 100, 60), C(60, 40, 30)};
    SPECIES[E_FROG]    = {"Frog", 2, 2.0f, 0, false, false, {0, 0, 0}, 0, 0, 0.4f, C(80, 160, 70), C(50, 110, 50)};
    SPECIES[E_SPIDER]  = {"Spider", 2, 2.0f, 0, false, false, {0, 0, 0}, 0, 0, 0.35f, C(60, 50, 50), C(30, 30, 30)};
    SPECIES[E_PORCUPINE] = {"Porcupine", 22, 1.1f, 0, false, false, {0, 0, 0}, 1, 0, 0.7f, C(90, 70, 55), C(220, 210, 190)};
    SPECIES[E_COYOTE]  = {"Coyote", 25, 3.7f, 5, false, false, {0, 0, 0}, 1, 1, 0.8f, C(180, 150, 110), C(120, 100, 80)};
    SPECIES[E_WORKER]  = {"Worker", 70, 2.9f, 8, false, false, {0, 0, 0}, 0, 0, 1.0f, C(80, 120, 190), C(230, 190, 150)};
    SPECIES[E_VILLAGER]= {"Villager", 60, 2.4f, 6, false, false, {0, 0, 0}, 0, 0, 1.0f, C(60, 140, 200), C(230, 190, 150)};
    SPECIES[E_RAIDER]  = {"Raider", 60, 3.0f, 11, false, false, {0, 0, 0}, 0, 0, 1.0f, C(90, 30, 30), C(120, 90, 80)};

    // ------------------------------------------------------------ recipes
    const char* B = "Basics";
    R(I_AXE, 1, ST_NONE, B, {{I_STICK, 2}, {I_FLINT, 2}, {I_FIBER, 2}});
    R(I_PICK, 1, ST_NONE, B, {{I_STICK, 2}, {I_STONE, 3}, {I_FIBER, 2}});
    R(I_SPEAR, 1, ST_NONE, B, {{I_STICK, 2}, {I_FLINT, 1}, {I_FIBER, 2}});
    R(I_HOE, 1, ST_NONE, B, {{I_STICK, 2}, {I_STONE, 2}, {I_FIBER, 1}});
    R(I_TORCH, 1, ST_NONE, B, {{I_STICK, 1}, {I_FIBER, 2}});
    R(I_BUCKET, 1, ST_NONE, B, {{I_WOOD, 4}, {I_FIBER, 2}});
    R(I_P_CAMPFIRE, 1, ST_NONE, B, {{I_STONE, 5}, {I_STICK, 3}});
    R(I_P_WORKBENCH, 1, ST_NONE, B, {{I_WOOD, 8}, {I_STONE, 2}});
    R(I_P_LEAFPILE, 1, ST_NONE, B, {{I_LEAVES, 8}});
    R(I_P_TORCHPOST, 1, ST_NONE, B, {{I_STICK, 2}, {I_FIBER, 1}, {I_WOOD, 1}});

    const char* F = "Food";
    R(I_COOKEDMEAT, 1, ST_CAMPFIRE, F, {{I_RAWMEAT, 1}});
    R(I_COOKEDFISH, 1, ST_CAMPFIRE, F, {{I_FISH, 1}});
    R(I_ROD, 1, ST_NONE, F, {{I_STICK, 2}, {I_FIBER, 3}});
    R(I_BREAD, 1, ST_CAMPFIRE, F, {{I_WHEAT, 3}});
    R(I_STEW, 1, ST_CAMPFIRE, F, {{I_POTATO, 2}, {I_CABBAGE, 1}, {I_BEANS, 2}});
    R(I_JERKY, 2, ST_DRYRACK, F, {{I_RAWMEAT, 2}});
    R(I_HAY, 2, ST_DRYRACK, F, {{I_FIBER, 3}});

    const char* M = "Materials";
    R(I_BRICK, 2, ST_KILN, M, {{I_CLAY, 2}});
    R(I_GLASS, 1, ST_KILN, M, {{I_SAND, 3}});
    R(I_COPPER, 1, ST_KILN, M, {{I_ORE, 2}});
    R(I_CLOTH, 1, ST_LOOM, M, {{I_WOOL, 3}});
    R(I_LEATHER, 1, ST_TANNER, M, {{I_HIDE, 1}, {I_LEAVES, 2}});
    R(I_SHEARS, 1, ST_WORKBENCH, M, {{I_IRON, 2}});
    R(I_IRON, 1, ST_KILN, M, {{I_IRONORE, 2}, {I_WOOD, 1}});
    R(I_SILICON, 1, ST_KILN, M, {{I_QUARTZ, 2}, {I_WOOD, 2}});
    R(I_TIMER, 1, ST_WORKBENCH, M, {{I_GLASS, 1}, {I_SILICON, 1}, {I_COPPER, 1}});
    R(I_IRONAXE, 1, ST_WORKBENCH, M, {{I_IRON, 2}, {I_STICK, 2}});
    R(I_IRONPICK, 1, ST_WORKBENCH, M, {{I_IRON, 3}, {I_STICK, 2}});
    R(I_SICKLE, 1, ST_WORKBENCH, M, {{I_COPPER, 1}, {I_STICK, 2}});

    const char* W = "Clothing";
    R(I_FURCOAT, 1, ST_TANNER, W, {{I_HIDE, 4}, {I_FIBER, 4}});
    R(I_FURBOOTS, 1, ST_TANNER, W, {{I_HIDE, 2}, {I_FIBER, 2}});
    R(I_WOOLHAT, 1, ST_LOOM, W, {{I_WOOL, 3}});
    R(I_WOOLCOAT, 1, ST_LOOM, W, {{I_CLOTH, 4}, {I_LEATHER, 2}});

    const char* H = "Building";
    R(I_P_WALL, 2, ST_WORKBENCH, H, {{I_WOOD, 3}});
    R(I_P_STONEWALL, 2, ST_WORKBENCH, H, {{I_STONE, 4}, {I_CLAY, 1}});
    R(I_P_DOOR, 1, ST_WORKBENCH, H, {{I_WOOD, 4}});
    R(I_P_FLOOR, 4, ST_WORKBENCH, H, {{I_WOOD, 2}});
    R(I_P_BED, 1, ST_WORKBENCH, H, {{I_WOOD, 6}, {I_FIBER, 8}});
    R(I_P_CHEST, 1, ST_WORKBENCH, H, {{I_WOOD, 8}});
    R(I_P_GRANARY, 1, ST_WORKBENCH, H, {{I_WOOD, 12}, {I_CLAY, 4}});
    R(I_P_CELLAR, 1, ST_WORKBENCH, H, {{I_STONE, 12}, {I_WOOD, 4}, {I_CLAY, 4}});
    R(I_P_KILN, 1, ST_WORKBENCH, H, {{I_STONE, 10}, {I_CLAY, 8}});
    R(I_P_LOOM, 1, ST_WORKBENCH, H, {{I_WOOD, 10}, {I_STICK, 4}, {I_FIBER, 6}});
    R(I_P_TANNER, 1, ST_WORKBENCH, H, {{I_WOOD, 6}, {I_STICK, 4}});
    R(I_P_DRYRACK, 1, ST_WORKBENCH, H, {{I_STICK, 6}, {I_FIBER, 4}});
    R(I_P_WELL, 1, ST_WORKBENCH, H, {{I_STONE, 14}, {I_WOOD, 4}});
    R(I_P_ROAD, 4, ST_WORKBENCH, H, {{I_STONE, 2}, {I_SAND, 1}});
    R(I_P_MINE, 1, ST_WORKBENCH, H, {{I_WOOD, 20}, {I_STONE, 10}, {I_TORCH, 2}});
    R(I_P_TRADEPOST, 1, ST_WORKBENCH, H, {{I_WOOD, 12}, {I_CLOTH, 2}});

    const char* A = "Farm";
    R(I_P_FENCE, 4, ST_WORKBENCH, A, {{I_WOOD, 3}});
    R(I_P_GATE, 1, ST_WORKBENCH, A, {{I_WOOD, 4}});
    R(I_P_TROUGH, 1, ST_WORKBENCH, A, {{I_WOOD, 6}});
    R(I_P_KENNEL, 1, ST_WORKBENCH, A, {{I_WOOD, 8}, {I_LEAVES, 4}});
    R(I_P_COOP, 1, ST_WORKBENCH, A, {{I_WOOD, 8}, {I_HAY, 4}});
    R(I_P_POND, 1, ST_WORKBENCH, A, {{I_STONE, 6}, {I_CLAY, 6}});
    R(I_P_BIRDHOUSE, 1, ST_WORKBENCH, A, {{I_WOOD, 4}, {I_STICK, 2}});
    R(I_P_BEEHIVE, 1, ST_WORKBENCH, A, {{I_WOOD, 8}, {I_HAY, 4}});
    R(I_P_COMPOST, 1, ST_WORKBENCH, A, {{I_WOOD, 6}});
    R(I_P_GREENHOUSE, 1, ST_WORKBENCH, A, {{I_GLASS, 2}, {I_WOOD, 2}});
    R(I_P_SPRINKLER, 1, ST_WORKBENCH, A, {{I_COPPER, 2}, {I_WOOD, 2}, {I_CLAY, 2}});
    R(I_P_FLOWERS, 2, ST_NONE, A, {{I_FIBER, 2}, {I_COMPOST, 1}});
    R(I_P_WATERFALL, 1, ST_WORKBENCH, A, {{I_STONE, 16}, {I_CLAY, 4}, {I_IRON, 1}});
    R(I_P_DRIP, 4, ST_WORKBENCH, A, {{I_CLAY, 3}, {I_FIBER, 2}});
    R(I_P_PUMP, 1, ST_WORKBENCH, A, {{I_IRON, 2}, {I_COPPER, 2}, {I_BRICK, 2}});
    R(I_TREEGUARD, 3, ST_WORKBENCH, A, {{I_IRON, 1}});

    const char* P = "Power";
    R(I_P_DAM, 1, ST_WORKBENCH, P, {{I_STONE, 12}, {I_WOOD, 6}, {I_CLAY, 4}});
    R(I_P_WHEEL, 1, ST_WORKBENCH, P, {{I_WOOD, 16}, {I_IRON, 2}, {I_COPPER, 1}});
    R(I_P_SOLAR, 1, ST_WORKBENCH, P, {{I_GLASS, 2}, {I_SILICON, 2}, {I_COPPER, 2}, {I_IRON, 1}});
    R(I_P_BATTERY, 1, ST_WORKBENCH, P, {{I_COPPER, 4}, {I_CLAY, 4}, {I_BRICK, 2}});
    R(I_P_LAMP, 1, ST_WORKBENCH, P, {{I_GLASS, 1}, {I_COPPER, 1}});
    R(I_P_HEATER, 1, ST_WORKBENCH, P, {{I_IRON, 2}, {I_COPPER, 2}, {I_BRICK, 4}});
    R(I_P_FRIDGE, 1, ST_WORKBENCH, P, {{I_IRON, 3}, {I_COPPER, 3}, {I_GLASS, 2}});
    R(I_P_BIOGAS, 1, ST_WORKBENCH, P, {{I_BRICK, 12}, {I_COPPER, 3}, {I_CLAY, 6}});
}
