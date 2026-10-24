# Quake Visuals — open items to discuss

Quake Visuals (`r_quakeVisuals 1`, or **Setup → System → Graphics → Quake Visuals**) draws Doom maps with Quake 3 textures, item models and lights. The pairings live in `data/q3doom/quakevisuals.dat`; edit it, then `vid_restart`.

Below is what has **no sensible Quake 3 asset** in our paks, or where I made a judgment call. For each, tell me which way you want to go.

## 1. Decorations (no Quake 3 models available)

The Quake 3 paks have **no prop models**: Quake 3's lamps, torches, skulls and pillars were compiled into its maps' geometry. Only their textures shipped. So every Doom decoration keeps its Doom sprite:

- **Lamps, candles and torches** (floor lamp, tech lamps, candle, candelabra, the six torches, burning barrel): Doom sprite, plus a glow and a colored baked light.
- **Pillars and columns** (tall/short green and red pillars, heart and skull columns, tech column).
- **Trees and stalagmites** (big tree, brown stump, stalagmite).
- **Skulls and bodies:** skulls on sticks, the pile of skulls, impaled and hanging bodies, dead monsters and players.
- **Pools of blood and gore:** could use Quake 3's gib models (skull, chest, intestine, arm, leg…) instead of the sprites.
- **Exploding barrels.**
- **Evil eye and floating skull rock.**

Options: keep the sprites (current), build simple models ourselves, or source assets with a compatible license.

## 2. Items with no Quake 3 equivalent (Doom sprites kept)

- **The six keys and skull keys:** Quake 3 has no keys. The red and blue CTF flags exist; there is no yellow one.
- **Backpack.**
- **Computer area map.**
- **Light amplification visor.**
- **Invulnerability sphere:** in base Quake 3 only in Team Arena, which we don't have.

## 3. Item choices to confirm

| Doom | Quake 3 model |
|---|---|
| Health bonus | Small (green) health |
| Stimpack | Medium health |
| Medikit | Large health |
| Soulsphere | Mega health |
| Armor bonus | Armor shard |
| Green armor | Yellow armor |
| Blue armor | Red armor |
| Clip / box of bullets | Machinegun ammo (box 1.3× larger) |
| Shells / box of shells | Shotgun ammo (box 1.3× larger) |
| Rocket | A single rocket |
| Box of rockets | Rocket ammo |
| Cell / cell pack | Plasma ammo (pack 1.3× larger) |
| Berserk | Quad damage |
| Partial invisibility | Invisibility |
| Radiation suit | Battle suit |

**Weapon pickups:**
- With Quake 3 weapons: the weapon you get.
- With Doom weapons: the closest-looking Quake 3 weapon. The chaingun shows as the machinegun, the chainsaw as the gauntlet.

## 4. Textures kept as Doom on purpose

- **Switches (SW1/SW2):** you need to recognize them. Quake 3 has very few button textures.
- **Key-colored door frames (DOORBLU, DOORRED, DOORYEL) and exit signs.**
- **See-through middle textures** (MIDGRATE, MIDBRN1, MIDVINE, BRNSMAL, BRNBIG): they need their holes.
- **Warning signs (`*POIS`, GRAYDANG) and PLANET1.**

## 5. Texture matches I'm least sure of

- **Crates:** Quake 3 has no crates. They use metal panels (`base_trim/panel02`).
- **Wood (WOOD*, BIGDOOR5/6, wooden floors):** Quake 3 only has wooden ceilings (`gothic_ceiling/woodceiling1a`), which are used for all of them.
- **Computers (COMPUTE*, COMPSTA*, CONS1):** machinery panels (`base_wall/atech1_*`). There are no animated screens.
- **Doors (BIGDOOR1–4, DOOR1/3):** gothic door panels. Quake 3's tech doors are big arches, which don't fit Doom's door sizes.
- **Faces and figures (SP_DUDE*, MARBFACE, SKINFACE):** gothic skull reliefs and skin.
- **Water (FWATER):** Quake 3's water is see-through. A Doom floor has nothing below it, so it's drawn opaque. The same goes for every liquid.
- **Skies:**
  - E1: a blue cloudy sky
  - E2: the red kill sky
  - E3: the hell sky
  - E4: Tim's hell sky
- **Brightness:** Quake 3 textures are darker than Doom's, so the sector light is multiplied by `r_quakeVisualsBright` (1.5).

## 6. Lighting

- **Baked point lights:** at lamps, candles, torches and lit ceilings, and at glowing floors (nukage, lava, teleporter gates and lit floor panels).
  - They are colored, and walls and closed doors block them.
  - They are computed when the map loads (all 36 maps take 2.3 s in total).
  - They light walls, floors and ceilings, **not** monsters, items or decorations.
- **Sector lighting:** still drives the overall light. Doom's flickering and blinking lights still work.
- **Not done:**
  - shadows from monsters or players
  - lights that move with doors and lifts (the lighting uses each sector's starting position)
  - lights from lit wall textures, which glow but don't cast light

Do you want monsters and sprites lit by the point lights too?

## 7. Out of scope

Monsters, weapons in your hands, and the HUD stay as they are: monsters are Doom sprites per the goals, and the weapon set is its own option.
