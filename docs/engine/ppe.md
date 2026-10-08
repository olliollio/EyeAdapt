# PPE (post-processing) and eye accommodation: verified notes

Source: extracted vanilla scripts at `P:\scripts` and data at `P:\graphics`, `P:\DZ`. All paths below are relative to `P:\scripts` unless noted. Line numbers refer to the version extracted on 2026-10-07.

Legend: **[V]** = read in source, **[D]** = derived from reading the code (logic traced, not run), **[U]** = unverified, needs in-game testing.

---

## 0. Overview

- PPE ("PostProcess Effects") is a **script-side** system in `3_game/ppemanager/`. It wraps engine materials (`.emat`) and a few native setters ("exceptions": EV, eye accommodation, DOF, NV light params).
- Flow: a **requester** (singleton, registered in `PPERequesterBank`) calls `SetTargetValue*(material, param, value, priorityLayer, operator)` → `PPEManager` collects all requesters' values per material parameter → blends them **by priority layer and operator** → writes the final value once per frame via `Material.SetParamByIndex` or the native setter.
- **There is no built-in time fading.** Values are applied in the same frame. Every fade you see in vanilla is done by the requester or the caller (e.g. `FlashbangEffect.Update`). [V]
- All requesters are client-only in practice (they touch rendering). The manager is created in `DayZGame` (`3_game/dayzgame.c:1014`) and initialised in `DeferredInit` (`dayzgame.c:1136`). It updates on `CALL_CATEGORY_GUI` (`ppemanager.c:95`). [V]

---

## 1. Architecture

### 1.1 Classes and files

| Class | File | Role |
|---|---|---|
| `PPEManagerStatic` / `PPEManager` | `3_game/ppemanager/ppemanager.c` | singleton, update loop, material class map |
| `PPERequesterBank` | `3_game/ppemanager/pperequesterbank.c` | registry of requester singletons, IDs |
| `PPERequesterRegistrations` | `pperequesterbank.c:229` | **the modding hook** for custom requesters |
| `PPERequesterBase`, `PPERequester_GameplayBase`, `PPERequester_MenuBase` | `3_game/ppemanager/requesters/pperequestplatformsbase.c` | requester base, setters |
| `PPERequestParamData*` | `3_game/ppemanager/pperequestdata.c` | per-requester per-param value holder |
| `PPEClassBase` | `3_game/ppemanager/materials/ppematclassesbase.c` | script mirror of one material |
| `PPEMatClassParameterFloat/Color/...` | `3_game/ppemanager/materials/matparameters/` | **blending logic** |
| `PPEConstants`, `PPOperators`, `PPEExceptions`, `PPERequesterCategory` | `3_game/ppemanager/ppeconstants.c` | enums |
| `PPEffects` | `3_game/ppeffects.c` | old static API, marked "Deprecated; 'PPEManager' used instead" (line 1) |

### 1.2 Manager update (per frame) [V] `ppemanager.c:328-337`

```c
void Update(float timeslice) {
    ProcessRequesterUpdates(timeslice);  // req.OnUpdate() for requesters flagged "updating"
    ProcessMaterialUpdates(timeslice);   // blend queued params (PPEMatClassParameter*.Update)
    ProcessApplyValueChanges();          // write to material / native
}
```

### 1.3 Requester registration (how a mod adds one) [V]

`PPERequesterBank` registers vanilla requesters via static consts (`REQ_CAMERANV = RegisterRequester(PPERequester_CameraNV)` etc., `pperequesterbank.c:12-47`). For mods, Bohemia left an explicit hook (`pperequesterbank.c:228-248`):

```c
//! Mod this, if you wish to register custom requesters inside 'RegisterAdditionalRequesters' override
class PPERequesterRegistrations extends Managed {
    void PPERequesterRegistrations() { RegisterAdditionalRequesters(); }
    protected void RegisterAdditionalRequesters() { }
}
```

So EyeAdapt does:

```c
// 3_Game (the requester class must be in 3_Game too, because this modded class lives there)
modded class PPERequesterRegistrations {
    override protected void RegisterAdditionalRequesters() {
        super.RegisterAdditionalRequesters();
        PPERequesterBank.RegisterRequester(PPERequester_EyeAdapt);
    }
}
// usage anywhere later:
PPERequester_EyeAdapt req = PPERequester_EyeAdapt.Cast(PPERequesterBank.GetRequester(PPERequester_EyeAdapt));
```

- `RegisterRequester` spawns the instance via `type.Spawn()`, assigns the next ID (`m_lastID++`), returns the ID (`pperequesterbank.c:172-192`). [V]
- Instances created outside the bank fail `VerifyRequester` and are ignored ("not valid") (`pperequesterbank.c:195-215`, `pperequestplatformsbase.c:14,25-29`). Always use `GetRequester`. [V]
- `GetRequester(typename)` is a linear search over all instances; cache the reference. [V]

### 1.4 Requester lifecycle [V] `pperequestplatformsbase.c`

| Method | Behaviour |
|---|---|
| `Start(Param par = null)` | calls `OnStart(par)`, sets `m_IsRunning = true` (l.38) |
| `Stop(Param par = null)` | **returns early if the requester never set any value** (`m_RequestDataStructure.Count() <= 0`), else `OnStop(par)`, `m_IsRunning = false` (l.44-51) |
| `OnStart` (protected) | `SetRequesterUpdating(true)` (l.354) |
| `OnStop` (protected) | `SetRequesterUpdating(true)` + `SetDefaultValuesAll()` → all params this requester touched are released (l.360-365) |
| `OnUpdate(float delta)` | sends queued values to the manager, then `SetRequesterUpdating(false)` (l.336-352). Only called while the requester is flagged "updating". |
| `GetCategoryMask()` | `PPERequester_GameplayBase` → `GAMEPLAY_EFFECTS` (2), `PPERequester_MenuBase` → `MENU_EFFECTS` (4) |
| `IsRequesterRunning()` | `m_IsRunning` |

Setters (all `protected`, so they are called from methods on your requester subclass):

```c
SetTargetValueBool (int mat, int param, bool val, int layer, int op = PPOperators.SET)
SetTargetValueInt  (int mat, int param, bool relative, int val, int layer, int op = PPOperators.SET)
SetTargetValueFloat(int mat, int param, bool relative, float val, int layer, int op = PPOperators.ADD_RELATIVE)
SetTargetValueColor(int mat, int param, array<float> val, int layer, int op = PPOperators.ADD_RELATIVE)
SetTargetValue{Bool,Int,Float,Color}Default(int mat, int param)   // release this param
```

- Each setter calls `QueueValuesSend()` → `SetRequesterUpdating(true)`, so **calling a setter every tick keeps the requester alive in the update loop** (this is how `PPERequester_ContaminatedAreaTint.OnUpdate` animates: `ppercontaminated.c:28-60`). Setters skip the send if the value didn't change (float/int/bool; color always sends). [V]
- `relative` (float): `false` → value is absolute and gets normalised with `Math.InverseLerp(min, max, val)` clamped 0..1 using the **script-registered min/max** (`RelativizeValue`, l.368-385). `true` → you pass the 0..1 normalised value directly. [V]
- `StopAllEffects(mask)` is called by vanilla on: player init (`playerbase.c:6113`, ALL), respawn (`playerbase.c:2427`, GAMEPLAY_EFFECTS), mission finish (`missiongameplay.c:273`, ALL). `DayZGame.LoadingHide` calls it with mask 0 which matches nothing (`dayzgame.c:3464`). [V] → Our requester will get stopped on death/respawn; re-apply values afterwards.

### 1.5 Blending: layers and operators [V] `matparameters/ppematclassparameterfloat.c:34-225`

Per material parameter, each requester contributes `{value, operator}` on its **priority layer** (int). Each frame:

1. `float_value_total = m_ValueDefault / m_ValueMax` (normalised start, **note: not InverseLerp**, l.129-132).
2. Layers are sorted ascending (`AddPriorityInfo` → `m_CommandLayersArray.Sort()`, `ppematclassparametercommanddata.c:81-92`) and applied in order:

| `PPOperators` | float formula (normalised space) |
|---|---|
| `LOWEST` | `min(total, v)` |
| `HIGHEST` | `max(total, v)` |
| `ADD` | `total + v` |
| `ADD_RELATIVE` | `(1 - total) * v + total` |
| `SUBSTRACT` | `total - v` |
| `SUBSTRACT_RELATIVE` | `total - v * total` |
| `SUBSTRACT_REVERSE` | `v - total` |
| `SUBSTRACT_REVERSE_RELATIVE` | `v * total - total` |
| `MULTIPLICATIVE` | `total * v` |
| `SET` | `v` (later layers still apply) |
| `OVERRIDE` | `v`, and stops processing higher layers |

3. Clamp to 0..1 after each layer, then `Math.Lerp(min, max, total)` back to absolute (l.197, 217). **The script-registered min/max are hard caps.**
4. If no active requests remain → the **script-registered default** is written (`SetParameterValueDefault`, l.234-239).

Colors (`ppematclassparametercolor.c:69-279`) work on raw RGBA (no normalisation); `ADD_RELATIVE` = `ADD`, `SUBSTRACT_RELATIVE` = `SUBSTRACT` (relative variants commented out as "no known max values, unusable"). Start = registered default color.

Gotchas [D]:
- **Layer collisions:** `m_LayerInfo` is a `map<layer, ...>`, so two requesters on the **same layer** of the same param overwrite each other (whichever is iterated last wins). Pick layer numbers not used by vanilla.
- **Exposure ADD stacking is broken:** EV param is registered `-100..100`, default 0 (`ppeexposurenative.c:24`). Start total = `0/100 = 0` (= -100 EV in normalised space). An absolute `+7` EV gets relativised to `0.535`. Alone, `0 + 0.535` → +7 EV (works by accident). Two ADD layers (e.g. NVG +7 at layer 100 and flashbang at 400) → `0.535 + 0.5..0.75` → clamped to 1 → **+100 EV**. A `SET X` below an ADD layer has the same problem (`rel(X) + 0.5+`). Any EyeAdapt exposure layer must therefore **not coexist** with vanilla ADD layers on EV. Verify with `g_Game.GetCurrentEVValue()` logging. [D][U]
- `Saturation` has a dependency: changing it re-evaluates `ColorizationColor` and `OverlayColor` in a second pass (`PPEMatClassParameterFloatSaturation`, `ppematclassparameterfloat.c:242-249`), and the colorization color is lerped toward its brightest channel by `EaseOutSine(saturation)` (`PPEMatClassParameterColorColorization`, `ppematclassparametercolor.c:317-346`). So low saturation also washes out any tint (ours, NVG green, toxic tint). [V]
- Script defaults **don't always match the .emat** (see §2.2). Releasing a param writes the script default, not the emat value. [D]

---

## 2. Material classes and parameter IDs

Registered in `PPEManager.InitPPEManagerClassMap` (`ppemanager.c:101-136`). Material ID = `PostProcessEffectType` enum (`1_core/proto/enworld.c:71-98`: ..., `FilmGrain`=9, ..., `Colors`=15, `Glow`=16, ..., `GaussFilter`=21) or `PPEExceptions` (`ppeconstants.c:42-49`: `EXPOSURE`=50, `DOF`=51, `EYEACCOM`=52, `NVLIGHTPARAMS`=53).

Format: `CONST = idx : "Name" default [min..max]` (script registration). [V]

### 2.1 `PPEGlow` (`Graphics/Materials/postprocess/glow`) — `matclasses/ppeglow.c`

This is **the** material for both engine auto-exposure (HDR) and colour grading. All vanilla colour effects use Glow, not Colors.

| Group | Params |
|---|---|
| HDR / eye adaptation | `PARAM_VISIBLEPERCENT`=0 "VisiblePercent" 95 [1..100]; `PARAM_TARGETBRIGHTNESS`=1 "TargetBrightness" 0.3 [0.01..1]; **`PARAM_SPEEDDARKTOBRIGHT`=2 "SpeedDarkToBright" 0.3 [0.01..1]; `PARAM_SPEEDBRIGHTTODARK`=3 "SpeedBrightToDark" 0.3 [0.01..1]**; `PARAM_TONEMAPPING`=4 int 2 [0..2]; `PARAM_HDR`=5 bool 1 |
| Filmic tonemap | 6 `FilmicShoulderStrength` 0.153 [0..1]; 7 `FilmicLinearStrength` 0.357 [-1..1]; 8 `FilmicLinearAngle` 0.231 [-1..1]; 9 `FilmicToeStrength` 0.1573 [0.001..3]; 10 `FilmicToeNumerator` 0.011 [-1..1]; 11 `FilmicToeDenumerator` 3.75 [0.1..20]; 12 `FilmicExposureBias` 4.0 [0.1..20]; 13 `FilmicWhitePoint` 1.5 [0.1..20] |
| Bloom | `PARAM_BLOOMTHRESHOLD`=14 0.5 [0..1]; `PARAM_BLOOMSTEEPNESS`=15 1 [0..10]; `PARAM_BLOOMINTENSITY`=16 1 [0..4] |
| Colour | `PARAM_BRIGHTNESS`=17 1 [0..2]; `PARAM_CONTRAST`=18 1 [0..2]; `PARAM_OFFSET`=19 0 [-1..1]; `PARAM_OVERLAYFACTOR`=20 0 [0..1]; `PARAM_OVERLAYCOLOR`=21 color (1,1,1,0); `PARAM_SATURATION`=22 1 [0..1]; `PARAM_COLORIZATIONCOLOR`=23 color (1,1,1,0); `PARAM_DESATURATIONWEIGHTS`=24 vector (0.299,0.587,0.114,0) |
| Vignette | `PARAM_VIGNETTE`=25 0 [0..2] (comment: C++ max 1, accepts more); `PARAM_VIGNETTECOLOR`=26 color (0,0,0,0) |
| Lens | 27 `LensDistort` 0 [-5..5]; 28 `MaxChromAbberation` 0 [0..1]; 29/30 `LensCenterX/Y` 0 [-1..1] |

Vanilla layer constants in `PPEGlow` (l.42-75): `L_20_HIT`=100, `L_20_FLASHBANG`=300, `L_20_SHOCK`=500; `L_21_SHOCK`=100, `L_21_HIT`=300, `L_21_FLASHBANG`=500; `L_22_BLOODLOSS`=100; `L_23_GLASSES`=100, `L_23_TOXIC_TINT`=200, `L_23_HMP`=300, `L_23_NVG`=600; `L_25_*` vignette 100..800; `L_26_*` 100..800; `L_27..30_ADS`=100. (Suffix number = param index.)

### 2.2 `glow.emat` actual values [V] `P:\graphics\materials\postprocess\glow.emat`

```
GlowEffect {
 VisiblePercent 95
 TargetBrightness 0.25
 SpeedDarkToBright 6
 SpeedBrightToDark 6
 ToneMapping Filmic
 FilmicLinearAngle 0.423
 BloomThreshold 0.5
 BloomSteepness 1.25
 BloomIntensity 0.75
}
```

**Mismatches with the script registration:** `TargetBrightness` 0.25 vs 0.3; `SpeedDarkToBright`/`SpeedBrightToDark` **6** vs 0.3 with script max **1**; `FilmicLinearAngle` 0.423 vs 0.231; `BloomSteepness` 1.25 vs 1; `BloomIntensity` 0.75 vs 1. Vanilla never requests these params, so the mismatch is latent. If we touch them through PPE: (a) we can't exceed the registered max (speeds capped at 1, i.e. 6x slower than vanilla), (b) on release PPE writes the script default (0.3 / 1 / ...), permanently changing the look until restart. [D]

Other relevant emats (all under `P:\graphics\materials\postprocess\`): `colors.emat` (`ColorsEffect`: Brightness 1, Contrast 1, Offset 0, OverlayFactor 0, OverlayColor "1 1 1 0", Saturation 1, ColorizationColor "1 1 1 0", DesaturationWeights "0.299 0.587 0.114 0"), `filmgrainnv.emat`, `filmgrain.emat`, `gauss.emat`, `radialblur.emat`, `chromaber.emat`. `PPEColors` has the comment "'ColorsEffect' type may be used differently in c++, no emat linked to it? Investigate." (`ppecolors.c:2`) → **don't use PPEColors, use PPEGlow.**

### 2.3 Other material classes

| Class (file) | Params | Vanilla layers |
|---|---|---|
| `PPEExposureNative` (`exceptions/ppeexposurenative.c`), ID `PPEExceptions.EXPOSURE` | `PARAM_INTENSITY`=0 "Intensity" 0 [-100..100] → `g_Game.SetEVValue(v)` | `L_0_NVG_OPTIC`=100, `L_0_NVG_GOGGLES`=200, `L_0_NVG_OFF`=300, `L_0_FLASHBANG`=400, `L_0_BURLAP`=500, `L_0_DEATH`=1000 |
| `PPEEyeAccomodationNative` (`exceptions/ppeeyeaccomodationnative.c`), ID `EYEACCOM` | `PARAM_INTENSITY`=0 1.0 [0..1000] → `g_Game.GetWorld().SetEyeAccom(v)` | `L_0_UNDERGROUND`=100, `L_0_NVG_GENERIC`=300, `L_0_BURLAP`=500 |
| `PPELightIntensityParamsNative` (`exceptions/ppelightintensityparamsnative.c`), ID `NVLIGHTPARAMS` | `PARAM_LIGHT_MULT`=0 "lightIntensityMul" 1 [0..50]; `PARAM_NOISE_MULT`=1 "noiseIntensity" 0 [0..50] → `g_Game.NightVissionLightParams(mul, noise)` | `L_0_NVG`=100, `L_0_TOXIC_TINT`=200, `L_1_NVG`=100, `L_1_TOXIC_TINT`=200 |
| `PPEFilmGrain` (`ppefilmgrain.c`, material `filmgrainNV`) | 0 Intensity 0 [0..1]; 1 Sharpness 2.35 [0..20]; 2 GrainSize 2.75 [1..9]; 3/4 IntensityX0/X1 0 [0..1]; 5 Monochromatic bool true; 6 Simple bool false; 7 Distort bool true; 8 Frequency 20 [1..1000]. **Comment l.2-4: grain only shows if `NightVissionLightParams` noiseIntensity is non-zero** (`PPELightIntensityParamsNative.PARAM_NOISE_MULT`). | `L_1_NVG`=100, `L_1_TOXIC_TINT`=200, `L_2_NVG`=100, `L_2_TOXIC_TINT`=200 |
| `PPEGaussFilter` (`ppegaussfilter.c`) | 0 Intensity 0 [0..1] | ADS 100, SHOCK 200, FEVER 300, FLASHBANG 400, INV 500, MENU 600, CONTROLS 700, LATENCY 750, TUTORIALS 800, FEEDBACK 850, SERVER_BROWSER 900, DISCONNECT 1000 |
| `PPERadialBlur` (`pperadialblur.c`) | 0 PowerX 0 [0..0.1]; 1 PowerY; 2/3 OffsetX/Y 0.05 [0..0.5]; 4 PixelScale 0.5 [0.125..1] | `L_0_PAIN_BLUR`=100 |
| `PPEChromAber` (`ppechromaber.c`) | 0/1 PowerX/Y 0 [0..0.05] | INTRO 100 |
| `PPEColors` (`ppecolors.c`) | same colour params as Glow 17-24 at idx 0-7 | none; avoid |
| `PPEColorGrading`, `PPEMedian` | no usable params ("TODO", "unused/not modifiable from script?") | – |

Also registered: UnderWater, SSAO, DepthOfField, HBAO, RotBlur, GodRays, Rain, WetDistort, DynamicBlur, SMAA, FXAA, SunMask, DOF (exception), Distort, Ghost. Not relevant.

---

## 3. Native functions related to exposure / lighting [V]

| Function | Declared | Notes |
|---|---|---|
| `DayZGame.SetEVValue(float)` | `3_game/dayzgame.c:3838` | stores prev/current, calls native `SetEVUser`. Getters `GetCurrentEVValue()` (3845), `GetPreviousEVValue()` (3850) |
| `Game.SetEVUser(float)` | `3_game/global/game.c:1352` | "Sets custom camera camera EV. range: -50.0..50.0? //TODO" |
| `World.GetEyeAccom()` / `SetEyeAccom(float)` | `3_game/global/world.c:58-59` | underground uses 0..1 (1 = normal, lower = darker). `World.SetAperture(invDiameter)` (l.154-165) calls `SetEyeAccom(-1)` for `<= 0` (reset) else `SetEyeAccom((1/invDiameter)^2)`. Only used by the inactive `Blinded` modifier (`4_world/.../conditions/blinded.c`, `ActivateCondition` returns false). |
| `Game.NightVissionLightParams(lightIntensityMul, noiseIntensity)` | `game.c:1374` | "multiplicator of lights intensity", "intensity of noise PP" |
| `World.LoadNewLightingCfg(path)` | `world.c` | main lighting config; selected by `WorldLighting.SetGlobalLighting(id)` (`3_game/worldlighting.c`): 0 default, 1 darknight, 2 sakhal; moddable (commented example) |
| `World.LoadUserLightingCfg(path, name)` / `SetUserLightingLerp(float)` | `world.c` | a second lighting config blended in at runtime. Used **only** by `UndergroundHandlerClient` (`4_world/classes/undergroundhandlerclient.c:39,334-341`) with `dz\data\lighting\lighting_underground.txt` |
| `GetSceneHDRMul(int camera)` | `1_core/proto/enworld.c:160` | "scene light multiplier (based on measured scene light levels) - pre-exposure of light". Used by the breath effect, clamped 0.005 ("dusk?") .. 1.0 ("dark?") (`5_mission/.../gameplayeffectwidgets.c:549-568`) |
| `World.GetMoonIntensity()`, `GetSunOrMoon()`, `IsNight()` | `world.c:54-56` | useful for §3.2 moon scaling |
| `Material.SetParam(name, value)`, `ResetParam(name)`, `GetParamIndex(name)`, `SetParamByIndex(idx, value)` | `1_core/proto/proto.c:267-295` | **no getter**. `ResetParam` "reset parameter of material to default value" |
| `World.GetMaterial(path)` | `world.c` | e.g. `"Graphics/Materials/postprocess/glow"` |

No script access to **EVMin/EVMax** exists: they appear only in the lighting config files (no `EVMin`/`EVMax` symbol in `P:\scripts`). [V]

### Lighting configs [V] `P:\DZ\data\lighting\`

- `lighting_default.txt`, `lighting_darknight.txt`, `lighting_underground.txt`. Per sun-angle stage (`C_Night`, `C_Astro1..3`, `C_Nautical1..3`, `C_Civil1..3`, `C_Day*`, and overcast `O_*`): `diffuse`, `ambient`, `sky`, `desiredLuminanceCoef(Cloud)`, `EVMin`, `EVMax`, `filmGrainIntensity`, ...
- Night stages: `EVMin = -2.0`, `EVMax = 0.0` (comments: "scene gets darker with higher value (default -2.0)" / "scene gets brighter with higher value (default 0.5)"). Nautical/civil: EVMax 0.5 / 0.2.
- `desiredLuminanceCoef` at night: default **0.12**; darknight **-0.14** (`C_Night`), **-0.22** (`O_Night`). darknight also adds `filmGrainIntensity = 0.0125` at night.

---

## 4. Night vision template [V]

`PPERequester_CameraNV` (`3_game/ppemanager/requesters/ppercameranv.c`):

- Modes as int consts: `NV_NO_BATTERY`=0, `NV_DEFAULT_OPTICS`=1, `NV_DEFAULT_GLASSES`=2, `NV_PUMPKIN`=3, `NV_TRANSITIVE`=4, `NV_DAYTIME_OPTICS`=5.
- `OnStart(par)` → `SetNVMode(Param1<int>.Cast(par).param1)`. `OnStop` just calls super (auto-reset of all touched params).
- `SetNVMode` always sets `EYEACCOM = 1.0` (`SET`, layer `L_0_NVG_GENERIC`=300), i.e. **neutralises the underground eye-accommodation darkening**, then e.g. goggles:

```c
SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR, {0.0,1.0,0.0,0.0}, PPEGlow.L_23_NVG, PPOperators.MULTIPLICATIVE);
SetTargetValueFloat(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY, false, 7.0 * m_UGExposureCoef, PPEExposureNative.L_0_NVG_GOGGLES, PPOperators.ADD);
SetTargetValueFloat(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_SHARPNESS, false, 10.0, PPEFilmGrain.L_1_NVG, PPOperators.SET);
SetTargetValueFloat(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_GRAINSIZE, false, 1.0, PPEFilmGrain.L_2_NVG, PPOperators.SET);
SetTargetValueFloat(PPEExceptions.NVLIGHTPARAMS, PPELightIntensityParamsNative.PARAM_LIGHT_MULT, false, 2.0, PPELightIntensityParamsNative.L_0_NVG, PPOperators.HIGHEST);
SetTargetValueFloat(PPEExceptions.NVLIGHTPARAMS, PPELightIntensityParamsNative.PARAM_NOISE_MULT, false, 1.0, PPELightIntensityParamsNative.L_1_NVG, PPOperators.HIGHEST);
```

  Optics: +7 EV, light mult 3, noise 2. Daytime optics: +2 EV. Pumpkin: orange colorization, +5.5 EV, no grain. No battery: colorization ×0 (black), EV released.
- `SetUndergroundExposureCoef(coef)` scales the EV boost underground (called by `UndergroundHandlerClient.UpdateNVGRequester`).
- **Who drives it:** `DayZPlayerCameraBase.SetNVPostprocess(int NVtype)` (`4_world/entities/manbase/dayzplayer/dayzplayercamera_base.c:478-543`) maps `NVTypes` (enum at l.54) to `PPERequesterBank.GetRequester(REQ_CAMERANV).Start(new Param1<int>(mode))`, or `.Stop()` for `NVTypes.NONE`. Camera checks `player.IsNVGWorking()` / `player.GetNVType()` each update (`UpdateCameraNV`, l.422). On camera transitions from NV it starts `NV_TRANSITIVE` (l.375-377).
- It also calls `PlayerBaseClient.SwitchPersonalLight(NVtype < 1)` (l.540-543): NVG turns the **personal light** off.

Template takeaways: one requester, mode-switch method that sets all params, `Start(Param)` to activate, `Stop()` to release everything. For a continuous 0..1 value, replace the mode switch with a `SetAdaptation(float a)` method called every tick (like `PPERequester_BloodLoss.SetBloodLossLevel`, `pperbloodloss.c`, or `PPERUndergroundAcco.SetEyeAccommodation`, `pperundergroundacco.c`).

---

## 5. FlashbangEffect [V]

`FlashbangEffect` (`4_world/classes/explosions/flashbangeffect.c`) + `PPERequester_FlashbangEffects` (`3_game/ppemanager/requesters/pperflashbangeffects.c`).

- Created by `PlayerBase.SpawnFlashbangEffect` (`playerbase.c:1172`); strength set by `FlashGrenade` via `SetupFlashbangValues(effectCoef, effectCoef, effectCoef)` where `effectCoef` = distance falloff between kill and max range, clamped 0.1.. (`4_world/entities/grenade_base/flashgrenade.c:63-73`), after a raycast visibility check and `IsPlayerOrientedTowardPos(player, pos, 60)`.
- Timing: `m_HitDuration = 8.0`, `m_BreakPoint = 2.5`, both × progress multiplier. `Update(dt)`: if `t <= breakPoint` → value = max; else `value = max * (1 - clamp(InverseLerp(hitDuration - breakPoint, hitDuration, t)))`. **Net effect: full strength until t = 5.5 s, then linear fade to 0 at 8 s** (the 2.5..5.5 s window also yields max because InverseLerp is negative and clamped). [D]
- Day/night: `m_DayTimeToggle = 5` by day, `10` if `GetDayTime() >= 22 || < 7`.
- `SetFlashbangIntensity(intensity, daytime_toggle)`:
  - Exposure: day → `clamp(intensity * 5, 0, 50)`; night → `50 * EaseInQuart(intensity)`; applied `ADD` at `L_0_FLASHBANG` (400).
  - Blur: `GaussFilter.PARAM_INTENSITY` relative `intensity`, `ADD_RELATIVE`, layer 400.
  - White-out: `Glow.PARAM_OVERLAYFACTOR` relative `intensity`, `HIGHEST`, `L_20_FLASHBANG`; `Glow.PARAM_OVERLAYCOLOR` = `{1,1,1,intensity}`, `SET`, `L_21_FLASHBANG`.
  - Plus a widget overlay `EffectWidgetsTypes.COVER_FLASHBANG` updated in requester `OnUpdate`.

For EyeAdapt glare: same building blocks (overlay white with small factor, EV bump, optional tiny gauss) but much softer and shorter. Note `EaseInQuart` at night keeps low intensities very weak, a useful shape for "only strong light glares".

---

### 5.1 Bloom from script: verified in game (2026-10-08)

- `Material.SetParam("BloomIntensity"/"BloomThreshold", v)` on the glow material returns true, but a **one-time** write has **no visible effect**: the engine refreshes the glow pass from its own values. Writing the value **every frame** works (clear halo around a campfire at intensity 2.75, threshold 0.2).
- Use the material object the PPE manager renders with: `m_PPEClassMap.Get(PostProcessEffectType.Glow).GetMaterial()` (protected, so a `modded class PPEManager` accessor). Vanilla's PPE path is `"Graphics/Materials/postprocess/glow"`.
- `Material` is a native handle (`typedef int[]`); vanilla checks it with `== 0x0`. There is no getter, so boosts are added to configured base values (vanilla `glow.emat`: intensity 0.75, threshold 0.5).
- **Don't use `Material.ResetParam(name)`** (suspected from in-game behaviour, confirm): after a `ResetParam`, later `SetParam` calls on that parameter had no visible effect. The glow worked on the first activation and never again after the first reset; an earlier "one-time write has no effect" observation also followed a reset. EyeAdapt writes configured base values back instead (vanilla 0.75 / 0.5) and stops writing. Also do that on death and in `OnMissionFinish`.

## 6. Other vanilla requesters that touch our params [V]

| Requester (file) | Param | Layer / op | When |
|---|---|---|---|
| `PPERequester_BloodLoss` (`pperbloodloss.c`) | Glow `SATURATION` | 100 `SET` (relative) | blood loss symptom (`4_world/classes/playersymptoms/states/secondary/bloodloss.c`), continuously while blood < max |
| `PPERequester_CameraNV` | EV, EYEACCOM, COLORIZATION, FilmGrain, NVLIGHTPARAMS | see §4 | NVG / NV optics |
| `PPERequester_FlashbangEffects` | EV, Gauss, OVERLAY | 400 / 300 / 500 | flashbang |
| `PPERequester_BurlapSackEffects` (`pperburlapsack.c`) | EV −100 `SET` 500; EYEACCOM 0 `SET` 500; vignette | – | sack on head |
| `PPERequester_DeathDarkening` (`pperdeathdarkening.c`) | EV `Lerp(0,-10,EaseInCubic(v))` `SET` 1000 | – | dying |
| `PPERUndergroundAcco` (`pperundergroundacco.c`) | EYEACCOM 0..1 `SET` 100 | – | caves/bunker, driven by `UndergroundHandlerClient` |
| `PPERequester_ContaminatedAreaTint`, `PPERequester_SpookyAreaTint` | COLORIZATION ×(0.86,0.85,0.56) `MULTIPLICATIVE` 200; FilmGrain sharpness/size | – | gas zones |
| `PPERequester_Glasses*` (`pperglasses.c`) | COLORIZATION `SUBSTRACT` 100 | – | sunglasses |
| `PPERequester_HMPGhosts`/`_3` | COLORIZATION `ADD` 300 | – | heavy metal poisoning |
| Unconscious / tunnel / shock / menu / drowning | VIGNETTE 100..800 | – | – |
| ADS optics (`ppercameraads_opt.c`), many menus | Gauss, DOF, lens | – | – |

Nobody in vanilla touches Glow `BRIGHTNESS`, `CONTRAST`, `OFFSET`, `BLOOM*`, `TARGETBRIGHTNESS`, `SPEED*`, `VISIBLEPERCENT`. [V] (grep over all scripts)

**Personal light:** `PlayerBaseClient` creates a client-only `PersonalLight` (`4_world/entities/scriptedlightbase/pointlightbase/personallight.c`, a `PointLightBase` → `ScriptedLightBase`, radius 3, brightness 0.38, follows the camera) if the server allows it (`disablePersonalLight` in cfggameplay / server cfg, `playerbaseclient.c`). Relevant twice: it brightens nights on its own, and the planned `ScriptedLightBase` registry must **exclude it** from light-dose scanning.

---

## 7. Eye accommodation / auto-exposure: what is and isn't script-accessible

Verified:
- The engine auto-exposure ("HDR" block of `GlowEffect`) is parameterised by `VisiblePercent`, `TargetBrightness`, **`SpeedDarkToBright`**, **`SpeedBrightToDark`**. **The speeds are two separate parameters**, so the engine supports asymmetric speeds; vanilla just sets both to 6. They are exposed as `PPEGlow.PARAM_SPEEDDARKTOBRIGHT` / `PARAM_SPEEDBRIGHTTODARK` and are settable via `Material.SetParam` / PPE. [V]
- `EVMin`/`EVMax` live only in the lighting config (per sun stage); no script getter/setter. The only runtime lever on the lighting config is `LoadNewLightingCfg` (whole config) or `LoadUserLightingCfg` + `SetUserLightingLerp` (one blended override, already used by the underground system). [V]
- `SetEVUser` (via `g_Game.SetEVValue`) is a "custom camera EV". `SetEyeAccom` is a separate native multiplier used for underground darkening (1 = normal). [V]

Unverified [U]:
- Whether `SetEVUser` is applied **after** the auto-exposure clamp (`EVMin`/`EVMax`), or the auto-exposure measures it and compensates. Strong indirect evidence that it is not capped: vanilla NVGs use exactly this channel (+7 EV) and visibly brighten even dark-night configs, and flashbangs push +50 EV to white-out at night.
- Whether `SetEyeAccom(> 1)` brightens (registered range goes to 1000, vanilla only uses 0..1). Values < 1 darken (underground).
- Units of `SpeedDarkToBright`/`SpeedBrightToDark` (6 = per second rate? higher = faster?), and whether runtime changes take effect immediately.
- Whether `Material.ResetParam(name)` restores the **emat** value (it should, per its doc comment).
- Exact meaning/range of `GetSceneHDRMul(0)` (breath effect treats 0.005 as bright/dusk and 1.0 as dark). Potentially a free "how bright is the screen right now" signal for glare detection.
- `NightVissionLightParams(lightIntensityMul)`: multiplies (presumably) dynamic light intensity. NVG uses 2-3. Not useful for brightening ambient night, could be useful only if we wanted to change local-light brightness.

---

## 8. Verified facts vs. needs in-game testing (summary)

**Verified in source**
- Mod requester registration via `modded class PPERequesterRegistrations` → `RegisterAdditionalRequesters()`.
- Layered blending per param, operators, ascending layer order, clamp to script min/max, no built-in fading.
- Same-layer collision overwrites; release writes script default.
- Glow param IDs/ranges above; glow.emat values; mismatches (speeds 6 vs script max 1).
- Separate SpeedDarkToBright / SpeedBrightToDark params exist.
- NVG = colorization ×green + EV ADD +7 + EyeAccom 1.0 SET + grain + light mult.
- Flashbang = EV ADD up to 50 at night, overlay white, gauss, hold-then-linear-fade (5.5 s → 8 s).
- Blood loss uses Glow saturation SET at layer 100.
- EVMin/EVMax only in lighting configs; night EVMin -2, EVMax 0.

**Derived from code reading (high confidence, but confirm with a log line)**
- EV `ADD` layers stack incorrectly (normalised-space offset), giving +100 EV when two are active. Do not mix our EV layer with NVG/flashbang.
- Touching Glow speed/bloom params through stock PPE permanently replaces emat values with script defaults on release.

**Needs in-game testing**
- Q1: does EV +1..+3 visibly brighten `lighting_darknight` / Lad-style configs, or is it re-normalised by auto-exposure?
- EyeAccom > 1 effect.
- Runtime effect and units of Speed* params; `ResetParam` behaviour.
- `GetSceneHDRMul` usefulness.

---

## 9. Implications for EyeAdapt

### 9.1 Requester design (recommended)

```c
// 3_Game/PPE/PPERequester_EyeAdapt.c
class PPERequester_EyeAdapt extends PPERequester_GameplayBase
{
    // unique layers, chosen to avoid vanilla values
    static const int L_EV        = 450;  // above NVG(100/200), flashbang(400); below burlap(500), death(1000)
    static const int L_SAT       = 150;  // above bloodloss(100)
    static const int L_TINT      = 250;  // between toxic(200) and HMP(300)
    static const int L_OVERLAY_F = 350;  // between flashbang(300) and shock(500) on OVERLAYFACTOR
    static const int L_OVERLAY_C = 550;  // above flashbang(500) on OVERLAYCOLOR -- or just skip color and use flashbang's white

    void SetState(float evBoost, float saturation, vector tint, float glare)
    {
        bool yieldEV = NVorFlashbangRunning();   // PPEManagerStatic.GetPPEManager().IsAnyRequesterRunning({PPERequester_CameraNV, PPERequester_FlashbangEffects})
        if (yieldEV || evBoost <= 0.001)
            SetTargetValueFloatDefault(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY);
        else
            SetTargetValueFloat(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY, false, evBoost, L_EV, PPOperators.SET);

        // LOWEST: never more saturated than bloodloss wants, and never raises saturation
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION, true, saturation, L_SAT, PPOperators.LOWEST);
        SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR, {tint[0], tint[1], tint[2], 0.0}, L_TINT, PPOperators.MULTIPLICATIVE);
        SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYFACTOR, true, glare, L_OVERLAY_F, PPOperators.HIGHEST);
    }
}
```

- Drive it from our own client tick (e.g. `MissionGameplay.OnUpdate` or a 4_World handler); setters re-queue the requester automatically, so no `OnUpdate` override is needed. Interpolate (`adaptation`, glare fade) ourselves; PPE doesn't fade.
- Call `Start()` once when night begins, `Stop()` at day (releases everything → rule 1: day untouched). Re-`Start()` after death/respawn because vanilla `StopAllEffects` stops gameplay requesters.
- Gate on `g_Game.GetWorld().IsNight()` / sun angle, and treat `evBoost` as the primary brightness lever (1-3 EV per CLAUDE.md §3.5). EV is additive to the server's lighting, so worst case (boost 0) equals vanilla (rule 2) and the server lighting config stays untouched (rule 3).
- Desaturation via Glow `SATURATION` with `LOWEST`, so it composes with blood loss instead of overriding it. Because colorization is lerped by saturation, the blue shift will automatically fade as saturation drops; tune tint and saturation together.
- Glare: `OVERLAYFACTOR` (white, borrowed from flashbang's colour or our own `OVERLAYCOLOR` layer), optionally a short small EV bump (only when not yielding), optionally `Contrast` (unused by vanilla, safe) for "loss of detail". Avoid Gauss blur (players hate it; many vanilla users).
- Brightness fallback: if EV turns out to be re-normalised by auto-exposure (Q1 negative), Glow `BRIGHTNESS` (0..2) and `OFFSET` are unused by vanilla and act after tonemapping. Less physically correct (lifts blacks / flattens), but guaranteed to work. `EYEACCOM > 1` is a second candidate (compose with `SET` at a layer below underground 100, e.g. 50, so underground/NVG/burlap override us cleanly).
- Film grain needs `NVLIGHTPARAMS.PARAM_NOISE_MULT > 0`; that also feeds NV noise. Use `HIGHEST` on a unique layer if we add grain later.

### 9.2 Asymmetric engine adaptation speed (Lad's idea)

Possible without hacks: the engine already has `SpeedDarkToBright` and `SpeedBrightToDark`. But **don't route it through stock PPE**: registered max is 1 (vanilla emat uses 6) and release writes 0.3. Options:
1. `modded class PPEGlow` → override `RegisterMaterialParameters` (call super, then re-register params 1-3 with emat-matching defaults and wider ranges, e.g. `[0.01..50]`). Risk: hardcoded defaults would overwrite a lighting mod's glow.emat values (Lad's mod edits these).
2. Better: set them directly with `SetParam` on the PPE manager's glow material only while active (see §5.1). **Don't restore with `ResetParam`**, it appears to block later `SetParam` calls (§5.1); write the original values back instead. PPE won't fight us because nothing in vanilla requests these params.

### 9.3 Open questions from CLAUDE.md §8

| Q | Status |
|---|---|
| Q1 Can a script EV boost brighten a dark config, or is it capped by EVMin/EVMax? | **Answered (in-game, 2026-10-08): not capped.** Tested on `lightingConfig 1` (confirmed via `CfgGameplayHandler.GetLightingConfig()` on the client), moon 0, overcast 0.59: +0..+5 EV all apply, `sceneHDRMul` doubles per step, `GetCurrentEVValue()` follows one PPE update later. Sweet spot for full adaptation: +4..+5 EV. Note: with `enableCfgGameplayFile` unset, `lightingConfig` comes from server.cfg, not cfggameplay.json (`cfggameplaydatajson.c:173`). |
| Q2 Are glow.emat params accessible at runtime? | **Answered (script side)**: yes, `PPEGlow` exposes VisiblePercent, TargetBrightness, SpeedDarkToBright, SpeedBrightToDark, bloom, filmic params; `Material.SetParam/ResetParam` also work by name. Runtime visual effect and speed units still to test. |
| Q3 Does vanilla eye accommodation fight our exposure? | **Open.** Unknown whether auto-exposure measures before or after EVUser. Also: vanilla requesters will "fight" via PPE layering (NVG, flashbang ADD on EV → +100 EV bug), solved by yielding our EV layer while those run. |

Bonus for later phases: `World.GetMoonIntensity()`, `GetSunOrMoon()`, `IsNight()` exist natively (Q7 partly answered), and `GetSceneHDRMul(0)` may give a cheap measured-brightness signal.
