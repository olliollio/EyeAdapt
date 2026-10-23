# EyeAdapt – Admin Guide

For server admins. Work in progress: sections 1-6 explain the mod, section 7 lists every setting with its default, section 8 has example presets. Defaults are those of config version 26 (`EyeAdaptSettings.c`). If a value here differs from your generated `config.json`, the file is right.

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

- The server keeps each player's adaptation (keyed by hashed ID, no plain Steam ID) and notes the time when they disconnect. All records are saved to one file, `profiles/EyeAdapt/adaptation.json`, every 60 s and on shutdown.
- `LogoutMode` decides what a returning player gets:

  | Mode | Returning player gets |
  |---|---|
  | `"keep"` (default) | The NA they logged off with. Logged off at a fire = nearly blind on return, logged off in the dark = still adapted. |
  | `"fixed"` | Offline time counts as darkness, but at most `LogoutAdaptation` (default 0.2). |
  | `"sleep"` | Offline time counts as darkness, a long break means full night vision ("you were sleeping"). |

  In every mode a quick relog gains nothing beyond what waiting in the dark would have given.
- **A server restart is a time freeze.** Offline time continues where it stopped, the downtime doesn't count. Players who were online when the server went down return with the NA they had.
- New characters (first spawn, after death) start at `StartAdaptation`. With the default 0.2 that is a dark start, like stepping out of a lit room: the fast stage gives a first improvement within a minute, full night vision takes `FullAdaptSeconds` (default 10 min).
- Records that no longer matter are dropped: in `"sleep"` and `"fixed"` once offline time has taken the player to the same value as no record (1.0 or `LogoutAdaptation`), in `"keep"` after `RecordKeepDays` (30) of offline time while the server runs; such a player returns at `StartAdaptation`. Deleting `adaptation.json` while the server is stopped resets everyone the same way.
- The mod does not write into character persistence. Removing the mod later cannot break characters.

### 2.4 Debug keys

`DebugKeys` enables Numpad cheats on the client. **Set it to `false` on live servers.** It lets players cheat. The server setting is synced to the client, so the keys only work when the server has it on. The keys are ignored while any menu or the chat is open, and every key shows a short on-screen message.

| Key | Action |
|---|---|
| Numpad 1 | Set NA to 0 (just left a lit room) |
| Numpad 3 | Set NA to 1 (fully dark-adapted) |
| Numpad 7 | Effect on/off, for an A/B comparison with the vanilla night |
| Numpad 9 | Run a light scan now and print every contributing light to the script log (class name and matched table entry, see section 5.2). Also shows the total dose and the strongest light on screen |
| Numpad * | Force full glare on/off, to judge the glare look |
| Numpad / | Toggle client-local test headlights on the nearest car within 30 m, to test oncoming headlights alone. The real lights go off when you get out |

**Testing night vision (NV goggles or NV scope).** There is no extra key for it. Set the starting value with Numpad 1 or 3, put on powered goggles (or aim through a powered NV scope), and watch the NA value on screen (`ShowAdaptation`, updated every second) or in the `[EyeAdapt]` log line (`DebugLogging`, every 5 seconds). Goggles count as a light dose of `NightVisionDose` (both eyes), a scope as `NightVisionScopeDose` (only the aiming eye), so NA falls towards the matching target instead of rising. While NV is on, glare, dazzle, grain and the soft look are off. Take the goggles off and NA recovers normally in the dark. To test the cost cleanly, start from Numpad 3 and compare the value after a minute with NV on against a minute without.

### 2.5 Quick tuning guide

| You want | Change |
|---|---|
| Faster or slower adaptation | `FullAdaptSeconds` or `FullAdaptNightFraction` |
| Brighter or subtler night vision | `MaxEvBoost` (stops, default 5) |
| Less grey, more colour | `MaxDesaturation` |
| Light costs less or more | `DoseSensitivity`, `FalloffExponent`, or a light's `Factor` in the table |
| Switch glare on or make it weaker | `GlareStrength` (default 0 = off, 1.0 = full) |
| Noisy night image (optional) | `NightGrain` (0 = off), `NightGrainSize`, `NightGrainFrequency` |
| Soft night image (optional) | `NightSoftness` (0 = off) |
| Flashlight in the face does not blind | `FlashlightDazzle: false` (on by default) |
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
| Logging in again | Depends on `LogoutMode` (2.3): by default the NA you logged off with. Relogging quickly gains nothing. |

Colour returns near light: the closer and brighter the light, the more colour comes back, because the cones take over from the rods.

### 3.2 Adaptation time

How long full dark adaptation takes is set by `AdaptationMode`:

| Mode | Behaviour |
|---|---|
| `"fixed"` | Full adaptation after `FullAdaptSeconds` real seconds in the dark (default 600 = 10 min). |
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

`MaxEvBoost` (default 5 stops) is the gain at full NA under the brightest sky the ambient factor allows. A clear full moon gives `FullMoonShare` of it, a clear new moon `StarlightShare`, phases in between are interpolated:

| Condition | Setting | Effect |
|---|---|---|
| Moon | `MoonIntensityFull` (1.0) | Raw moon intensity that counts as full moonlight. |
| Clear full moon | `FullMoonShare` (0.8) | Share of `MaxEvBoost` under a clear full moon. Lower it if full-moon nights look too bright (real-time servers with dark nights). 1 = full boost. |
| Clear, moonless sky | `StarlightShare` (0.6) | Share of `MaxEvBoost` under a clear new moon, same scale as `FullMoonShare`. |
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
| Contrast boost | `MaxContrastBoost` | 0.125 (counters the grey look of a strong boost) |
| Black point | `NightBlackLevel` | 0.0 (off; optional, pulls the sky back to black after the EV boost) |
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
- Vanilla film grain only shows when the engine's night-vision noise multiplier is non-zero. While grain is on, the mod sets that multiplier to twice the current grain amount at a low priority, so it grows and shrinks with adaptation: no grain at zero NA, the full `NightGrain` amount at full NA. Real night vision (goggles and scopes) uses its own, higher value and is not changed.
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

Glare has two phases. Both scale with how adapted the player was (fully adapted = full effect) and with `GlareStrength` (1.0 = full glare, **default 0 = glare off**; set e.g. 1.0 to switch it on).

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

`FlashlightDazzle: true` makes someone else's flashlight, headtorch or headlights in your face act like a short flashbang-style white-out (without sound). This is a gameplay decision, so admins can switch it off with `FlashlightDazzle: false`. It is on by default in the test release.

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
- **Occlusion:** walls and terrain between you and the light reduce it (`OccludedFactor`), trees and bushes too (`VegetationOccludedFactor`), and, if `UseGrassOcclusion` is on (off by default), so does grass in the line of sight (`GrassOccludedFactor`, only for lights up to `GrassLightMaxHeight` above the ground). The ray stops `OcclusionEndOffset` (0.5 m) before the light so the light's own housing doesn't block it. Grass is not geometry, so the line is sampled instead: grass counts where the line runs lower above the ground than `GrassHeight` (0.5 m), or `TallGrassHeight` (1.0 m) on tall grass surfaces. Grass within `GrassSkipNearEye` (1 m) of you and within `GrassSkipNearLight` (3 m) of the light is ignored, because headlights and your own feet always stand in it.
- **Colour:** red light is discounted (`RedLightFactor`, see Type below).
- **Own lights:** your own flashlight, headtorch or weapon light counts only `OwnLightFactor` (10 %). You are not looking into it. Own point lights, such as a torch or chemlight in your hand, count by distance like any other.

The scan runs every `ScanInterval` (0.25 s) at night and once a second by day (glare and dazzle are off by day, so the scan only feeds the adaptation value).

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
| `FireplaceBase` (smouldering fireplaces, barrels, ovens: fuel gone, still hot) | 0.1 | `full` | 8 | Vanilla has no light for embers, so this row is not a light class: the scan counts them as weak point lights with the campfire brightness × Factor. 0 = embers do not count. Without a row the same defaults apply. |
| `BarrelHoles_ColorBase`, `FireplaceFireBarrel` (barrel with holes, map fire barrel) | 0.5 | `full` | – | Enclosed fires: the row is looked up by the class the light is attached to, not by the light class. Factor = share of the light that gets out. Also applied to the embers of that fire. |
| `FireplaceIndoor`, `OvenIndoor` (in-building fireplace, wall oven) | 0.3 | `full` | – | Same, enclosed. |
| `FireplaceOven` (stone oven) | 0.3 | `full` | – | Not a class: stands for a stone-ring fireplace in oven state (same class as the campfire). |

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

The mod was designed so that the cost stays small and does not grow with the size of the map. **Measured on one client (6.6): about 0.03 ms per frame by day and 0.06–0.07 ms per frame at night in a base lit by 12 torches, under 0.6 % of the frame.** It has not been measured on a populated server yet. The notes below describe where work happens and which settings control it.

### 6.1 Where the work happens

| Side | What runs | How often | Cost driver |
|---|---|---|---|
| Client | Light scan | every `ScanInterval` (0.25 s) at night, 1 s by day | Number of lights near the player |
| Client | Adaptation update, visuals | every frame | Constant, a handful of arithmetic operations and a few post-processing parameters (by day the glare math is skipped, and post-processing values are only sent when they change); the night factor is read once a second |
| Client | Report to the server | every `ReportIntervalSeconds` (15 s), at night only | One tiny RPC |
| Server | Remember one player | on report and disconnect | One small record in memory |
| Server | Save all records | every 60 s and on shutdown | One small JSON file, a few dozen bytes per player |
| Server | Night length measurement | per tick, only in `relativeToNight` mode | A few arithmetic operations |
| Server | Config sync | once per connecting client | One RPC |

The visual effect itself is client-side post-processing, so it adds no server load. The server never scans lights and never needs to know where players are looking.

### 6.2 The light scan

- **No world search.** The mod does not query objects around the player. Every dynamic light registers itself in a list when it is created and removes itself when it is destroyed, so the scan only loops over that list: typically a handful to a few dozen entries, never the hundreds of trees, buildings and loot items around.
- **Cheap checks first, expensive checks last.** For each light the scan checks, in this order: is it on, is it a personal light, the table entry (cached per class), the distance, the view angle and beam direction, the dose threshold, and only then occlusion. Most lights are dropped by the cheap distance test, because they are further away than their radius, `Range` or `ScanRadius`.
- **Occlusion is the expensive part.** Each light that survives the cheap checks costs one physics raycast, plus the grass check described below. A light with `Factor` 0 in the table, or one outside its range, never reaches that step.
- **Two passes, strongest light first.** The scan first runs the cheap checks on all lights and sorts the survivors by dose. Only then does it do the occlusion tests, from the strongest light down. This keeps a big lit base cheap:
  - **Budget:** at most `MaxOcclusionChecks` (default 8) raycasts per scan. Lights over the budget count as blocked (`OccludedFactor`) instead of costing a raycast. Because the strongest lights are checked first, the ones that matter are tested. Set it to 0 for no limit.
  - **Saturation stop:** once the total dose is about 98 % saturated, the remaining lights are skipped, since they cannot change the result visibly. Lights that matter for glare (centred in view) and other people's spot lights (for the optional dazzle) are still tested until the glare dose is saturated as well.
- **Grass check.** Off by default (`UseGrassOcclusion`). When on, and the raycast is free, lights up to `GrassLightMaxHeight` above the ground get the line to the light sampled every `GrassSampleStep` (4 m): one terrain height lookup per sample, and a surface name lookup only where the line runs lower than the grass is tall. A light 40 m away costs about 10 samples.
- **The ambient light reading is cheap.** Each scan also reads the moon, the overcast value and the underground state for the ambient gain. These are plain getters on the world and weather, with no object search. The only world search in the mod is the Numpad `/` test headlights key (`DebugKeys` only), which queries objects once per key press.
- **The scan also runs by day, more slowly.** Adaptation keeps running by day, so the scan and the update also run in daylight, but the scan drops to once a second and the glare and dazzle updates are skipped. Only the visuals are gated by the night.

In a normal scene, only a few lights pass all the cheap checks, so a scan costs a few raycasts. A busy scene, such as a big lit base with many lamps in view or a convoy of cars with headlights, costs more, but it is limited by the number of lights inside their radius and `ScanRadius`.

### 6.3 Settings that affect performance

| Setting | Effect on cost |
|---|---|
| `ScanInterval` | Higher = fewer scans. At 0.5 s the cost halves, but glare starts noticeably late. Keep at 0.25 unless needed. |
| `ScanRadius` | Smaller = fewer lights pass the distance test. Also limited by each light's own radius. |
| Light table `Range` | A small `Range` on a bright light culls it early. |
| Light table `Factor: 0` | Ignores the light before any other work. Use it for lights that should never count. |
| `MaxOcclusionChecks` | Upper limit of raycasts per scan (default 8, 0 = no limit). Lower = cheaper in lit bases, but more lights are assumed blocked. |
| `UseOcclusion` | `false` removes all raycasts and the grass check. Cheapest option, but lights behind walls then count fully, which breaks the idea of the mod. Not recommended: with the default `MaxOcclusionChecks` the raycasts cost well under 0.1 ms per frame (6.6). |
| `UseGrassOcclusion` | Off by default. `true` adds the terrain sampling after the raycast. |
| `GrassSampleStep` | Larger = fewer samples per light (default 4 m), but it can miss narrow grassy crests. |
| `GrassLightMaxHeight` | Only lights this low above the ground (default 1.5 m) get the grass check. Lower = cheaper. |
| `DoseThreshold` | Higher = weak lights are dropped before occlusion is tested. |
| `ReportIntervalSeconds` | Larger = fewer RPCs. The server only keeps the last value anyway. |
| `DebugKeys` | `false` on live servers (also for cheating reasons). |

### 6.4 Server load

- **One small file.** Player records live in memory and are written to `adaptation.json` once a minute, a few dozen bytes per player. Old records are dropped (2.3).
- **Network.** One config RPC per connecting client, and one small RPC per client every 15 s. This is negligible compared with vanilla traffic.
- **No per-player server tick.** The server does not run a loop per player. Adaptation lives on the client, the server only stores it.

### 6.5 Logging

With `DebugLogging: true` the mod prints two `[EyeAdapt]` status lines to the client script log every 5 seconds, plus event lines (dazzle, flash grenade hit, restore after login, night clock, glare bloom, settings received). With it off, the mod only logs the one-time server config load lines. The lines are useful while testing but add up in a log file over a long session, so the default is `false` (quiet log). The lines do not affect frame rate noticeably, only log size. Numpad 9 output is not affected by this switch.

With `PerfLog: true` every client writes one `[EyeAdapt] perf` line to its script log every 10 seconds, independent of `DebugLogging`:

```
[EyeAdapt] perf | fps 85 (worst frame 28 ms) | update avg 0.063 ms max 0.639 ms (0.534 % of frame) | scans 39 avg 0.379 ms max 0.573 ms | lights 12, passed avg 11 max 11, raycasts avg 8 max 8
```

- `fps` / `worst frame`: the whole game over the last 10 s, not only EyeAdapt.
- `update`: EyeAdapt's own work per frame (scan included), average and worst single frame, and the average as a share of the frame time.
- `scans`: light scans in the window (4 per second at night, 1 per second by day), average and worst time of one scan.
- `lights`: lights registered near the player, how many passed the cheap checks, and occlusion raycasts per scan.

Times are measured with the CPU performance counter around EyeAdapt's code only. Leave `PerfLog` off on live servers unless you are measuring.

### 6.6 Measured results

One client, 2026-10-23: AMD Ryzen 7 7800X3D, RTX 4090, 4K, local server on the same PC, Chernarus, mod version 0.3.0 + `PerfLog`. Default settings (`ScanInterval` 0.25, `MaxOcclusionChecks` 8, `UseOcclusion` true, `UseGrassOcclusion` false). The scene: a fenced base with 4 watchtowers, 12 vanilla torches (`Torch_Video`) as the only lights, the player standing in the yard with all of them in view.

| Scene | Lights / passed / raycasts per scan | EyeAdapt per frame (avg / worst) | One scan (avg / worst) | Share of the frame |
|---|---|---|---|---|
| Day, almost no lights | 1 / 0 / 0 | 0.02–0.04 ms / 0.1–0.2 ms | 0.02 ms / 0.03 ms | 0.3 % |
| Day, 12–14 torches in view | 14 / 13 / 3–8 | 0.03 ms / 0.3–0.5 ms | 0.15–0.30 ms / 0.40 ms | 0.3 % |
| Night, no lights near | 1–2 / 0 / 0 | 0.03 ms / 0.1 ms | 0.01–0.02 ms / 0.03 ms | 0.3–0.4 % |
| Night, 3 torches in view | 12 / 3 / 3 | 0.04–0.05 ms / 0.2–0.3 ms | 0.10–0.12 ms / 0.21 ms | 0.4 % |
| **Night, 12 torches in view (worst case)** | 12 / 11 / 8 (the limit) | **0.06–0.07 ms / 0.73 ms** | **0.30–0.38 ms / 0.66 ms** | **0.5–0.6 %** |

What this means:

- **Occlusion raycasts are nearly the whole scan cost**, about 0.04 ms each. A scan without raycasts costs 0.01–0.02 ms. With the default limit of 8, one scan never went above 0.7 ms.
- **At night the scan runs 4 times a second**, so the cost spreads to well under 0.1 ms per frame on average. The worst single frame (a frame that also runs a scan) was 0.73 ms, a few percent of a 60 FPS frame.
- **The frame rate drop in a lit base is the engine, not EyeAdapt.** In the 12-torch scene the game ran at about 80–90 FPS, against 120 (frame cap) by day. Turning the effect off with Numpad 7 did not change it. Numpad 7 only switches off the visuals, the scan keeps running, so this also shows the post-processing itself costs nothing measurable. EyeAdapt's share at that frame rate is about 0.5 %, so without EyeAdapt the game would run about 0.5 FPS faster. Each torch has a dynamic light and fire particles, and rendering those is what costs frames, with or without the mod.
- **Server:** no measurable load. The server ran at about 7200 FPS with a worst frame of 2–5 ms during the whole test.

Not measured yet: `MaxOcclusionChecks` 0 (no limit, in the 12-torch scene it would be 11 raycasts instead of 8, so roughly 0.45 ms per scan), many players with flashlights, car headlights, `UseGrassOcclusion` on.

### 6.7 How to measure on your server

1. Set `PerfLog: true` in `config.json`, restart, and read the `[EyeAdapt] perf` lines in the client script log (6.5).
2. Stand in the worst case for your server (lit base, parked cars with lights on, several players with flashlights) for a minute. Compare `update avg` and `max` with the table in 6.6.
3. For more detail, press Numpad 9 once (needs `DebugKeys`). The log shows how many lights are registered, how many passed the cheap checks, how many raycasts were used, and how many lights were skipped because the dose was saturated.
4. Keep the game window focused while measuring: DayZ throttles itself to about 20 FPS in the background, which makes the `fps` value meaningless.
5. If EyeAdapt's share is too high, lower `MaxOcclusionChecks` first. Keep `UseGrassOcclusion` off (the default), or raise `GrassSampleStep`. `UseOcclusion: false` removes all raycasts, but then lights behind walls count fully, which breaks the gameplay idea; it is not recommended.
---

## 7. Settings reference

Every key in `config.json` with its default. Section numbers point to the explanation. `Version` is managed by the mod, do not edit it.

### 7.1 General and persistence

| Key | Default | Meaning |
|---|---|---|
| `Version` | 27 | Config version, set by the mod. |
| `Enabled` | true | Master switch. |
| `DebugKeys` | false | Numpad debug keys (2.4). **Set to false on live servers.** |
| `DebugLogging` | false | Client script log: status lines every 5 s and event lines (6.5). |
| `PerfLog` | false | Client script log: EyeAdapt's own cost (per-frame update, light scans) every 10 s (6.5, 6.7). For performance tests. |
| `ShowAdaptation` | false | On-screen text "NA 73 %" in the top left, updated every second. Players could read their NA from it, so leave it off on live servers. |
| `StartAdaptation` | 0.2 | NA for new characters (2.3). |
| `LogoutMode` | "keep" | What a returning player gets: `"keep"`, `"fixed"` or `"sleep"` (2.3). |
| `LogoutAdaptation` | 0.2 | `"fixed"` only: highest NA a returning player gets. |
| `RecordKeepDays` | 30 | `"keep"` only: forget a player after this much offline time (server uptime). 0 = never. |
| `ReportIntervalSeconds` | 15 | Client reports its NA to the server this often, at night only (plus once when logout starts). |

### 7.2 Adaptation curve (3.2, 3.3)

| Key | Default | Meaning |
|---|---|---|
| `AdaptationMode` | "fixed" | `"fixed"` or `"relativeToNight"`. |
| `FullAdaptSeconds` | 600 | Full adaptation time in `fixed` mode (10 min). |
| `FullAdaptNightFraction` | 0.2 | Share of the whole night in `relativeToNight` mode. |
| `AdaptMinSeconds` / `AdaptMaxSeconds` | 60 / 900 | Clamp for `relativeToNight`. |
| `FastCurveSeconds` | 20 | Fast stage, ~95 % complete after this. |
| `FastCurveWeight` | 0.3 | Share of the gain from the fast stage. |
| `AdaptCurvePower` | 1.5 | Visual strength = adaptation ^ power. 1 = linear, higher = the first minutes show less and the late phase counts more. |
| `LightFastSeconds` | 2 | Fast stage drop time under light. |
| `LightSlowSeconds` | 8 | Slow stage drop time under light. |

### 7.3 Light scan and dose (5.1, 5.5)

| Key | Default | Meaning |
|---|---|---|
| `ScanInterval` | 0.25 | Seconds between light scans. |
| `ScanRadius` | 40 | Maximum distance a light counts (also limited by its own radius). |
| `FalloffExponent` | 1.3 | Distance falloff. 2 = physical inverse square. |
| `DoseThreshold` | 0.02 | Contributions below this count as 0. |
| `DoseSensitivity` | 2.0 | Saturation of the summed dose. |
| `MaxDoseEffect` | 1.0 | How far a full dose pushes the NA target down. |
| `ViewMinFactor` | 0.25 | Share a light behind the player still counts. |
| `OwnLightFactor` | 0.1 | Own flashlight, headtorch or weapon light. |
| `RedLightFactor` | 0.15 | Red light discount. |
| `NightVisionDose` | 0.5 | Powered NV goggles. 0 = free. |
| `NightVisionScopeDose` | 0.25 | Powered NV scope, aiming eye only. 0 = free. |
| `FlashbangAdaptationLoss` | 1.0 | NA lost when a flash grenade hits. 0 = off. |
| `OwnVehicleLightDose` | 0.5 | Minimum dose when sitting in a car with headlights on. 0 = free. |
| `Lights` | see 5.4 | The light table (5.2). |

### 7.4 Occlusion (5.1, 6.3)

| Key | Default | Meaning |
|---|---|---|
| `UseOcclusion` | true | Raycast between eye and light. |
| `MaxOcclusionChecks` | 8 | Raycasts per scan. 0 = no limit. |
| `OccludedFactor` | 0.15 | Light behind walls or terrain. |
| `VegetationOccludedFactor` | 0.5 | Light behind trees or bushes. |
| `OcclusionEndOffset` | 0.5 | Ray stops this many metres before the light. |
| `UseGrassOcclusion` | false | Sample the line for grass (costs extra, off by default). |
| `GrassOccludedFactor` | 0.3 | Light seen through grass. |
| `GrassHeight` / `TallGrassHeight` | 0.5 / 1.0 | Grass heights in metres. |
| `GrassSampleStep` | 4.0 | Metres between samples. |
| `GrassSkipNearEye` / `GrassSkipNearLight` | 1.0 / 3.0 | Grass this close to you or the light is ignored. |
| `GrassLightMaxHeight` | 1.5 | Only lights up to this height above the ground get the grass check. |

### 7.5 Night gate and ambient light (3.5, 3.6)

| Key | Default | Meaning |
|---|---|---|
| `DuskOffsetHours` / `DuskHours` | 0 / 2 | Fade-in after sunset (in-game hours). |
| `DawnOffsetHours` / `DawnHours` | -1.5 / 1.5 | Fade-out around sunrise (in-game hours). |
| `MoonIntensityFull` | 1.0 | Raw moon intensity counted as full moonlight. |
| `FullMoonShare` | 0.8 | Share of `MaxEvBoost` under a clear full moon. |
| `StarlightShare` | 0.6 | Share of `MaxEvBoost` under a clear moonless sky. |
| `OvercastClear` / `OvercastFull` | 0.35 / 1.0 | Overcast values that count as clear and as fully overcast. |
| `OvercastMinShare` | 0.25 | Fully overcast relative to clear. |
| `MinAmbientGain` | 0.2 | Floor as a share of `MaxEvBoost`. |
| `UndergroundGain` | 0 | Gain underground (Livonia, Sakhal). |
| `AmbientSmoothSeconds` | 2 | Smoothing of ambient changes. |

### 7.6 Visuals (3.7)

| Key | Default | Meaning |
|---|---|---|
| `MaxEvBoost` | 5 | EV stops under a clear full moon at full NA. |
| `MaxDesaturation` | 0.6 | Colour loss at full NA. |
| `ColorReturnSmoothSeconds` | 0.5 | Smoothing of the colour return near light. |
| `MaxContrastBoost` | 0.125 | Counters the foggy look of the boost. |
| `NightBlackLevel` | 0.0 | Black point at full adaptation (colour offset after the tonemap, 0 = off, 0 to -0.1). The EV boost lifts the empty night sky to grey, this pulls it back to black. Stars and the Milky Way are bright and stay. Scaled with the ambient light squared. Off by default: on dark server configs even small values (-0.015) crush the night to pitch black. Only try it on bright-night servers, start around -0.005. |
| `BlueShift` | 0.1 | Purkinje effect. 0 = off. |
| `NightGrain` | 0 | Film grain, 0 = off. |
| `NightGrainSize` / `NightGrainFrequency` | 2.75 / 20 | Grain look. |
| `NightSoftness` | 0 | Soft bloom look, 0 = off. |

### 7.7 Glare and dazzle (4)

| Key | Default | Meaning |
|---|---|---|
| `GlareStrength` | 0 | Overall glare scale. **0 = off (default)**, 1.0 = full glare. |
| `GlareViewPower` | 8 | How centred the light must be. |
| `GlareTriggerLevel` | 0.3 | NA lag that starts a blowout. |
| `GlareHoldSeconds` / `GlareFadeSeconds` | 0.6 / 1.0 | Blowout phase. |
| `GlareEvBoost` / `GlareOverlay` / `GlareContrastLoss` | 6 / 1.0 / 0.3 | Blowout strength. |
| `GlowFadeSeconds` | 6 | Bloom halo fade. |
| `GlareBloomIntensity` / `GlareBloomThresholdDrop` | 2.0 / 0.3 | Bloom halo strength. |
| `BloomBaseIntensity` / `BloomBaseThreshold` | 0.75 / 0.5 | Vanilla glow values. Change only if another mod alters them. |
| `GlareSmoothSeconds` | 0.15 | Smoothing of the glare fades. |
| `GlareRecoverySeconds` | 30 | Glare recovery limit. 0 = off. |
| `FlashlightDazzle` | true | Other people's beams in your face dazzle. |
| `DazzleThreshold` / `DazzleMinAdaptation` | 0.3 / 0.3 | When dazzle triggers. |
| `DazzleHoldSeconds` / `DazzleFadeSeconds` / `DazzleRearmSeconds` | 1.0 / 2.0 / 2.0 | Dazzle timing. |
| `DazzleOverlay` / `DazzleEvBoost` | 0.9 / 5 | Dazzle strength. |

---

## 8. Example presets

Starting points, **not tested on a live server yet**. Put only the keys you want to change into `config.json`: missing keys keep their defaults. A key you change must have its value on the right JSON type (numbers without quotes, `true`/`false` for booleans).

### 8.1 Realistic (the intended experience)

Slow adaptation, light costs a lot, glare on.

```json
{
  "AdaptationMode": "fixed",
  "FullAdaptSeconds": 480,
  "MaxEvBoost": 5.0,
  "GlareStrength": 1.0,
  "DebugKeys": false
}
```

### 8.2 Mild (casual and PvE servers)

Quick adaptation, light barely hurts, no glare. Closest to a plain brighter night.

```json
{
  "AdaptationMode": "fixed",
  "FullAdaptSeconds": 150,
  "LightSlowSeconds": 20,
  "DoseSensitivity": 1.0,
  "GlareStrength": 0.3,
  "DebugKeys": false
}
```

### 8.3 Hardcore (PvP, light discipline)

Long adaptation, light is punished, flashlight dazzle on, grain against gamma abuse.

```json
{
  "AdaptationMode": "fixed",
  "FullAdaptSeconds": 750,
  "LightSlowSeconds": 5,
  "DoseSensitivity": 3.0,
  "NightVisionDose": 0.8,
  "FlashlightDazzle": true,
  "NightGrain": 0.15,
  "DebugKeys": false
}
```

### 8.4 Fast-night server (e.g. 48x night)

Adaptation scales with the short night.

```json
{
  "AdaptationMode": "relativeToNight",
  "FullAdaptNightFraction": 0.25,
  "AdaptMinSeconds": 60,
  "AdaptMaxSeconds": 600,
  "DebugKeys": false
}
```
