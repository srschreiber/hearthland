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

- **Day/night and seasons.** A day lasts 8 real minutes and each season 7 days. Nights and winters
  are cold enough to kill you. Warm up by a fire or indoors (any area fully enclosed by walls and
  doors), and wear fur or wool clothing.
- **Food spoils.** Food rots in your pack. Granaries and root cellars slow it down, and a powered
  cold store nearly stops it. Chests left outdoors in the rain go damp, and mold spreads.
- **Farming.** Till, plant and water. Frost in late autumn kills unharvested crops, except
  frost-hardy cabbage. Each harvest drains the soil. Manure, compost, and beans grown in rotation
  restore it.
- **No pesticides.** Insects build up in large single-crop fields. Ponds attract frogs, leaf piles
  attract spiders, and tame ducks and fowl eat pests.
- **Animals.** Sheep (wool), buffalo (milk), ducks and jungle fowl (eggs), horses (riding), and dogs
  (guards). Wolves hunt livestock at night, and bears roam the forests. Livestock breed and drop
  manure, and need hay in troughs during winter.
- **Tribes.** Trading villages fly banners and have a trader marked with **$**. Raider camps are
  marked with skulls and red war paint, and they raid once your farm grows. Connect a trading
  village to a trading post with road for better prices. Recruit workers who farm, cut wood, herd,
  cook and guard. They need an indoor bed, food in storage and water.
- **Disasters.** Lightning wildfires, which spread through trees, so keep a firebreak around
  buildings. Also spring floods, droughts, heatwaves, blizzards and insect swarms.
- **Dams** stop spring floods and power water wheels, but their reservoir permanently drowns the low
  land upstream.
- **Power.** Water wheels, solar panels and biogas digesters feed batteries, lamps, heaters,
  sprinklers and cold stores.

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
./build/hearthland --shot out.png --farm --tod 0.9 --day 23 --panel inv   # screenshot and exit
```
