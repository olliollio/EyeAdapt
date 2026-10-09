# EyeAdapt – Admin Guide

For server admins. Work in progress: this covers the general info, how to use the mod and the light table. The full list of every setting and example presets will follow.

---

## 1. What the mod does

EyeAdapt adds simulated eye adaptation to DayZ nights. It is a client-side post-processing layer driven by a server config.

- Staying in the dark at night slowly improves the player's vision: brighter image, less colour, a slight blue shift.
- Light costs night vision. Standing at a fire, using a flashlight or looking into headlights pulls the player back towards "light-adapted".
- Dark-adapted eyes facing sudden bright light get a short glare (overexposure, bloom), strongest when the light is close and centred in view.
- Red light barely costs night vision, so red chemlights and the red headtorch have a real use.

**What it does not do:**

- **It never makes the night darker than your server's baseline.** Worst case is the plain vanilla night you configured. Adaptation only adds brightness on top.
- **It does nothing during the day.** The effect only runs between dusk and dawn (taken from the map's sunrise and sunset).
- **It does not touch your lighting config** (`cfggameplay.json` / `lightingConfig`). Your night stays as dark as you set it, the mod only lifts what the player's eyes can make of it.
- Available light still limits the gain. Full moon on an open field gives a lot, overcast new moon in a forest gives little, underground gives nothing (a flashlight is needed).

Fast server time (`serverTimeAcceleration`, `serverNightTimeAcceleration`) is supported: in `relativeToNight` mode the mod measures the real length of the night itself.

---

## 2. How to use

### 2.1 Installation

1. Add the mod to the server and the client mod list (`-mod=...`). It has to be loaded on **both** sides. The server holds the config and the saved player data, the client does the visuals.
2. Copy the mod's `.bikey` into the server's `keys` folder.
3. Start the server once. It creates `<server>/profiles/EyeAdapt/config.json` with the defaults.
4. Edit the file, restart the server.

### 2.2 The config file

`<server>/profiles/EyeAdapt/config.json`

- Created with defaults when missing. Every setting is a plain JSON key.
- **Edits need a server restart.** The server reads the file at start and sends the values to each client when it connects. Nobody needs to repack or update the mod to change a value.
- **Missing keys use their defaults**, so you can delete any key you don't want to touch.
- **Your edits are kept.** The file is only rewritten when it is missing or when the mod's config version is newer than the `Version` in your file. Then new keys are added, and your values for the existing keys stay.
- DayZ writes floats with a long tail (`0.15000000596046449`) and booleans as `1`/`0`. That is normal, you can type `0.15` and `true`.
- Do not change `Version` yourself.

If the file has a JSON error the server logs `config.json could not be read` and falls back to the defaults. Validate the file in any JSON checker after editing.

### 2.3 Player persistence

- The server saves each player's adaptation in `<server>/profiles/EyeAdapt/players/<id>.json` (hashed ID, no plain Steam ID). It saves on disconnect, on shutdown and every `SaveIntervalSeconds`.
- Time spent offline counts as time in the dark, so relogging gives nothing beyond what waiting would have given. Players without a record, and fresh characters, start at `StartAdaptation`.
- To reset one player, delete their file. To reset everyone, delete the `players` folder.
- The mod does not write into character persistence. Removing the mod later cannot break characters.

### 2.4 Debug keys

`DebugKeys` enables Numpad cheats on the client (set adaptation, switch the effect off, print the light list to the script log). **Set it to `false` on live servers.** It lets players cheat.

### 2.5 Quick tuning guide

| You want | Change |
|---|---|
| Faster or slower adaptation | `FullAdaptSeconds` or `FullAdaptNightFraction` |
| Brighter or subtler night vision | `MaxEvBoost` (stops, default 5) |
| Less grey, more colour | `MaxDesaturation` |
| Light costs less or more | `DoseSensitivity`, `FalloffExponent`, or a light's `Factor` in the table |
| Weaker or no glare | `GlareStrength` (0 = off) |
| Noisy night image (optional) | `NightGrain` (0 = off), `NightGrainSize`, `NightGrainFrequency` |
| Soft night image (optional) | `NightSoftness` (0 = off) |
| Flashlight in the face blinds | `FlashlightDazzle: true` (off by default) |
| Effect off entirely | `Enabled: false` |

---

## 3. Night adaptation (NA)

NA is a single value per player from 0 to 1, kept on the client and saved by the server.

- **0** = light-adapted. The player just left a lit place and sees the plain server night, exactly as vanilla.
- **1** = fully dark-adapted. The player gets the full night look and the full brightness gain.

Everything the player sees at night is driven by this value and by the available ambient light.

### 3.1 What happens to the player

| Situation | What happens |
|---|---|
| Dusk, staying outside | NA rises along with the falling light. Nothing noticeable, the eyes keep pace. |
| Leaving a lit house or fire into the dark | NA is low, so the player sees only the vanilla night. Vision improves over the next minutes. |
| Standing in the dark for a while | NA rises: image gets brighter, colours fade towards grey, a slight blue shift, a little more contrast to avoid a foggy look. |
| Standing at a campfire at night | NA falls fast and stays low. The player sees the fire but cannot see into the dark. People in the dark can see the player. |
| Looking at a light while fully adapted | Glare (see section 4), then NA is lower. |
| Using night vision | NA drops by the NV dose (section 5.5). |
| Logging in after a long break | Offline time counts as darkness, so NA is high ("you were sleeping"). Relogging quickly gains nothing. |

Colour returns near light: the closer and brighter the light, the more colour comes back, because the cones take over from the rods.

### 3.2 Adaptation time

How long full dark adaptation takes is set by `AdaptationMode`:

| Mode | Behaviour |
|---|---|
| `"fixed"` | Full adaptation after `FullAdaptSeconds` real seconds in the dark (default 240, 300–600 recommended for release). |
| `"relativeToNight"` | `FullAdaptNightFraction` of the whole night's real length (sunset to sunrise, not of the time left; 0.2 = 20 %). The mod measures the night length itself, on client and server, so `serverTimeAcceleration` and `serverNightTimeAcceleration` are covered. The result is clamped to `AdaptMinSeconds` (60) .. `AdaptMaxSeconds` (900). Until the night is measured, `FullAdaptSeconds` is used. |

Examples:

| Server | Suggested setup |
|---|---|
| Real time (night about 6–8 real hours) | `fixed`, 300–600 s. Real eyes need 30–45 min, which is far too long for a game. |
| Accelerated night (e.g. 48x, night about 10 min) | `relativeToNight`, 0.2–0.3, so adaptation scales with the short night. |
| Mild, forgiving | `fixed`, 120–180 s. |
| Hardcore | `fixed`, 600–900 s, or raise `LightSlowSeconds` and `DoseSensitivity`. |

### 3.3 The two stages

NA is built from two curves, which gives "quickly a bit better, then slowly much better":

| Stage | Setting | Meaning |
|---|---|---|
| Fast (pupil and cones) | `FastCurveSeconds` (20) | ~95 % complete after this long. |
| | `FastCurveWeight` (0.3) | Share of the total gain that comes from the fast stage (0..1). |
| Slow (rods) | `FullAdaptSeconds` or the night fraction | ~95 % complete after this long. Carries the remaining gain. |

**Falling is much faster than rising.** Light pulls both stages down quickly:

| Setting | Default | Meaning |
|---|---|---|
| `LightFastSeconds` | 2 | Fast stage drops this fast under light. |
| `LightSlowSeconds` | 8 | Slow stage drops this fast under light. |

So one minute at a fire can undo several minutes of waiting in the dark. A bigger value makes light less punishing.

### 3.4 How light sets the target

The scan (section 5) turns all lights in range into one saturated dose from 0 to 1. The dose defines a **target** for NA, and NA moves towards it:

- `MaxDoseEffect` (1.0) is how far a full dose pushes the target down. At 1.0 a full dose means NA falls all the way to 0, a lower value leaves some adaptation even next to a fire.
- With a dose of 0 (no light), the target is 1 and NA rises at the speeds above.
- `DoseSensitivity` (2.0) is the saturation: higher means a few lights already give a full dose.

### 3.5 Ambient light: how much gain NA can give

NA is only the *state of the eyes*. The brightness gain it produces depends on how much light there is to work with:

`gain = MaxEvBoost × ambient × NA`

`MaxEvBoost` (default 5 stops) is the gain under a clear full moon at full NA. The ambient factor lowers it:

| Condition | Setting | Effect |
|---|---|---|
| Moon | `MoonIntensityFull` (1.0) | Raw moon intensity that counts as full moonlight. |
| Clear, moonless sky | `StarlightShare` (0.6) | Gain relative to a clear full moon night. |
| Overcast | `OvercastClear` (0.35), `OvercastFull` (1.0), `OvercastMinShare` (0.25) | Fully overcast sky gives at least this share of a clear sky. |
| Darkest possible sky | `MinAmbientGain` (0.2) | Floor, as a share of `MaxEvBoost`. |
| Underground | `UndergroundGain` (0) | Livonia bunkers, Sakhal. 0 = no gain, a flashlight is needed. |

`AmbientSmoothSeconds` (2) smooths changes. Interiors, forests and dark configs therefore stay dark, and adaptation only amplifies the light that exists.

### 3.6 The night gate

The effect only runs at night. A night factor from 0 (off) to 1 (full) scales the whole effect, taken from the map's sunrise and sunset. Sunrise and sunset are looked up for the current in-game date, so they move through the year. All values are in **in-game hours**, not real time.

| Phase | Setting | Meaning |
|---|---|---|
| Evening | `DuskOffsetHours` (0) | The fade-in starts this many hours after sunset (negative = before). |
| Evening | `DuskHours` (2) | The factor rises smoothly from 0 to 1 over this many in-game hours. |
| Morning | `DawnOffsetHours` (-1.5) | The fade-out starts this many hours after sunrise. The default of -1.5 starts it 1.5 h before sunrise. |
| Morning | `DawnHours` (1.5) | The factor falls smoothly from 1 to 0 over this many in-game hours. |

With the defaults the effect is off at sunset, reaches full strength 2 h later, holds through the night, starts fading 1.5 h before sunrise and is gone at sunrise. The default ends it early because morning twilight is already bright and a full boost looked washed out in testing.

To keep the effect running longer into the morning, raise `DawnOffsetHours` (for example to -0.5), keeping `DawnHours` so the fade still has time. The two settings are independent: the fade-out ends at `sunrise + DawnOffsetHours + DawnHours`.

During the day the mod does nothing and vanilla is untouched.

### 3.7 Visuals

At full NA (before ambient scaling):

| Effect | Setting | Default |
|---|---|---|
| Brightness boost | `MaxEvBoost` | 5 EV |
| Desaturation | `MaxDesaturation` | 0.6 (scaled down near light) |
| Contrast boost | `MaxContrastBoost` | 0.15 (counters the grey look of a strong boost) |
| Blue shift | `BlueShift` | 0.1 (0 = off) |
| Colour return smoothing | `ColorReturnSmoothSeconds` | 0.5 |

Blur is not used. The mod does not simulate the blind spot in the centre of the view either, because it would make aiming feel broken.

**Optional extras (both off by default).** They follow the rod vision, like the desaturation: strongest at full adaptation, fading near light, off in daylight and off while night vision is on.

| Effect | Setting | Default | Meaning |
|---|---|---|---|
| Film grain | `NightGrain` | 0 (off) | Grain intensity at full adaptation, 0..1. Dark-adapted vision is noisy. The grain also makes gamma and brightness filters less useful for cheating. Start around 0.1–0.2 and look at it in game. |
| | `NightGrainSize` | 2.75 | 1..9. Larger = coarser grain (vanilla default). |
| | `NightGrainFrequency` | 20 | 1..1000. How fast the grain changes. High = a shimmering "visual snow", low = a calmer grain. |
| Soft look | `NightSoftness` | 0 (off) | 0..1. Rods see little fine detail, so the image gets a soft glow at full adaptation. It is done with bloom (raised bloom, lowered threshold), not a blur filter, so aiming stays sharp. |

Notes:
- Vanilla film grain only shows when the engine's night-vision noise multiplier is non-zero. While grain is on, the mod sets that multiplier to 1 at a low priority. Real night vision (goggles and scopes) uses its own, higher value and is not changed.
- If another mod already uses film grain at night (for example a lighting overhaul with `filmGrainIntensity`), the two add up. Lower `NightGrain` or leave it off.
- Both effects are untested at this point. Tuning values are still to be found.

---

## 4. Glare and dazzle

Glare is the reverse direction: dark-adapted eyes facing sudden bright light. It only happens when the player is **actually dark-adapted** and the light is **roughly centred in view**.

### 4.1 When it triggers

All of these must be true:

- The player is at night and not in the daylight fade.
- NA lags behind the light by at least `GlareTriggerLevel` (0.3). In practice that means a well-adapted player faces a bright light. A light-adapted player barely notices.
- The light is centred in view. `GlareViewPower` (8) sets how centred: strength is `cos(angle)^power`, so 8 gives half strength at about 30° off centre. Lights behind or to the side do nothing.
- The glare recovery is not blocking it (section 4.4).

### 4.2 What the player sees

Glare has two phases. Both scale with how adapted the player was (fully adapted = full effect) and with `GlareStrength` (1.0 = default, 0 = glare off entirely).

**Phase 1: blowout.** A short, hard overexposure.

| Setting | Default | Meaning |
|---|---|---|
| `GlareHoldSeconds` | 0.6 | Fully blown out, the player can hardly see. |
| `GlareFadeSeconds` | 1.0 | Fade out into phase 2. |
| `GlareEvBoost` | 6 | Extra EV stops while blown out. |
| `GlareOverlay` | 1.0 | Linear white-out (a flashbang is 1.0). |
| `GlareContrastLoss` | 0.3 | Contrast reduction (detail loss). |

**Phase 2: glow.** A bloom halo around bright light, fading while the eyes adapt.

| Setting | Default | Meaning |
|---|---|---|
| `GlowFadeSeconds` | 6 | Halo is ~95 % gone after this. |
| `GlareBloomIntensity` | 2.0 | Added to the bloom intensity (engine max 4). |
| `GlareBloomThresholdDrop` | 0.3 | Lowers the bloom threshold, so more of the image blooms. |

Colour returns in the lit area during glare. `GlareSmoothSeconds` (0.15) smooths the fades. `BloomBaseIntensity` and `BloomBaseThreshold` are the vanilla glow values the boost is added to; only change them if another mod alters the glow settings.

### 4.3 What happens to NA

Looking into the light also lowers NA through the normal dose (section 3.4), so the player stays impaired after the visual glare is gone, for as long as it takes to adapt again.

### 4.4 Glare recovery (no free flashbangs)

`GlareRecoverySeconds` (30): after a blowout or dazzle, the next one is weaker. Right after one ended it is 0, and after the full time it is back to full. Eyes that were just blinded are less sensitive. This stops a player from being blinded again and again while walking along headlights or a row of lamps. Set it to 0 to switch the limit off.

### 4.5 Flashlight dazzle (optional, off by default)

`FlashlightDazzle: true` makes someone else's flashlight, headtorch or headlights in your face act like a short flashbang-style white-out (without sound). This is a gameplay decision, so it is off by default.

| Setting | Default | Meaning |
|---|---|---|
| `DazzleThreshold` | 0.3 | How strong the beam in the face must be. A centred flashlight reaches this at about 7 m with the defaults. |
| `DazzleMinAdaptation` | 0.3 | No dazzle below this strength. Strength is NA × glare recovery, so light-adapted eyes barely notice and fully adapted eyes get the full white-out. |
| `DazzleHoldSeconds` | 1.0 | Full white-out (vanilla flashbang: about 5.5 s). |
| `DazzleFadeSeconds` | 2.0 | Fade back (vanilla flashbang: about 2.5 s). |
| `DazzleRearmSeconds` | 2.0 | The beam must be out of the face this long before it can dazzle again. A lamp held on the player dazzles once, then the eyes have adjusted. |
| `DazzleOverlay` | 0.9 | White-out strength (flashbang: 1.0). |
| `DazzleEvBoost` | 5 | Extra EV stops at full dazzle. |

Own lights never dazzle the player. Dazzle only reacts to other people's beams, and it counts as a glare for the recovery limit.

---

## 5. Light handling

### 5.1 How a light is judged

Every light near the player adds a **dose**. All doses are added up and run through a saturation curve, so 5 lamps and 50 lamps feel about the same. A light's dose depends on:

- **Brightness and the table:** the light's own brightness times the `Factor` from the light table.
- **Distance:** it falls off with distance (`FalloffExponent`). Beyond `ScanRadius` (default 40 m), beyond the light's own radius, or beyond the table's `Range`, it counts as 0. Contributions under `DoseThreshold` count as 0.
- **View angle:** lights in the middle of the view count fully, lights to the side less, lights behind you at least `ViewMinFactor`.
- **Occlusion:** walls and terrain between you and the light reduce it (`OccludedFactor`), trees and bushes too (`VegetationOccludedFactor`), and so does grass in the line of sight (`GrassOccludedFactor`).
- **Colour:** red light is discounted (`RedLightFactor`, see Type below).
- **Own lights:** your own flashlight, headtorch or weapon light counts only `OwnLightFactor` (10 %). You are not looking into it. Own point lights, such as a torch or chemlight in your hand, count by distance like any other.

The scan runs every `ScanInterval` (0.25 s).

### 5.2 The light table

`Lights` in `config.json` is a list of entries:

```json
{ "ClassName": "FlareLight", "Factor": 10.0, "Type": "full", "Range": 100.0 }
```

| Field | Meaning |
|---|---|
| `ClassName` | Script class of the light. Also matches subclasses, and the most specific entry wins. `CarLightBase` covers every front car light, and a more specific entry for one car model would override it. |
| `Factor` | Brightness multiplier. `1` = normal, `0` = ignore the light completely. |
| `Type` | `"auto"`: red discount if the light's colour is red. `"red"`: always the red discount. `"full"`: never a discount (for bright or broadband light that bleaches the eyes even when it looks red). |
| `Range` | Metres the light counts up to. `0` = use `ScanRadius`. It is always also limited by the light's own radius. |

**A light with no entry counts with Factor 1, Type `"auto"`, Range 0.** That is the right default for most lights, so the table only lists exceptions.

Lookups are cached per class, so the table costs almost nothing per scan.

**Adding modded lights:** with `DebugKeys` on, press Numpad 9 next to the light. It prints every contributing light to the script log, with the class name and the table entry it matched (or "no table match"). Copy the class name into a new entry.

### 5.3 General lights (no special handling)

These use the default (Factor 1, `"auto"`, own radius). Colour decides whether the red discount applies.

| Light | Notes |
|---|---|
| Flashlight, pistol light, rail light | Spot lights. Directional: they only matter if the beam points at the player. Own light counts 10 %. |
| Headtorch | Spot light. The normal one is white. **`Headtorch_Black` is red**, so it gets the red discount. |
| Chemlights | White, green, blue and yellow count fully. **Red chemlight gets the red discount.** |
| Stove, blowtorch | Tiny blue point lights (radius 1 m). Practically no effect. |
| Alarm light, buoys | Red lights. Red discount applies. |
| Underground entrances, warhead storage, roadblock blinker, Christmas lights, event lights | Weak map and event lights, default handling. |
| Infected and animal eye glow | Default handling, small radius. |

### 5.4 Specially handled lights (default table entries)

| ClassName | Factor | Type | Range | Why |
|---|---|---|---|---|
| `MuzzleFlashLight`, `MuzzleFlashLight_1`, `MuzzleFlashLight_2` | 0 | – | – | Muzzle flashes last 10–80 ms and would make every shot a dose. Ignored. Vanilla's `_1` and `_2` do not inherit from `MuzzleFlashLight`, so all three are listed. |
| `FireplaceLight` (campfire, barrel, oven, fireplace), `BonfireLight`, `TirePileLight`, `TorchLight`, `PortableGasLampLight` | 1 | `full` | – | Flames look orange-red in RGB, which the colour check would treat as red. They are broadband light and do bleach the eyes, so they get no red discount. |
| `CarLightBase` (all front headlights) | 5 | `auto` | 70 | Bright focused beams that glare dark-adapted eyes from 50–70 m. |
| `CarRearLightBase` | 1 | `auto` | – | Brake and reverse lights. Red brake lights get the red discount by colour. |
| `FlareLight` (flare gun, all colours) | 10 | `full` | 100 | Lights a whole area from the sky. Too bright to keep the red discount, so a red flare costs the same as a green one. |
| `RoadflareLight` | 1 | `full` | – | Red, but bright and broadband, so no red discount. |
| `ExplosionLight` (fireworks launcher) | 250 | `full` | 60 | The light's nominal brightness is only 0.05, but vanilla sets its colours on a 0–255 scale, so the real light is about 250 times stronger. 250 brings it to roughly flare strength. All firework colours count the same. |
| `EntranceLight` (Livonia bunker entrance and stairs, incl. `EntranceLightStairs1/2`, `Main1/2`) | 4 | `auto` | – | These vanilla lights are very dim (brightness 0.5, radius 5–10 m), so at factor 1 they barely moved NA. Underground there is no EV boost, so the effect shows when the player comes back up. |
| `SpotlightLight` (generator spotlight) | 1 | `auto` | 60 | Own radius is 60 m. |

### 5.5 Hard-coded in the mod (not in the table)

- **`PersonalLight` is always ignored.** It is a weak fill light that follows the player's camera. If it counted, every player would permanently be lit. This is separate from the vanilla `disablePersonalLight` setting in `serverDZ.cfg`, which still works as before.
- **Night vision (NV goggles and scopes)** is not a light. Powered goggles cost `NightVisionDose` (default 0.5, both eyes), a powered NV scope costs `NightVisionScopeDose` (default 0.25, only the aiming eye). `0` makes it free. Either way, glare and dazzle stay off while NV is on.
- **Flash grenade hit.** The grenade's light lives only 0.35 s, which the light scan could miss. So the mod does not rely on the scan: when vanilla decides the player is hit (in range and with line of sight to the head), NA is reduced by `FlashbangAdaptationLoss` (default 1.0 = back to zero, 0 = off). The white-out and the ringing are vanilla's. After that NA recovers normally in the dark. `FlashGrenadeLight` is not in the light table.
- **Sitting in a car with its headlights on** costs at least `OwnVehicleLightDose` (default 0.5), because you look at the lit road. Your own headlights are not treated as a light in your face.
- **Debug and glare rules** such as `GlareViewPower` and the trigger levels are normal settings, not table entries.

### 5.6 Known limits

- **Very short flashes are not detected.** Explosions, grenades and gas shells (`ExplosiveLight`, `ShellLight`) live only 0.15–0.35 s, so the 0.25 s scan often misses them. They are not in the table. A fix is planned. Flash grenades are handled separately (see section 5.5). Fireworks (`ExplosionLight`) last 2.1 s and are handled.
- **Lights that only exist as glowing textures** (street lamps and lit windows on vanilla maps) are not lights in the game, so they cannot be detected and do not cost night vision. This matches vanilla, where they light nothing.
- **Config-only lights** (the pumpkin helmet) are not scripted lights and are not seen by the mod.
- **Modded maps and items** that use their own light classes work, but need a table entry if they should count differently from the default.

---

## 6. Performance

The mod was designed so that the cost stays small and does not grow with the size of the map. **It has not been benchmarked on a populated server yet.** The notes below describe where work happens and which settings control it. Treat the numbers as design intent until measured.

### 6.1 Where the work happens

| Side | What runs | How often | Cost driver |
|---|---|---|---|
| Client | Light scan | every `ScanInterval` (0.25 s) | Number of lights near the player |
| Client | Adaptation update, visuals | every frame | Constant, a handful of arithmetic operations and a few post-processing parameters |
| Client | Report to the server | every `ReportIntervalSeconds` (15 s) | One tiny RPC |
| Server | Save all online players | every `SaveIntervalSeconds` (120 s) | One small JSON file per online player |
| Server | Save one player | on disconnect | One small JSON file |
| Server | Night length measurement | per tick | A few arithmetic operations |
| Server | Config sync | once per connecting client | One RPC |

The visual effect itself is client-side post-processing, so it adds no server load. The server never scans lights and never needs to know where players are looking.

### 6.2 The light scan

- **No world search.** The mod does not query objects around the player. Every dynamic light registers itself in a list when it is created and removes itself when it is destroyed, so the scan only loops over that list: typically a handful to a few dozen entries, never the hundreds of trees, buildings and loot items around.
- **Cheap checks first, expensive checks last.** For each light the scan checks, in this order: is it on, is it a personal light, the table entry (cached per class), the distance, the view angle and beam direction, the dose threshold, and only then occlusion. Most lights are dropped by the cheap distance test, because they are further away than their radius, `Range` or `ScanRadius`.
- **Occlusion is the expensive part.** Each light that survives the cheap checks costs one physics raycast, plus the grass check described below. A light with `Factor` 0 in the table, or one outside its range, never reaches that step.
- **Two passes, strongest light first.** The scan first runs the cheap checks on all lights and sorts the survivors by dose. Only then does it do the occlusion tests, from the strongest light down. This keeps a big lit base cheap:
  - **Budget:** at most `MaxOcclusionChecks` (default 8) raycasts per scan. Lights over the budget count as blocked (`OccludedFactor`) instead of costing a raycast. Because the strongest lights are checked first, the ones that matter are tested. Set it to 0 for no limit.
  - **Saturation stop:** once the total dose is about 98 % saturated, the remaining lights are skipped, since they cannot change the result visibly. Lights that matter for glare (centred in view) and other people's spot lights (for the optional dazzle) are still tested until the glare dose is saturated as well.
- **Grass check.** If the raycast is free, the line to the light is sampled every `GrassSampleStep` (2 m): one terrain height lookup per sample, and a surface name lookup only where the line runs lower than the grass is tall. A light 40 m away costs about 20 samples.
- **The ambient light reading is cheap.** Each scan also reads the moon, the overcast value and the underground state for the ambient gain. These are plain getters on the world and weather, with no object search. The only world search in the mod is the Numpad `/` test headlights key (`DebugKeys` only), which queries objects once per key press.
- **The scan also runs by day.** Adaptation keeps running by day, so the scan and the update also run in daylight. Only the visuals are gated by the night.

In a normal scene, only a few lights pass all the cheap checks, so a scan costs a few raycasts. A busy scene, such as a big lit base with many lamps in view or a convoy of cars with headlights, costs more, but it is limited by the number of lights inside their radius and `ScanRadius`.

### 6.3 Settings that affect performance

| Setting | Effect on cost |
|---|---|
| `ScanInterval` | Higher = fewer scans. At 0.5 s the cost halves, but glare starts noticeably late. Keep at 0.25 unless needed. |
| `ScanRadius` | Smaller = fewer lights pass the distance test. Also limited by each light's own radius. |
| Light table `Range` | A small `Range` on a bright light culls it early. |
| Light table `Factor: 0` | Ignores the light before any other work. Use it for lights that should never count. |
| `MaxOcclusionChecks` | Upper limit of raycasts per scan (default 8, 0 = no limit). Lower = cheaper in lit bases, but more lights are assumed blocked. |
| `UseOcclusion` | `false` removes all raycasts and the grass check. Cheapest option, but lights behind walls then count fully. This is the biggest saving. |
| `UseGrassOcclusion` | `false` removes the terrain sampling but keeps the raycast. |
| `GrassSampleStep` | Larger = fewer samples per light (4 m halves the grass cost), but it can miss narrow grassy crests. |
| `DoseThreshold` | Higher = weak lights are dropped before occlusion is tested. |
| `SaveIntervalSeconds` | Larger = fewer disk writes. 0 = save only on logout and shutdown, with the risk of losing recent values after a crash. |
| `ReportIntervalSeconds` | Larger = fewer RPCs. The server only keeps the last value anyway. |
| `DebugKeys` | `false` on live servers (also for cheating reasons). |

### 6.4 Server load

- **File writes.** A save writes one small JSON file for each player who is online. With the default interval, 60 players means 60 small writes every two minutes. Raise `SaveIntervalSeconds` on very full servers if the profile folder sits on a slow disk.
- **Disk space.** One small file per player who has ever played. Delete the `players` folder to reset everything.
- **Network.** One config RPC per connecting client, and one small RPC per client every 15 s. This is negligible compared with vanilla traffic.
- **No per-player server tick.** The server does not run a loop per player. Adaptation lives on the client, the server only stores it.

### 6.5 Logging

The mod prints two `[EyeAdapt]` lines to the client script log every 5 seconds. They are useful while testing but add up in a log file over a long session. A switch (`DebugLogging`) is planned. Until then, the log lines do not affect frame rate noticeably, only log size.

### 6.6 How to measure on your server

1. Compare the frame time at night with and without the effect: Numpad 7 toggles it (needs `DebugKeys`).
2. Stand in the worst case for your server (lit base, parked cars with lights on, several players with flashlights) and press Numpad 9 once. The log shows how many lights are registered, how many passed the cheap checks, how many raycasts were used, and how many lights were skipped because the dose was saturated.
3. If the frame rate drops, lower `MaxOcclusionChecks` first, then try `UseOcclusion: false`. If that fixes it, raise `GrassSampleStep` or disable only `UseGrassOcclusion` instead.
4. Watch the server frame time while players disconnect and during the periodic saves. A hitch every `SaveIntervalSeconds` means the disk is too slow, so raise the interval.