# Hearthland

A top-down pixel-art survival game set on the world's surface. You start with nothing and grow a
self-sustaining modern farm through the seasons, with dams, solar panels, greenhouses, biogas,
livestock and workers who run the land for you.

Written in C++20 with [raylib](https://www.raylib.com/) 5.5. All art is drawn procedurally at startup.

## Build & run

Requires CMake 3.20+ and a C++20 compiler. CMake fetches raylib automatically.

```sh
cmake -S . -B build
cmake --build build -j8
./build/hearthland
```

Saves go to `hearthland.sav` in the working directory. The game autosaves every dawn.

## Controls

| Input | Action |
| --- | --- |
| WASD / arrows | Move (Shift to sprint) |
| Left click | Use held item: chop, mine, gather, till, plant, water, attack, build |
| Right click | Interact: storage, eat/drink/wear, tame/feed, ride, trade, sleep, manage workers |
| Shift + left click | Pick a building back up |
| 1-0 / mouse wheel | Select hotbar slot |
| Tab | Inventory & crafting |
| G | Milestones & farm ledger |
| M / N | World map / toggle minimap |
| H | Help |
| +/- | Zoom |
| F5 / F9 | Save / load |
| Esc | Close panel / pause menu |

## How the world works

- **One endless growing season.** There's no winter, so you can build at your own pace. A day lasts 8 real
  minutes and a week is 7 days. Nights are cool and storms soak you, so fire, shelter and clothes matter.
- **Soil (N/P/K).** Every crop drains nitrogen, phosphorus and potassium differently, and yield follows the
  scarcest one. Hoe green manures under to restore it: beans fix N, buckwheat mines P and K, and mustard
  suppresses pests. Compost (better with a ~30:1 carbon:nitrogen mix), manure, wood ash and flood silt all
  help. Penned chickens leave droppings that feed the soil once you till them in. Free-range birds eat
  unfenced crops.
- **Crops.** Wheat, potatoes (find them wild, plant a potato), cabbage, beans, tomatoes, winter squash,
  green beans, buckwheat and mustard.
- **Orchards.** Trees mature in one week and fruit every week after. Apple seeds produce random varieties
  (size, colour, nutrition). Cut a scion from a tree you like and graft it onto a sapling to keep the
  variety. There are also pears, apricots, grapes, raspberries and blackberries.
- **No pesticides.** Ponds (frogs), leaf piles and compost (spiders, wasps), bird houses, mountain ash,
  wildflowers and poultry keep pests down. Bees boost seed and fruit yields and spread wildflowers.
- **Food spoils.** Granaries and root cellars slow spoilage, and a powered cold store nearly stops it.
  Chests left in the rain go mouldy.
- **Animals.** Sheep, buffalo, ducks, jungle fowl, horses and dogs can be tamed. Wolves hunt livestock,
  coyotes chew irrigation lines, porcupines ring-bark deciduous trees (wire tree guards stop them), and
  bears roam the woods.
- **Tribes.** Trading villages fly banners and have a trader marked **$**. Raider camps have skulls and
  war paint. Roads to a trading post improve prices. Recruit workers as farmers, woodcutters, herders,
  cooks, guards, repairers, miners or fishers. Workers need a bed, water and food, and a varied diet plus a
  beautiful farm (native trees, ponds, flowers, a waterfall) make them work faster.
- **Water.** Fields near rivers stay damp. Farther out you need wells (1.5 bar) or electric pumps
  (4.5 bar) feeding sprinklers and drip lines. Every device uses pressure, and timers schedule them.
- **Power.** Solar panels (weak at dawn and dusk, nothing at night), water wheels by a dam, and biogas
  charge 10 kWh battery banks. Machines draw from the bank within 8 tiles, by priority.
- **Materials.** Copper, iron ore, quartz to silicon, and sand to glass, all refined in a kiln. Mines dug
  into rocky ground and worked by miners supply ore.
- **Disasters.** Lightning wildfires, storm floods (dams stop them but flood land upstream), droughts,
  heatwaves and insect swarms.

## Code layout

| File | Purpose |
| --- | --- |
| `src/game.h` | Shared types and the `Game` state |
| `src/data.cpp` | Items, objects, crops, species, recipes |
| `src/world.cpp` | World generation, indoor detection, A* pathfinding |
| `src/sim.cpp` | Time, weather, temperature, crops, pests, spoilage, fire, power, disasters, goals |
| `src/entities.cpp` | Player actions, animal AI, workers, villagers, raiders |
| `src/sprites.cpp` | Procedural pixel-art atlas |
| `src/render.cpp` | World rendering and lighting |
| `src/ui.cpp` | HUD and panels |
| `src/save.cpp` | Versioned binary saves with bounds-checked loading |

### Developer flags

```sh
./build/hearthland --soak 40 --seed 7     # build a test farm, fast-forward 40 days, print daily stats
./build/hearthland --shot out.png --farm --tod 0.9 --panel inv   # screenshot and exit
./build/hearthland --powertest      # battery charge curve
./build/hearthland --watertest      # irrigation pressure, leaks, coyotes vs. a repairer
```
