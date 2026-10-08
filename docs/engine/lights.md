# Dynamic lights in DayZ (vanilla script research)

Source: extracted vanilla scripts at `P:\scripts` and configs at `P:\DZ`. Paths below are relative to `P:\scripts` unless prefixed with `P:\DZ`. Everything marked **Verified** was read in source; **Unverified** items need an in-game test.

## TL;DR

- Every scripted dynamic light is a client-only entity derived from `ScriptedLightBase` (`4_world/entities/scriptedlightbase.c`), created exclusively through the static `ScriptedLightBase.CreateLight(...)` / `CreateLightAtObjMemoryPoint(...)`. A registry hooked into `ScriptedLightBase` catches every vanilla dynamic light (and every modded light that follows the same pattern).
- Vanilla already keeps a static registry, `ScriptedLightBase.m_NightTimeOnlyLights`, but it holds only lights that are *not* visible in daylight. Flashlights, headtorches, car headlights, bonfires, road flares and flare-gun flares are missing from it, so it can't be used as is.
- **Trap:** native `GetBrightness()` does **not** return the nominal brightness. Vanilla rewrites it to about `100 × flicker` (see §2). Read the script field `m_Brightness` instead.
- A **red headtorch** exists: `Headtorch_Black` turns its light red. Red chemlights, red road flares and red flare-gun flares also exist. No red flashlight exists.
- Street lamps and lit windows are not dynamic lights in vanilla (nothing in script or config). A few items also have config-only `Reflectors` lights that bypass `ScriptedLightBase`.

---

## 1. Class hierarchy

```
Managed
 └ IEntity                 1_core/proto/enentity.c:164
   └ Object                3_game/entities/object.c:64
     └ ObjectTyped
       └ Entity            3_game/entities/entity.c:1
         └ EntityLightSource   3_game/entities/entitylightsource.c:8   (native light API)
           └ ScriptedLightBase 4_world/entities/scriptedlightbase.c:10 (script lifecycle)
             ├ PointLightBase  4_world/entities/scriptedlightbase/pointlightbase.c   ctor: SetLightType(LightSourceType.PointLight)
             └ SpotLightBase   4_world/entities/scriptedlightbase/spotlightbase.c    ctor: SetLightType(LightSourceType.SpotLight)
                 └ CarLightBase → CarRearLightBase …
```

`IEntity : Managed` means plain (non-`ref`) script references to lights are **weak** and become `null` automatically when the light is deleted.

`enum LightSourceType { NotDef, PointLight, SpotLight }` (entitylightsource.c:1). Use `GetLightType()` to tell spot lights from point lights.

## 2. Native API: `EntityLightSource` (3_game/entities/entitylightsource.c) — Verified

| Group | Methods |
|---|---|
| Type | `SetLightType(int)` (constructor only), `int GetLightType()` |
| State | `SetEnabled(bool)`, `bool IsEnabled()`, `SetCastShadow`/`GetCastShadow`, `SetVisibleDuringDaylight(bool)`, `bool IsVisibleDuringDaylight()` |
| Size | `SetRadius(float)`, `float GetRadius()` |
| Colour | `SetDiffuseColor(r,g,b)`, `SetDiffuseColorV(vector)`, `vector GetDiffuseColorV()`, `Set/GetDiffuseAlpha`; same set for `Ambient` |
| Brightness | `SetBrightness(float)`, `float GetBrightness()`, `SetPulseCoef(float)` (no getter) |
| Spot | `SetSpotLightAngle(float)`, `float GetSpotLightAngle()` (degrees as used in vanilla, e.g. 110 for the flashlight) |
| Misc | flare (`SetFlareVisible`, ...), heat haze, `EnableSpecular`, `EnableLinear` |

**`GetBrightness()` trap (Verified by reading the code):** `ScriptedLightBase.CorrectLightPulseDuringDaylight()` (scriptedlightbase.c ~l.245) runs every frame from `HandleBrightnessFadeing`. For `m_Brightness < 100` (every vanilla light) it calls `SetBrightness(m_Brightness*pulse / (m_Brightness*0.01))`, which equals `100*pulse`, followed by `SetPulseCoef(m_Brightness*0.01)`. The native brightness is therefore about 100 for every light, and the real value is folded into the pulse coefficient, which has no getter. **Use `light.m_Brightness`**, a public script field, for the nominal brightness. It includes fades and the battery or efficiency scaling applied through `FadeBrightnessTo`. `GetRadius()` is fine (`SetRadius(m_Radius)` each frame), or use `m_Radius`.

## 3. `ScriptedLightBase` (4_world/entities/scriptedlightbase.c) — Verified

The header comment reads: *"Lights work only on client side! Lights with Brightness or Radius of 0 (or less) are automatically deleted."*

**Creation** (the only path vanilla uses):
```c
static ScriptedLightBase CreateLight(typename name, vector global_pos = "0 0 0", float fade_in_time_in_s = 0);
static ScriptedLightBase CreateLightAtObjMemoryPoint(typename name, notnull Object target, string memory_point_start, string memory_point_target = "", vector global_pos = "0 0 0", float fade_in_time_in_s = 0);
```
`CreateLight` uses `g_Game.CreateObject(name.ToString(), pos, true)` (local object) only when `!IsServer() || !IsMultiplayer()`. On a dedicated server it returns `null`, so lights never exist on the server. Each client creates its own lights for networked items, which run their light logic in client branches (`OnWorkStart` and similar).

**Constructor / destructor:**
```c
void ScriptedLightBase()   { m_LifetimeStart = g_Game.GetTime(); SetEnabled(true); SetEventMask(EntityEvent.FRAME); SetEventMask(EntityEvent.INIT); }
void ~ScriptedLightBase()  { /* removes this from m_NightTimeOnlyLights */ }
override void EOnInit(IEntity other, int extra) { if (!IsVisibleDuringDaylight()) { ...; m_NightTimeOnlyLights.Insert(this); } }
```
`EOnInit` reads `IsVisibleDuringDaylight()`, which subclass constructors set. EOnInit therefore runs **after all constructors**, when the per-type parameters are final. No vanilla subclass overrides `EOnInit`.

**Public script fields** (no `protected`): `m_Brightness`, `m_BrightnessTarget`, `m_Radius`, `m_RadiusTarget`, `m_BrightnessPulse`, `m_LifetimeEnd`, `m_FadeOutTime`, `m_Parent`, `m_LocalPos`, `m_LocalOri`, plus flicker and dancing-shadow parameters. `static ref set<ScriptedLightBase> m_NightTimeOnlyLights`.

**Lifecycle helpers:** `SetBrightnessTo`, `FadeBrightnessTo(v,t)`, `SetRadiusTo`, `FadeRadiusTo(v,t)`, `FadeIn(t)`, `FadeOut(t=-1)`, `SetFadeOutTime`, `SetLifetime(s)`, `AddLifetime`, `Destroy()` (deletes immediately, or after 0.03 s if attached), flicker (`SetFlickerSpeed/Amplitude`), `SetBlinkingSpeed`, `EnableDimming(...)`.

**Per-frame (`EOnFrame`):** lifetime check, flicker, dancing shadows, fades, dimming, `CheckIfParentIsInCargo()` (calls `SetEnabled(false)` while the parent item is in cargo), shadow optimisation, `OnFrameLightSource()` hook, blinking.

**Parenting:** `AttachOnObject(parent, local_pos, local_ori)` stores `m_Parent` and calls `parent.AddChild(this, -1)`. `AttachOnMemoryPoint(parent, start, target)` orients the light from the start memory point toward the target point, so a spot light's forward axis equals its beam direction. `GetAttachmentParent()` returns `m_Parent`. **Caveat:** `Headtorch_ColorBase.AttachLightOnHead` (headtorch_colorbase.c ~l.166) bypasses this. It calls `DetachFromParent()` (which sets `m_Parent = null`) followed by `player.AddChild(m_Light, boneIdx)`. A headtorch on a head therefore has `m_Parent == null`, while native `GetParent()` returns the player. The torch can also attach its light to a particle (`torch.c:571-573`). **For ownership checks, walk native `GetParent()`, not `m_Parent`.** **Torch (verified in game):** `Torch.UpdateLight` re-attaches the light to the flame particle (`m_Light.AttachOnObject(direct_particle, ...)`), so neither the native parent chain nor `GetAttachmentParent().GetHierarchyRootPlayer()` reaches the player. EyeAdapt doesn't need ownership for point lights: only own *spot* lights get the own-light discount, point lights in hand (torch, chemlight) count by distance like any other light.

## 4. Inventory of vanilla lights — Verified

B = nominal brightness (`SetBrightnessTo`), R = radius in m, colour = diffuse RGB (ambient is the same unless noted), Day = `SetVisibleDuringDaylight`. Paths: `PL/` = `4_world/entities/scriptedlightbase/pointlightbase/`, `SL/` = `.../spotlightbase/`.

| Light class | File | Created by | Type | B | R | Colour | Notes |
|---|---|---|---|---|---|---|---|
| FireplaceLight | PL/fireplacelight.c | `FireplaceBase` (campfire, barrel, oven, indoor fireplace) fireplacebase.c:681 | point | 4.75 | 25 | 1.3,0.35,0.15 | 20 s fade-in, 5 s fade-out, flicker 0.3, Day=false. Brightness fades to 0 when the player is >2000 m away (50 m for ovens) |
| BonfireLight | PL/bonfirelight.c | bonfire.c:33 (map object) | point | 15 | 40 | 1.0,0.45,0.25 | Day=true |
| TirePileLight | PL/tirepilelight.c | misc_tirepile_burning.c:27 | point | 15 | 40 | 1.0,0.45,0.25 | Day=true |
| TorchLight | PL/torchlight.c | torch.c:563 | point | 5 | 30 | 1.3,0.35,0.15 | scaled by fuel via `FadeBrightnessTo(m_TorchBrightness*coef)` |
| PortableGasLampLight | PL/portablegaslamplight.c | portablegaslamp.c:38 | point | 5 | 15 | 1.0,0.7,0.5 | |
| StoveLight | PL/stovelight.c | portablegasstove.c:149 | point | 3 | 1 | 0.2,0.45,0.7 (blue) | tiny |
| BlowtorchLight | PL/blowtorchlight.c | blowtorch.c:16 | point | 3 | 1 | 0.2,0.45,0.7 | tiny |
| ChemlightLight | PL/chemlightlight.c | chemlight.c:72 | point | 1.5 | 7.5 | per colour, see §5 | fades with efficiency (`SetIntensity`) |
| RoadflareLight | PL/roadflarelight.c | roadflare.c:197 | point | 8.5 | 30 | 1.0,0.3,0.3 (red) | Day=true, flicker |
| FlareLight (+Red/Green/Blue) | PL/flarelight.c | flare gun projectile, firearms/flaresimulation.c:35 | point | 10 | 100 | 0.7,0.7,0.3 / red 1,.3,.3 / green .3,1,.3 / blue .3,.3,1 | Day=true |
| FlareLightContamination | PL/flarelight.c | contaminatedarea_dynamic.c:234 | point | 10 | 100 | red | 20 s lifetime |
| ShellLight | contaminatedarea_dynamic.c:2 | gas shell impact | point | 10 | 100 | 1,1,0.3 | 0.15 s lifetime |
| ExplosiveLight | explosivesbase.c:1 | explosives | point | 10 | 30 | 1,1,0.3 | 0.15 s |
| FlashGrenadeLight | grenade_base.c:12 | flashgrenade.c:89 | point | 50 | 20 | white | 0.35 s |
| ExplosionLight | gear/tools/fireworkslauncher.c:1 | fireworks | point | 0.05 (flicker max ×3) | 60 | white | 2.1 s |
| MuzzleFlashLight / _1 / _2 | PL/muzzleflashlight.c | weaponparticles.c:215-219 | point | 1 / 0.5 / 0.2 | 15 / 17 / 12 | warm yellow | 0.04 / 0.08 / 0.01 s |
| FlashlightLight | SL/flashlightlight.c | flashlight.c:17 (memory points beamStart→beamEnd) | spot 110° | 4 | 25 | 0.9,0.85,0.75 | Day=true, 0.08 s fade-in |
| HeadtorchLight | SL/headtorchlight.c | headtorch_colorbase.c:125 | spot 95° | 3 | 20 | white 0.92,0.85,0.86; **red 1.0,0.2,0.2 on Headtorch_Black** | battery `SetIntensity` |
| PistollightLight | SL/pistollightlight.c | switchable_base/tlrlight.c:33 (pistol TLR) | spot 100° | 5.25 | 18 | 0.92,0.88,0.82 | second mode: 50° and 1.8× radius |
| UniversallightLight | SL/universallightlight.c | switchable_base/universallight.c:51 (rail light) | spot 100° | 6.05 | 25 | 0.95,0.88,0.8 | second mode: 50° and 1.8× radius |
| SpotlightLight | SL/spotlightlight.c | itembase/spotlight.c:106 (generator spotlight) | spot 140° | 20 | 60 | 0.8,0.9,1.0 | |
| AlarmLight : SpotlightLight | building/underground/entrance/land_underground_entrance.c:29 | bunker door alarm | spot | 10 | 15 | 1,0,0 | |
| CarLightBase subclasses (front) | SL/carlightbase/*frontlight.c | `CarScript.CreateFrontLight()`, carscript.c:2120 | spot 80-120° | 4-6 per bulb / 8-12 both | 55-100 | warm white | single bulb (segregated) or both bulbs merged (aggregated) |
| CarRearLightBase subclasses | SL/carlightbase/carrearlightbase/*.c | carscript.c:2181 | spot 120-270° | 1-7 | 6-18 | red brake / white reverse | Day=false |
| EntranceLight (+Stairs1/2, Main1/2) | PL/entrancelight.c | underground entrance and stairs (Livonia bunker) | point | 0.5 | 5.5 (Stairs1: 10, reddish) | 0.9,0.9,0.7 | |
| WarheadStorageLight | PL/warheadstoragelight.c | land_warheadstorage_main.c:304 | point | 6.5 | 7.5 | 1,0.7,0.3 | dimming |
| BuoyLight / MooringBuoyLight | PL/buoylight.c | land_buoy.c:11/34 | point | 5 | 15 / 10 | 1,0.2,0.2 (red) | dimming |
| Roadblock_Light | building/wrecks/staticobj_roadblock_wood_small.c:27 | roadblock blinker | point | 5.5 | 20 | 0.9,0.9,0.7 | blinking |
| XmasTreeLight | PL/xmastreelight.c | woodbase/christmastree.c:11 | point | 0.5 | 12 | 1.2,1.0,0.7 | |
| XmasSleighLight | PL/xmassleighlight.c | wrecks/wreck_santassleigh.c:19 | point | 3 | 24 | 0.7,0.7,1.0 | |
| AnniversaryMainLight / AnniversaryBoxLight | PL/anniversary*.c | anniversary event | point | 8 / 3 | 50 / 1 | warm | |
| GoatLight / ZombieMummyLight | PL/goatlight.c, zombiemummylight.c | animal and infected eye glow | point | 3.1 | 1.6 / 2.1 | red / yellow | Day=true / false |
| PersonalLight | PL/personallight.c | playerbaseclient.c:13 | point | 0.38 | 3 | 0.2,0.23,0.25 | **follows the camera every frame** (camera pos − 0.4 m Y). **Must be excluded** |
| PartyLight, BrightnessTestLight | PL/ | not created anywhere in vanilla | point | | | | debug only |

## 5. Red light and chemlight colours — Verified

- **Chemlights** (`chemlight.c:76-95`, colours in `chemlightlight.c`): `Chemlight_White` 0.9,0.9,1.0 · `Chemlight_Red` 1.0,0.3,0.2 · `Chemlight_Green` 0.3,1.0,0.2 · `Chemlight_Blue` 0.3,0.3,1.0 · `Chemlight_Yellow` 1.0,1.0,0.3. All five share B 1.5 and R 7.5.
- **Red headtorch:** `Headtorch_Black` (`4_world/entities/itembase/clothing/headtorch_black.c`) overrides `OnLightCreated()` to call `m_Light.SetColorToRed()` (1.0,0.2,0.2). Grey and other variants call `SetColorToWhite()`. It is a fixed per-variant colour, not a togglable mode.
- **No red flashlight**: `FlashlightLight`, `PistollightLight` and `UniversallightLight` have no colour variants.
- Other red sources: road flare, red flare-gun flare, contamination flare, buoy lights, alarm light, car brake lights.
- **Colour-based classification needs a flame exception (verified in game 2026-10-08).** "Red-ness" from `GetDiffuseColorV()` (`g,b < 0.35 * r`) also catches every flame: `FireplaceLight`/`TorchLight` are 1.3,0.35,0.15, which is "redder" than `Chemlight_Red` (1.0,0.3,0.2). Flames are broadband light, so EyeAdapt excludes `FireplaceLight`, `BonfireLight`, `TirePileLight`, `TorchLight`, `PortableGasLampLight` from the red discount. Modded flame lights with a custom class would still be misclassified (config table later).

## 6. Static map lights and config lights

- **Street lamps:** `StaticObj_Lamp_City1/2/3`, `StaticObj_Lamp_Ind`, `_Rail`, `_Harbour`, `_Stadium`, `StaticObj_Airfield_Lamp` (in `P:\DZ\structures*\...\config.cpp`) are plain `HouseNoDestruct` with only a `model`. They have no light class or `Reflectors`. `Land_Lamp_City1_amp` and the `*_Lamp_Amp` power poles are `PASReceiver` loudspeakers (`4_world/entities/advancedcommunication/`). **No vanilla street lamp or lit window is a light source in script or config.** Any glow would be baked emissive material, which our scan can't see and which doesn't matter for the dose.
- **Config `Reflectors`** (engine-handled lights, *not* `ScriptedLightBase`): found only in `P:\DZ\characters\headgear\config.cpp` (`PumpkinHelmet` at l.1830, orange 1,0.55,0, B 1, R 20, 70°; `Headtorch_ColorBase` at l.2788) and `P:\DZ\gear\camping\config.cpp` (`Spotlight` at l.10292, B 20, R 60, 140°). Headtorch and Spotlight also create script lights, so the config entry may be redundant or inactive (**Unverified**). **PumpkinHelmet is the only vanilla light that is config-only** and would be missing from a `ScriptedLightBase` registry. It's minor; it could be added via an item check if wanted.
- No vehicle configs contain `Reflectors`, so all car lights are scripted.
- **Engine-side far lights:** `P:\DZ\data\config.cpp` ~l.1662 has `farLightSpotMul`/`farLightPointMul`/`farLightStartLerp` (distance-based light flare LOD). This is renderer-only.

## 7. Existing registries in vanilla

- `ScriptedLightBase.m_NightTimeOnlyLights` (static `set`, filled in `EOnInit`, cleaned up in the destructor). Its only user is `UndergroundHandlerClient.EnableLights()` (4_world/classes/undergroundhandlerclient.c:488), which toggles `SetVisibleDuringDaylight` when underground. It **excludes every light constructed with `SetVisibleDuringDaylight(true)`**: flashlight, headtorch, pistol and rail lights, spotlight, car headlights, bonfire, tire pile, road flare, flare-gun flares, muzzle flashes and flashbangs. **Not sufficient.**
- `Object.IsScriptedLight()` (3_game/entities/object.c:766, overridden to true in `ScriptedLightBase`) is used by `VicinityItemManager` to skip lights in `GetObjectsAtPosition` results. This confirms lights do show up in world object queries, but scanning that way is the slow path.
- No other light manager exists.

## 8. APIs needed by the dose code — Verified signatures

| Need | API | Where |
|---|---|---|
| Game instance | `g_Game` (global `DayZGame`) or `GetGame()` (`DayZGame GetGame()` at 3_game/gameplay.c:636). Vanilla mostly uses `g_Game` (about 2700 uses in 4_world) | 3_game/dayzgame.c:3971 |
| Camera | `vector GetCurrentCameraPosition()`, `vector GetCurrentCameraDirection()` on CGame | 3_game/global/game.c:730-731 |
| Light world position | `GetTransform(out vector mat[4])` (world) → `mat[3]` position, `mat[2]` forward (**axis Unverified**); also `GetWorldPosition()` (object.c:297) | 1_core/proto/enentity.c:288 |
| Local vs world | `GetLocalTransform`, `CoordToParent`, `VectorToParent` exist. `GetPosition()` on an attached child is probably local (vanilla attaches via `SetPosition(local_pos)` then `AddChild`) → **Unverified, prefer GetTransform / GetWorldPosition** | enentity.c:318, 473 |
| Raycast | `static bool DayZPhysics.RaycastRV(vector begPos, vector endPos, out vector contactPos, out vector contactDir, out int contactComponent, set<Object> results = NULL, Object with = NULL, Object ignore = NULL, bool sorted = false, bool ground_only = false, int iType = ObjIntersectView, float radius = 0.0, CollisionFlags flags = CollisionFlags.NEARESTCONTACT)` | 3_game/global/dayzphysics.c:199 |
| Raycast (physics) | `static bool DayZPhysics.RayCastBullet(vector begPos, vector endPos, PhxInteractionLayers layerMask, Object ignoreObj, out Object hitObject, out vector hitPosition, out vector hitNormal, out float hitFraction)` | dayzphysics.c:211 |
| Intersect types | `ObjIntersectFire(0)`, `ObjIntersectView(1)`, `ObjIntersectGeom(2)`, `ObjIntersectNone(4)` | dayzphysics.c:67-71 |
| World query | `void GetObjectsAtPosition(vector pos, float radius, out array<Object> objects, out array<CargoBase> proxyCargos)`, plus `...3D` | game.c:922/929 |
| Timing | `proto native int TickCount(int prev)` (CPU ticks since `prev`), `g_Game.GetTime()` (ms, used by lights) | 1_core/proto/ensystem.c:106 |
| Vectors | `vector.Dot(v1,v2)` (static), `v.Normalized()`, `v.Normalize()` (returns length), `v.Length()`, `v.LengthSq()`, `vector.Distance()`, `vector.DistanceSq()`, `vector.Direction(p1,p2)` | 1_core/proto/enconvert.c:153-284 |
| Math | `Math.Pow(float v, float power)`, `Math.EULER` (const) | 1_core/proto/enmath.c:183, :11 |
| Ownership | `EntityAI.GetHierarchyRootPlayer()`, `GetHierarchyRoot()`, `GetHierarchyParent()`; native `IEntity.GetParent()` | 3_game/entities/entityai.c:875-881, enentity.c:571 |
| Visibility | `IsFlagSet(EntityFlags.VISIBLE)` (used by `HeadtorchLight.PerformVisibilityCheck`) | enentity.c:643 |

## 9. Registry hook: constructor vs EOnInit

Vanilla contains **no** example of a modded class with a constructor. The only `modded class` occurrences are commented-out examples in `particlelist.c`, `pperequesterbank.c:242` and `worldlighting.c:30`. `ScriptedLightBase`'s own constructor takes no arguments, so a modded parameterless constructor would be syntactically fine and, by Enforce's ctor chaining, would also run (community practice; **Unverified here**). That said, the cleanest hook is the one vanilla already uses:

```c
// 4_World/EyeAdapt/ScriptedLightBase.c
modded class ScriptedLightBase
{
    // weak refs (no 'ref'): IEntity is Managed, so entries auto-null on delete
    static ref array<ScriptedLightBase> s_EyeAdaptLights = new array<ScriptedLightBase>();

    override void EOnInit(IEntity other, int extra)
    {
        super.EOnInit(other, extra);            // keeps m_NightTimeOnlyLights behaviour
        s_EyeAdaptLights.Insert(this);
    }

    void ~ScriptedLightBase()                   // destructors chain; vanilla dtor still runs
    {
        if (s_EyeAdaptLights)
            s_EyeAdaptLights.RemoveItem(this);
    }
}
```

Why `EOnInit`: it runs after all subclass constructors (vanilla relies on this to read `IsVisibleDuringDaylight()`), it is guaranteed because the base ctor sets `EntityEvent.INIT`, and no subclass overrides it. Belt and braces: in the 0.5 s scan, also drop `null` entries (`for (i = count-1; i >= 0; i--) if (!list[i]) list.Remove(i);`), so even a missed destructor can't leak. Use `RemoveItem` (it preserves order and isn't needed for performance) or an `unordered` swap-remove; both are fine for dozens of entries.

**Unverified:** a modded destructor declared alongside the vanilla one. Verify with a log line in-game. If it causes issues, rely on weak-ref null-pruning alone.

## 10. Implications for EyeAdapt

1. **Registry:** hook `ScriptedLightBase.EOnInit` as above. It covers every vanilla dynamic light, plus modded lights built on the same base, which is the norm. Optional extra: `PumpkinHelmet` (config-only reflector).
2. **Exclude:** `PersonalLight` (camera-attached ambient fill), and optionally muzzle flashes (`MuzzleFlashLight*`, 10-80 ms). Explosions and flashbangs live only 0.15-0.35 s, so a 0.5 s scan will usually miss them. If glare from them is wanted, hook their creation instead.
3. **Per-light intensity:** `if (!light.IsEnabled()) skip;` then `float b = light.m_Brightness; float r = light.m_Radius;` (**not** `GetBrightness()`). Colour comes from `GetDiffuseColorV()`. Spot vs point comes from `GetLightType() == LightSourceType.SpotLight`, and the cone from `GetSpotLightAngle()` (degrees; whether it is the full cone or the half angle is **Unverified**, vanilla values of 95-140 suggest full cone). A config table keyed on `light.ClassName()` (or `light.IsInherited(FireplaceLight)`) can override `b`, so admins tune per type and red gets a low factor. A generic fallback derived from `m_Brightness × colour` handles unknown modded lights.
4. **Using the light's own radius** is a natural dead-zone: `if (dist > r) skip;` (campfire 25 m, flashlight 25 m, headlights up to 100 m, chemlight 7.5 m).
5. **Direction for spot lights:** `vector m[4]; light.GetTransform(m); pos = m[3]; fwd = m[2];` then require `vector.Dot(fwd, (eye - pos).Normalized()) > cos(angle/2)`. Test the forward axis in-game. Car rear lights are attached with orientation `"180 0 0"` (carscript.c:2183), which supports "forward = +Z".
6. **Own light:** walk `IEntity p = light.GetParent();` up the chain. If it reaches `g_Game.GetPlayer()`, the light is ours (headtorch on head). Also check `EntityAI.Cast(light.GetAttachmentParent()).GetHierarchyRootPlayer() == g_Game.GetPlayer()` for an in-hand flashlight or a weapon light. Don't rely on `m_Parent` alone.
7. **Occlusion:** `DayZPhysics.RaycastRV(eye, lightPos, cp, cd, comp, null, lightParentObj, g_Game.GetPlayer(), false, false, ObjIntersectView)`, passing the light's parent object as `with` (ignored) so the fireplace or flashlight model itself doesn't occlude. **Unverified:** the exact `with` semantics. Test it.
8. **Corrected §5.5 pseudo-code:**

```c
float CalcLightDose()
{
    vector eyePos = g_Game.GetCurrentCameraPosition();
    vector eyeDir = g_Game.GetCurrentCameraDirection();
    float total = 0;
    array<ScriptedLightBase> lights = ScriptedLightBase.s_EyeAdaptLights;
    for (int i = lights.Count() - 1; i >= 0; i--)
    {
        ScriptedLightBase light = lights[i];
        if (!light) { lights.Remove(i); continue; }
        if (!light.IsEnabled() || light.IsInherited(PersonalLight)) continue;

        float intensity = EyeAdaptLightTable.Get(light);   // config factor × light.m_Brightness
        if (intensity <= 0) continue;

        vector m[4]; light.GetTransform(m);
        vector toLight = m[3] - eyePos;
        float dist = toLight.Length();
        if (dist > Math.Min(SCAN_RADIUS, light.m_Radius)) continue;
        vector dirToLight = toLight.Normalized();

        float view = vector.Dot(eyeDir, dirToLight);
        if (view <= 0) continue;

        if (light.GetLightType() == LightSourceType.SpotLight)
        {
            float beam = vector.Dot(m[2], -dirToLight);      // is the beam pointing at me?
            if (beam < Math.Cos(light.GetSpotLightAngle() * 0.5 * Math.DEG2RAD)) continue;
        }

        float d = intensity * view / Math.Max(Math.Pow(dist, FALLOFF_EXP), 1);
        if (d < DOSE_THRESHOLD) continue;
        // optional occlusion via DayZPhysics.RaycastRV(...)
        total += d;
    }
    return 1 - Math.Pow(Math.EULER, -total * SENSITIVITY);
}
```

9. **Claude.md §8 status:**
   - **Q4 answered:** every vanilla dynamic light derives from `ScriptedLightBase` (inventory in §4). Car head and rear lights are `CarLightBase`/`CarRearLightBase` (spot). Static map lights don't exist as light sources. The only config-only light is `PumpkinHelmet`. Modded lights almost certainly use the same base.
   - **Q5 answered:** use `m_Brightness`/`m_Radius` (fields), `GetRadius()`, `GetDiffuseColorV()`/`GetAmbientColorV()`, `GetSpotLightAngle()`, `GetLightType()`, `IsEnabled()`. Avoid `GetBrightness()`.
   - **Q6 answered:** red light in vanilla comes from `Headtorch_Black` (red headtorch), `Chemlight_Red`, the road flare (red), the red flare-gun flare and buoys. No red flashlight.
   - **Q9 answered:** `GetCurrentCameraPosition/Direction` are correct (on `g_Game`), `TickCount(int prev)` exists, `vector.Dot`, `Normalized()`, `Length()`, `Math.Pow` and `Math.EULER` all exist. The raycast is `DayZPhysics.RaycastRV(...)` (full signature in §8). Prefer `g_Game` over `GetGame()`.

## Unverified / test in game

- Forward axis of a spot light's transform (`mat[2]`) and whether `GetSpotLightAngle()` is the full or the half cone.
- `GetPosition()` on attached lights: local or world.
- Whether a modded destructor coexists with the vanilla one (expected yes).
- Whether `Reflectors` in configs (headtorch, spotlight, PumpkinHelmet) produce actual engine lights.
- `RaycastRV` `with`/`ignore` behaviour for the light's parent model.
- Whether particle effects (`.ptc`) carry their own lights. Not visible from script.
