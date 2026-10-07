# World state: time of day, moon, weather, real time, interiors, forest (vanilla script research)

Source: extracted vanilla scripts at `P:\scripts` and configs at `P:\DZ`. Paths below are relative to `P:\scripts` unless they start with `P:\`. Tags: **[V]** verified in source, **[D]** derived from reading code (high confidence), **[U]** unverified, needs in-game testing.

Related notes: PPE / eye accommodation / lighting configs → [ppe.md](ppe.md). Light sources, raycast signatures, `GetObjectsAtPosition`, vector/math APIs → [lights.md](lights.md) (§8 there). This file doesn't repeat them.

---

## 0. Overview / TL;DR

| Need | Best signal | Client? |
|---|---|---|
| Night gate (binary) | `g_Game.GetWorld().IsNight()` (native, no doc) | yes [V usage] |
| Night gate (gradual) | `g_Game.GetDayTime()` (float hours) + `WorldData.GetApproxSunrise/SunsetTime(month)`; or `GetWorld().GetSunOrMoon()` (narrow switch, see §1.3) | yes [V usage] |
| Moon brightness | `g_Game.GetWorld().GetMoonIntensity()` (native, no doc, range unknown). No moon-phase API. | yes [D] |
| Overcast / fog / rain | `g_Game.GetWeather().GetOvercast().GetActual()` etc. (0..1) | yes [V usage] |
| In-game time progress | `GetDayTime()` delta vs. `g_Game.GetTickTime()` delta | yes |
| Time acceleration setting | no script getter on client; server can try `g_Game.ServerConfigGetInt("serverTimeAcceleration")` (int only) [U] | server |
| Wall-clock time | `GetYearMonthDayUTC` + `GetHourMinuteSecondUTC` (1_core) → build epoch yourself. No vanilla timestamp helper. | both |
| Underground (caves, bunkers) | `PlayerBase.m_UndergroundPresence` (`EUndergroundPresence`), client only | client |
| Inside building | `Man.IsSoundInsideBuilding()` (native) | yes [V usage] |
| Under roof | none on client; vanilla `Environment` is **server-only**. Copy its 25 m upward `RayCastBullet`. | DIY |
| Forest | nothing in vanilla. Options: surface type under player, tree count via `GetObjectsAtPosition`, upward raycast. | DIY |

---

## 1. Time of day

### 1.1 Native API [V]

`3_game/global/world.c` (class `World`, get it via `g_Game.GetWorld()`):

```c
proto native void  SetTimeMultiplier(float timeMultiplier);   // l.19 "Sets the world time acceleration, overriding config. Mostly used for debug purposes. 0-64, -1 to reset"
proto void         GetDate(out int year, out int month, out int day, out int hour, out int minute); // l.33 "Get actual ingame world time", month 1..12, minute 0..59
proto native void  SetDate(int year, int month, int day, int hour, int minute);              // l.51
proto native float GetLatitude();       // l.52
proto native float GetLongitude();      // l.53
proto native float GetMoonIntensity();  // l.54  (no doc comment)
proto native float GetSunOrMoon();      // l.55  (no doc comment)
proto native bool  IsNight();           // l.56  (no doc comment)
proto native float GetEyeAccom();       // l.58  (see ppe.md §3)
proto native void  SetEyeAccom(float);  // l.59
```

`3_game/global/game.c` (class `CGame`, use `g_Game`):

```c
proto native float GetDayTime();   // l.439 "Returns current daytime on server"  -> float hours, e.g. 22.0
proto native float GetTickTime();  // l.913 "Returns current time from start of the game", seconds
proto int         GetTime();       // l.1510 "returns mission time in milliseconds"
```

`1_core/proto/enworld.c`:

```c
proto native float GetWorldTime();              // l.14, no doc. Only use: Math.Randomize seed (3_game/entities/dayzinfectedtype.c:103)
proto native float GetSceneHDRMul(int camera);  // l.162 "scene light multiplier (based on measured scene light levels) - pre-exposure of light"
```

`3_game/weather.c:180`: `Weather.GetTime()` "Returns actual time from start of a server (how many seconds elapsed from server start)".

### 1.2 Vanilla day/night logic [V]

`3_game/worlddata.c` (`WorldData`, via `g_Game.GetMission().GetWorldData()`):

- Hard-coded sunrise/sunset (l.67-70): Jan 8.54 / 15.52, Jul 3.26 / 20.73 (hours). Per-world subclasses (`4_world/classes/worlds/*.c`) may override in `Init()`.
- `float GetApproxSunriseTime(float monthday)` / `GetApproxSunsetTime(float monthday)` (l.89-102): piecewise linear over the month number. Note the formula hits the July value at month **8**, not 7 (quirk, harmless).
- `int GetDaytime()` (l.104-121): `GetDate()` → `DAWN` = sunrise..sunrise+2h, `DAY` until sunset, `DUSK` = sunset..sunset+2h, else `NIGHT`. Constants in `class WorldDataDaytime` (l.463): `ANY=-1, NIGHT=0, DAY=1, DUSK=2, DAWN=3`. Called with integer `month` only, so sunrise jumps once per month.

Users:

| Where | Check | Side |
|---|---|---|
| `4_world/classes/explosions/flashbangeffect.c:53` | `g_Game.GetDayTime() >= 22.0 \|\| < 7.0` | client |
| `4_world/classes/undergroundhandlerclient.c:394` | `GetWorld().IsNight()` | client |
| `4_world/entities/itembase/fireplacebase.c:1916` | `GetWorld().IsNight()` (AI noise only at night) | server |
| `4_world/classes/environment/environment.c:658` | `WorldData.GetDaytime()` | server (see §6.3) |
| `3_game/systems/dynamicmusicplayer/dynamicmusicplayer.c:732` | `WorldData.GetDaytime()` | client |
| `5_mission/.../gameplayeffectwidgets.c:566` | `GetSceneHDRMul(0)` clamped 0.005 ("dusk?") .. 1.0 ("dark?") for breath colour | client |

So `IsNight()`, `GetDayTime()`, `GetDate()` and `GetSceneHDRMul()` are all used client-side by vanilla → the date is replicated to clients. [D]

**Night-only lights:** `ScriptedLightBase.m_NightTimeOnlyLights` (`4_world/entities/scriptedlightbase.c:44`) is a static set filled in `EOnInit` with every light whose `IsVisibleDuringDaylight()` is false (l.66-75). Script never decides "night" there: the **engine** hides such lights by day (`EntityLightSource.SetVisibleDuringDaylight`, `3_game/entities/entitylightsource.c:27`, no doc). The set exists so `UndergroundHandlerClient.EnableLights(bool)` (l.486-492) can force them visible underground during the day.

### 1.3 What `GetSunOrMoon` probably means [D from config]

The lighting configs (`P:\DZ\data\lighting\lighting_default.txt`) have a `sunOrMoon` key per sun-angle stage with the comment "defines the switch between sun and moon": `C_Night` (sunAngle -18) = **0.0**, `C_Astro1` (-17) = **0.5**, `C_Astro2` (-16) and everything above = **1.0**. Same in the `O_*` (overcast) set. `GetSunOrMoon()` very likely returns this interpolated value: **0 = moon is the light source, 1 = sun**. That makes it a near-binary switch at astronomical night (sun ≈ -17°). The scene is already dark at nautical twilight (-12°), so `GetSunOrMoon` alone switches too late for a "night" gate. [U: log it across a dusk]

Moon light itself: `C_Night.diffuse = {0.1, 0.15, 0.3}` (moon), `O_Night.diffuse = {0,0,0}` (fully overcast night = no moon light at all). `CfgWorlds.DefaultLighting` in `P:\DZ\data\config.cpp:229-243` has `moonDayIntensity=20.5`, `moonNightIntensity=5.3`, `nightAngle=5`, moon colours. No moon phase key in script or configs.

### 1.4 Recommended night signal for EyeAdapt

Use a **continuous night factor** from game time, not a boolean, so the effect fades in with dusk (design rule: night only, nothing by day):

```c
float EyeAdapt_NightFactor()          // 0 = day, 1 = full night
{
    int y, mo, d, h, mi;
    g_Game.GetWorld().GetDate(y, mo, d, h, mi);
    float t = g_Game.GetDayTime();    // float hours  [U: resolution]
    WorldData wd = g_Game.GetMission().GetWorldData();
    float rise = wd.GetApproxSunriseTime(mo);
    float set  = wd.GetApproxSunsetTime(mo);
    // ramp 0->1 over [set, set+DUSK_H], 1->0 over [rise-DAWN_H, rise]  (DUSK_H/DAWN_H configurable, vanilla uses 2 h)
    ...
}
```

Cross-check with `IsNight()` and `GetSunOrMoon()` in a debug log during phase 4. A modded `WorldData` (custom maps, Lad-style configs) can override sunrise/sunset, which this approach picks up automatically. Optional extra gate: `GetSceneHDRMul(0)`. It's measured from the actual image and might be the most honest "how dark is it" value, but its range is unknown. [U]

---

## 2. Moon [V API, U semantics]

- Only `World.GetMoonIntensity()` and `World.GetSunOrMoon()` exist. No moon phase, moon altitude or moon position getter anywhere in `P:\scripts` (grep "moon": only the two natives + server-browser icons).
- `GetMoonIntensity()` is **not used anywhere** by vanilla script. Expected (Arma heritage, `moonIntensity`) to combine phase and altitude, 0..1. [U: log it over several nights; compare full vs. new moon by `SetDate` on a local server]
- The date is settable (`World.SetDate`, `SetTimeMultiplier`) on a local test server, which helps to sweep moon phases for testing.

---

## 3. Weather [V]

`3_game/weather.c`. `g_Game.GetWeather()` (`game.c:1349`) returns `Weather`. Phenomena are `typedef WeatherPhenomenon Overcast/Fog/Rain/Snowfall/WindDirection/WindMagnitude` (l.151-156).

```c
float WeatherPhenomenon.GetActual();     // "actual value of phenomenon in range <0, 1>" (wind: magnitude)
float WeatherPhenomenon.GetForecast();   // "forecast value the phenomenon is heading towards"
float WeatherPhenomenon.GetNextChange(); // seconds until next forecast
Weather: GetOvercast(), GetFog(), GetRain(), GetSnowfall(), GetWindSpeed(), GetWind(), GetDynVolFog*()
```

- Use `GetActual()`; `GetForecast()` is only the target. `OnBeforeChange` (l.124) is "Called on server only". There is no client event for weather changes, so poll.
- **Client availability:** `DayZGame.OnUpdate` calls `worldData.UpdateWeatherEffects(GetWeather(), timeslice)` only `if (IsClient() || !IsMultiplayer())` (`3_game/dayzgame.c:2988-2994`), and that computes the snowflake scale from `weather.GetOvercast().GetActual()` (`worlddata.c:197`). So actual weather values are replicated and valid on the client. [D]
- Lighting already responds to overcast: the engine blends `C_*` (overcast 0.35) and `O_*` (overcast 1.0) stages. At night `O_Night` has **zero diffuse** (no moonlight), lower sky, and `desiredLuminanceCoef` 0.08 vs 0.12. So overcast 0.35 → 1.0 is the range where moonlight disappears. Below 0.35 it's full "clear" lighting. [V config, D interpolation]

---

## 4. Time acceleration / night length

- `serverTimeAcceleration` / `serverNightTimeAcceleration` are **server.cfg** keys. No script getter on the client. Only places they appear: the server browser's `GetServersResultRow.m_EnvironmentTimeMul` / `m_EnvironmentNightTimeMul` (`3_game/services/bioslobbyservice.c:213-214`). `g_Game.GetHostData()` (`game.c:242`) / `OnlineServices.GetCurrentServerInfo()` (`3_game/client/onlineservices.c:170`) return such a row for the current server, but the in-game menu comment says "should be working!" (`5_mission/gui/ingamemenu.c:110`), so it's unreliable (and probably empty on LAN/local). [U]
- Server side, `g_Game.ServerConfigGetInt(string name)` (`game.c:1130`) reads server.cfg keys (vanilla reads `lightingConfig`, `disablePersonalLight`, `enableCfgGameplayFile`, ...). Only an **int** getter exists, so `serverNightTimeAcceleration = 0.5` style values can't be read exactly. [U whether it works for these keys at all]
- `World.SetTimeMultiplier` exists (debug override), no getter.

**Measuring (recommended, client-only, no admin input):**

```c
// every N real seconds (e.g. 30-60 s)
float realNow = g_Game.GetTickTime();      // real seconds since game start
float dayNow  = g_Game.GetDayTime();       // in-game hours
float dGame = dayNow - m_LastDay; if (dGame < 0) dGame += 24;   // midnight wrap
float rate = dGame * 3600 / (realNow - m_LastReal);   // in-game seconds per real second
```

- Measure separately while night and day (night accel differs), smooth with an EMA, and discard samples that span a `SetDate` jump (huge `rate`) or a pause.
- Night length (real s) ≈ `(24 - nightHours) * 3600 / dayRate`, or measure the night directly: `nightHours * 3600 / nightRate`, with `nightHours` from `GetApproxSunset/SunriseTime`.
- Resolution: `GetDate` has **minute** resolution (at 1x, 1 tick/min, so sample ≥ 5 min or use `GetDayTime`). `GetDayTime()` returns float hours. Whether it's continuous or minute-quantised is [U]. Log it every frame for a few seconds.
- Hold the measured value server-side too? Not needed: every client can measure itself, and the first measurement is available after ~1 min. Until then, fall back to the fixed-time mode.

---

## 5. Real-world time (persistence timestamps) [V]

`1_core/proto/ensystem.c`:

```c
proto void GetYearMonthDay(out int year, out int month, out int day);        // l.52 local
proto void GetHourMinuteSecond(out int hour, out int minute, out int second); // l.28 local
proto void GetYearMonthDayUTC(out int year, out int month, out int day);     // l.100
proto void GetHourMinuteSecondUTC(out int hour, out int minute, out int second); // l.76
proto native int TickCount(int prev);   // l.106 CPU ticks, profiling only
```

- There's **no** vanilla epoch/unix-timestamp helper (grep "epoch|timestamp|unixtime": none). `CF_Date` belongs to Community Framework, not vanilla. Don't depend on CF just for this.
- `g_Game.GetTime()` (mission ms), `GetTickTime()` (s since game start), `Weather.GetTime()` (s since server start), `GetWorldTime()` all reset on restart, so they're useless for persistence.
- Build a UTC epoch yourself (int is 32-bit signed, fine until 2038, or use a 2020 epoch to be safe):

```c
static int EyeAdapt_UtcSeconds()
{
    int y, m, d, hh, mm, ss;
    GetYearMonthDayUTC(y, m, d);
    GetHourMinuteSecondUTC(hh, mm, ss);
    // days from civil (Howard Hinnant), valid for Gregorian dates
    if (m <= 2) y -= 1;
    int era = y / 400;                       // y >= 0 here
    int yoe = y - era * 400;
    int mp  = (m + 9) % 12;                  // Mar=0 .. Feb=11
    int doy = (153 * mp + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int days = era * 146097 + doe - 719468;  // days since 1970-01-01
    return days * 86400 + hh * 3600 + mm * 60 + ss;
}
```

Use UTC (no DST jumps). Call on the server only, so all players share one clock and client clocks can't be manipulated. JSON persistence: vanilla uses `JsonFileLoader<T>.LoadFile(path, data, errorMessage)` (e.g. `3_game/undergroundarealoader.c:128`).

---

## 6. Interior / underground

### 6.1 Underground system (caves, Livonia bunker, Sakhal) [V]

Files: `4_world/classes/undergroundhandlerclient.c`, `4_world/entities/scriptedentities/triggers/undergroundtrigger.c`, `3_game/undergroundarealoader.c`, `3_game/ppemanager/requesters/pperundergroundacco.c`.

**Data flow**

1. Server loads `$mission:cfgundergroundtriggers.json`, falling back to `dz/worlds/<world>/ce/cfgundergroundtriggers.json` (`undergroundarealoader.c:104-121`), spawns one `UndergroundTriggerCarrier` per trigger (`missionserver.c:89`), and RPC-syncs the JSON to each client on connect (`RPC_UNDERGROUND_SYNC`, `missionserver.c:311` → `dayzgame.c:3127`).
2. When a carrier enters the client's network bubble, it spawns a **local** `UndergroundTrigger` (`ECE_LOCAL`, `undergroundtrigger.c:44-69`). It only accepts the controlled player (`CanAddObjectAsInsider` → `IsControlledPlayer()`).
3. `OnEnterClientEvent` / `OnLeaveClientEvent` → `player.GetUndergroundHandler().OnTriggerEnter/Leave(this)` (l.163-185). **`PlayerBase.GetUndergroundHandler()` lazily creates the handler** (`playerbase.c:1652-1659`), so don't call it just to read state.
4. Handler ticks from `PlayerBase.EOnFrame` on the client (`playerbase.c:4341`) and kills itself once back to `NONE` at accommodation 1.

**Trigger types** (`EUndergroundTriggerType`, `undergroundtrigger.c:72`; determined in `Init`, l.90-118): has breadcrumbs → `TRANSITIONING`; else `EyeAccommodation == 1.0` → `OUTER`; else `INNER`. The highest type among the triggers the player is in wins.

**Presence** (`EUndergroundPresence`, `undergroundhandlerclient.c:1-7`): `NONE, OUTER, TRANSITIONING, FULL`. Stored on the player as public field **`PlayerBase.m_UndergroundPresence`** (`playerbase.c:75`, set via `SetUnderground`, l.2678). Client only (only set from the client handler).

**How it darkens** (the part relevant to EyeAdapt):

- **Eye accommodation:** target = trigger's `EyeAccommodation` (INNER/OUTER), or for TRANSITIONING an inverse-distance-weighted blend of breadcrumb values (`CalculateBreadCrumbs`, l.96-198; each crumb has `Position`, `EyeAccommodation`, `Radius` (default cutoff 5 m), `UseRaycast` (head → crumb `RayCastBullet` on TERRAIN|ROADWAY|BUILDING; blocked crumbs excluded), optional `DoorState` controller that follows a door's anim phase), or a line-point fade (`CalculateLinePointFade`, l.200-312). It moves exponentially: `acco += (target - acco) * speed * dt`, speed = trigger `InterpolationSpeed` or `DEFAULT_INTERPOLATION_SPEED = 7` (l.407-425). **Symmetric**, fast (~0.3 s time constant).
- Applied through `PPERUndergroundAcco.SetEyeAccommodation(v)` → `PPEExceptions.EYEACCOM`, `PPEEyeAccomodationNative.L_0_UNDERGROUND` (=100), `SET` (see ppe.md §6). Native `World.SetEyeAccom` scale: 1 = normal, 0 = black.
- **Lighting config blend:** constructor loads `dz\data\lighting\lighting_underground.txt` as user lighting (l.39). `SetUserLightingLerp(m_LightingLerp)` every tick (l.329-343). The lerp animates to 1 when presence becomes `FULL` (`LIGHT_BLEND_SPEED_IN = 5`, EaseInQuint) and back to 0 on leaving (`LIGHT_BLEND_SPEED_OUT = 1.75`, EaseOutCubic) (l.546-587).
- NVG compensation: `PPERequester_CameraNV.SetUndergroundExposureCoef(acco)` (1.0 if `IsNight()` or lerp = 1) (l.393-405).
- Also: env sounds scaled by acco, lightning suppressed, `m_NightTimeOnlyLights` forced visible.

**Data on disk:** `P:\DZ\worlds\chernarusplus\ce\cfgundergroundtriggers.json` is **empty** (no Chernarus triggers). `enoch` (Livonia bunker) has ~29 `EyeAccommodation` values (triggers + breadcrumbs) in 8 breadcrumb sets with values 0 .. 1. `sakhal` has ~51 values in 20 sets. Inner values are mostly **0.0** (pitch black without light).

### 6.2 Inside building [V API, U semantics]

- `Man.IsSoundInsideBuilding()` (native, `3_game/entities/man.c:37`, no doc). Used client-side for the sound "interior" variable (`4_world/entities/dayzplayerimplement.c:3454`) and by the server `Environment.IsInsideBuilding()` (`environment.c:313-316`). This is the engine's interior flag (probably building interior volumes/"inside" geometry). Cost: native bool read, free.
- `IsCameraInsideVehicle()` (man.c:38).

### 6.3 Under roof [V]

`Environment` (`4_world/classes/environment/environment.c`) is created **only on the server** (`playerbase.c:414-420`, inside `if (g_Game.IsServer())`). Its `IsUnderRoof()` is not available on the client. Its logic (`CheckUnderRoof`, l.391-411), run every `GameConstants.ENVIRO_TICK_ROOF_RC_CHECK = 10` s (constants.c:725) and only when not `IsInsideBuilding()`:

```c
vector from = m_Player.GetPosition();
vector to = from + "0 25 0";
PhxInteractionLayers mask = PhxInteractionLayers.ITEM_LARGE | PhxInteractionLayers.BUILDING | PhxInteractionLayers.VEHICLE;
m_IsUnderRoof = DayZPhysics.RayCastBullet(from, to, mask, null, hitObject, hitPosition, hitNormal, hitFraction);
m_IsUnderRoofBuilding = hitObject && hitObject.IsInherited(House);
```

Copy this client-side for "under a roof / overhang" (one ray every 1-2 s is negligible).

---

## 7. Forest canopy

Vanilla has **no** "in forest" query. What exists:

- **Tree classes** (`4_world/entities/core/inherited/plant.c`): `TreeHard`, `TreeSoft` (`IsTree()` true), `BushHard`, `BushSoft` (`IsBush()` true), all `PlantSuper` / `WoodBase`. Map trees are scriptable objects, class `TreeHard_<p3dname>` (comment l.9).
- **Client-side tree enumeration precedent:** `TreeEffecter.OnVariablesSynchronized` (`4_world/entities/woodbase.c:301-345`) calls `g_Game.GetObjectsAtPosition(pos, radius, ...)` and casts each to `WoodBase`. So static map trees are returned on the client. [V] No timing data. The other frequent client caller, `VicinityItemManager`, uses only 0.5-2 m radii every 0.25 s (`5_mission/gui/inventorynew/vicinityitemmanager.c:3-5, 245, 268`). Vanilla avoids large radii.
- **Surface type:** `float CGame.SurfaceGetType(float x, float z, out string type)` (`game.c:1166`), `SurfaceGetType3D` (l.1168), `SurfaceUnderObject(Object, out string type, out int liquidType)` (l.1169). Chernarus forest-floor surfaces exist in `P:\DZ\surfaces\config.cpp`: `cp_broadleaf_dense1/2`, `cp_broadleaf_sparse1/2`, `cp_conifer_common1/2`, `cp_conifer_moss1/2` (l.287-497). Livonia/Sakhal have their own sets (`P:\DZ\surfaces_bliss`, `surfaces_sakhal`). Cheapest heuristic (one call), but it's the ground texture, not the canopy. [U how well it matches]
- **Sound map controllers** `forest`, `trees`, `houses`, `night`, `caveSmall`, `caveBig`, `overcast` exist (`3_game/sound.c:34`), but script can only **override** them (`SetSoundControllerOverride`, `MuteAllSoundControllers`, `ResetAllSoundControllers`). There's no getter. [V]
- **Animal catching** defines `MASK_ENVIRO_FOREST` (`3_game/systems/animalcatching/catchingconstants.c:13`), but traps just use `MASK_ENVIRO_LAND_ALL`. No forest detection. Comment: "raycasts are unreliable outside network bubbles" (`catchingcontexttraps.c:138`).
- **Raycast:** `PhxInteractionLayers` has no tree/foliage layer (`3_game/global/dayzphysics.c:1-43`). A `RaycastRV(..., iType = ObjIntersectView)` upward probably hits tree view geometry (canopy). [U] Multiple slanted rays give a coverage fraction.

Cost guidance: a fan of 5-9 upward `RaycastRV` every 2 s, or `GetObjectsAtPosition(pos, 10-15 m)` filtered by `IsTree()` every 2 s, should be cheap. Benchmark with `TickCount` as described in lights.md §8. Avoid radii ≥ 30 m in towns.

---

## 8. Client vs server [V]

`3_game/global/game.c:1120-1127`: `IsMultiplayer()`, `IsClient()`, `IsServer()`, `IsDedicatedServer()` ("Robust check which is preferred than the above, as it is valid much sooner. You may want to use #ifdef SERVER instead"). The vanilla client-effect idiom is `if (IsClient() || !IsMultiplayer())` (dayzgame.c:2988). In offline/singleplayer both `IsServer()` and the client are the same process.

| API | Client | Server | Evidence |
|---|---|---|---|
| `World.GetDate`, `g_Game.GetDayTime`, `IsNight`, `GetSunOrMoon`, `GetMoonIntensity` | yes | yes | client users in §1.2 (moon/sun natives: [D]) |
| `WorldData.GetDaytime/GetApproxSunrise...` | yes | yes | `WorldData` is created per mission. Music player uses it client-side |
| `Weather...GetActual()` | yes | yes | §3 |
| `GetSceneHDRMul(0)` | yes (render) | meaningless | breath effect |
| `PlayerBase.m_UndergroundPresence` | yes | **no** (never set server-side) | §6.1 |
| `Man.IsSoundInsideBuilding()` | yes | yes | §6.2 |
| `Environment.IsUnderRoof()` | **no** (object only exists on server) | yes | §6.3 |
| `ServerConfigGetInt` | no use | yes | §4 |
| `GetYearMonthDayUTC` etc. | yes | yes | use server for persistence |
| `GetObjectsAtPosition`, raycasts | yes (within network bubble) | yes | §7 |

---

## 9. Verified vs. unverified

**Verified in source**
- Signatures of all time/moon/weather/real-time natives above. `GetDayTime` is float hours (compared with 22.0).
- Vanilla day/night = `WorldData.GetDaytime()` with hard-coded per-month sunrise/sunset and 2 h dawn/dusk; `IsNight()` used both sides.
- `sunOrMoon` in lighting configs flips 0 → 1 between sun angle -18° and -16°.
- Overcast night lighting stage has zero diffuse (no moonlight).
- No moon-phase API. No time-acceleration getter on the client. No unix-timestamp helper.
- Underground system: JSON triggers, client-local triggers, `m_UndergroundPresence`, eye accom via PPE layer 100 SET, user lighting config lerp, exponential symmetric interpolation speed 7.
- `Environment` (under roof, inside building) exists only on the server. Its roof check = 25 m upward `RayCastBullet`.
- Client-side `GetObjectsAtPosition` returns map trees (`WoodBase`).

**Needs in-game testing**
- Range and behaviour of `GetMoonIntensity()` (phase? altitude? overcast?), `GetSunOrMoon()` (assumed = config `sunOrMoon`), `IsNight()` threshold.
- `GetDayTime()` resolution (continuous vs. per-minute).
- `GetSceneHDRMul(0)` range at night vs. campfire. Could be the ideal "measured darkness" input.
- `ServerConfigGetInt("serverTimeAcceleration")` / `"serverNightTimeAcceleration"` and `g_Game.GetHostData().m_EnvironmentTimeMul` on a real server.
- `IsSoundInsideBuilding()` granularity (doorway, sheds, open barns).
- Upward `RaycastRV` with `ObjIntersectView` hitting tree canopies. Cost of `GetObjectsAtPosition` at 10-30 m in forest/town.
- Interaction of our EV/eye-accom with underground `SET` on EYEACCOM (layer 100): our requester must not fight it (ppe.md §9).

---

## 10. Implications for EyeAdapt

1. **Night gate:** compute a smooth `nightFactor` 0..1 from `g_Game.GetDayTime()` and `WorldData.GetApproxSunriseTime/SunsetTime(month)` (respects modded worlds), with configurable dusk/dawn ramps. Multiply the whole effect (boost, desaturation) by it. During the day `nightFactor = 0` → release the PPE requester entirely (vanilla untouched). Log `IsNight()`, `GetSunOrMoon()`, `GetSceneHDRMul(0)` alongside for tuning, and possibly switch to one of them later.
2. **Moon / overcast scaling (§3.2):**
   - `overcastFactor = 1 - InverseLerp(0.35, 1.0, GetOvercast().GetActual())` clamped. Matches the engine's own C_→O_ lighting blend where moonlight goes to zero.
   - `moonFactor = GetMoonIntensity()` normalised after measurement (no phase API; phase is presumably baked into it).
   - `ambientGain = Lerp(MinAmbientGain, 1, moonFactor * overcastFactor)` → scales `MaxExposureBoost`. Fog (`GetFog().GetActual()`) could reduce it further (optional config).
3. **Night length measurement:** client-only, sample `GetDayTime()` vs `GetTickTime()` every 30-60 s, separate day/night rates, EMA, discard jumps. Use fixed-time mode until a measurement exists. Optionally let the server send `ServerConfigGetInt` values if testing shows they work.
4. **Persistence timestamp (§3.6):** server-side `GetYearMonthDayUTC` + `GetHourMinuteSecondUTC` → own epoch function (snippet §5). Survives restarts. Store in `$profile:EyeAdapt/<steamID>.json` via `JsonFileLoader`.
5. **Underground:** read `PlayerBase.Cast(g_Game.GetPlayer()).m_UndergroundPresence` (don't call `GetUndergroundHandler()`). For `TRANSITIONING`/`FULL`, set `ambientGain → 0` (design rule 4: "nothing to work with, flashlight needed"). Vanilla already applies eye accom 0..1 there. Our boost must not lift it (EV `ADD` would). Optionally read `g_Game.GetWorld().GetEyeAccom()` as an extra multiplier [U whether it returns the PPE-blended value]. Note: Chernarus has no underground triggers. Livonia and Sakhal do.
6. **Interiors:** `player.IsSoundInsideBuilding()` (free) → reduce gain (config `InteriorGainFactor`, e.g. 0.3). Optionally add a client copy of the Environment 25 m up `RayCastBullet` every 1-2 s for "under roof".
7. **Forest (v2):** start with a surface-type lookup (one call, per-map table in config). If insufficient, do an upward `RaycastRV` fan or a 10-15 m `IsTree()` count every 2 s. All are client-side and feasible.
8. **CLAUDE.md §8:**
   - **Q7 answered (mostly):** moon = `World.GetMoonIntensity()` (only API, no phase getter; semantics to log), overcast = `Weather.GetOvercast().GetActual()`. Both readable on the client. Engine lighting already kills moonlight between overcast 0.35 and 1.0.
   - **Q8 partially answered:** vanilla offers no forest detection. Viable options are surface type (cheapest), `GetObjectsAtPosition` + `IsTree()` (client-side precedent exists), and upward `RaycastRV` (canopy hit unverified). Cost must still be measured.
   - Also relevant to **Q9:** time functions verified (`GetDayTime`, `GetTickTime`, `GetTime`, `TickCount`, UTC date/time).
