# Mission lifecycle, connect/disconnect, RPC, JSON & config.cpp: verified notes

Source: extracted vanilla scripts at `P:\scripts` (paths below are relative to it) and configs at `P:\DZ`. **[V]** = read in source. **[U]** = unverified: community knowledge or inference, test in game.
Scope: what EyeAdapt needs for phase 0 (toolchain) and phase 5 (server JSON config, RPC sync, logout/login persistence). Lights are in `lights.md`, PPE in `ppe.md`.

## 0. Overview / TL;DR

- **Mission objects.** Client runs `MissionGameplay`, server runs `MissionServer`. Both derive from `MissionBase : MissionBaseWorld : Mission`. Base hooks: `OnInit`, `OnMissionStart`, `OnUpdate(float timeslice)`, `OnMissionFinish`, `OnEvent`. **There is no `OnMissionLoaded`** in vanilla.
- **Server connect/disconnect** all go through `MissionServer.OnEvent` (`5_mission/mission/missionserver.c:299-420`). Each event calls an overridable method: `OnClientPrepareEvent`, `OnClientNewEvent`, `OnClientReadyEvent`, `OnClientRespawnEvent`, `OnClientReconnectEvent`, `OnClientDisconnectedEvent`. The *final* disconnect is `PlayerDisconnected(player, identity, uid)`. It runs after the logout countdown, and by then `identity` can already be deleted.
- **Identity.** `GetId()` = *hashed* Steam ID (for DB/logs). `GetPlainId()` = plaintext ID (Steam64 on PC). The vanilla comment says the plaintext ID "cannot be used in database or logs".
- **Config sync template.** Vanilla syncs `cfggameplay.json` with `RPCSingleParam(null, ERPCs.RPC_CFG_GAMEPLAY_SYNC, new Param1<CfgGameplayJson>(m_Data), true, identity)` in `ClientPrepareEvent`, which runs before the player entity exists. The client receives it in `DayZGame.OnRPC` (3_Game). The whole class is deep-serialized.
- **RPC.** `g_Game.RPCSingleParam(target, id, param, guaranteed, recipient)` or `ScriptRPC`. When `target == null`, the RPC lands in `DayZGame.OnRPC`, which also fires a static `DayZGame.Event_OnRPC` ScriptInvoker for every RPC. `ERPCs` is an auto-numbered enum (0..`RPC_END`, about 200), so mods pick their own large constant IDs.
- **JSON.** Use `JsonFileLoader<T>.LoadFile(path, data, err)` / `SaveFile(path, data, err)`. The `JsonLoadFile` / `JsonSaveFile` variants are deprecated. The file API is `FileExist`, `MakeDirectory`, `DeleteFile` and `$profile:`.
- **No unix timestamp in vanilla.** Only `GetYearMonthDayUTC` and `GetHourMinuteSecondUTC` exist, so a unix-seconds helper has to be computed by hand (§5).
- **Starter skeleton** works for milestone 0. Only minor issues (§9).

---

## 1. Mission lifecycle

### 1.1 Base class `Mission` [V] `3_game/gameplay.c:685-849`
```c
void OnInit()  {}   void OnMissionStart() {}   void OnMissionFinish() {}
void OnUpdate(float timeslice)            // timeslice = seconds since last frame
void OnEvent(EventType eventTypeId, Param params) {}
void OnGameplayDataHandlerLoad() {}       // called after cfggameplay.json loaded (cfggameplayhandler.c:83)
bool IsServer()  // false here, MissionServer overrides -> true (missionserver.c:228)
bool IsMissionGameplay() // MissionGameplay -> true (missiongameplay.c:1314)
void OnPlayerRespawned(Man player);       // client, see 1.4
bool IsPlayerDisconnecting(Man player);   // server: true during logout countdown (missionserver.c:233)
void StartLogoutMenu(int time) {}         // client: server told us logout started (see 2.5)
```
Hierarchy [V]: `MissionBase extends MissionBaseWorld` (`5_mission/mission/missionbase.c:1`), `MissionBaseWorld extends Mission` (`4_world/classes/missionbaseworld.c:1`), `MissionServer extends MissionBase` (`missionserver.c:5`), and `MissionGameplay` (client) also extends MissionBase.

**Order** [U, engine-driven, inferred]: constructor → `OnInit` → `OnMissionStart` → `OnUpdate` every frame → `OnMissionFinish` → destructor. `MissionGameplay.OnMissionStart` sets `DayZGame.MISSION_STATE_GAME` (`missiongameplay.c:195`), and `OnMissionFinish` sets `MISSION_STATE_FINNISH` and calls `PPEManagerStatic.GetPPEManager().StopAllEffects(PPERequesterCategory.ALL)` (`:273`). That second call kills our PPE requester too, which is fine.

### 1.2 Client vs server [V]
- `MissionServer` is the dedicated server's mission. `MissionGameplay` is the client's. Both have `OnInit`, `OnMissionStart` and `OnUpdate`. Only `MissionGameplay` and `MissionMainMenu` override `OnMissionFinish`.
- **Gotchas in vanilla overrides.** `MissionGameplay.OnMissionStart` (`:186`) and `OnMissionFinish` (`:257`) do **not** call `super`. Our `modded` overrides must still call `super.X()` so that vanilla (and other mods) run.
- `MissionGameplay.OnInit` has an `m_Initialized` guard (`:100`) because it can be called more than once. Make our init idempotent too (`if (!m_EyeAdapt) m_EyeAdapt = new ...`).
- `MissionServer.OnInit` (`:83`) loads `CfgGameplayHandler.LoadData()` etc. This is the natural place to load **our** server config.
- Compile-time defines used by vanilla: `#ifdef SERVER` / `#ifndef SERVER` (e.g. `missionbase.c:31`, `cfggameplaydatajson.c` `ITEM_DataBase` ctor) and `#ifndef NO_GUI`. Runtime checks: `g_Game.IsServer()`, `IsClient()`, `IsMultiplayer()`. `IsDedicatedServer()` is described as "Robust check which is preferred ... valid much sooner. You may want to use #ifdef SERVER instead" (`3_game/global/game.c:1120-1127`).
- `GetGame()` returns `DayZGame` (`3_game/gameplay.c:636`), and the global `g_Game` is the same object (`dayzgame.c:3971`). Vanilla now mostly writes `g_Game`.

### 1.3 Getting the local player on the client [V]
- `proto native DayZPlayer GetPlayer()` (`3_game/global/game.c:946`). On the client this is the controlled player and is **null during loading**. Vanilla always null-checks it, e.g. `MissionGameplay.TickScheduler` (`:218-221`) and `DayZGame.OnRPC` (`if (!g_Game.GetPlayer()) break;`, `dayzgame.c:3204`).
- `Man.IsControlledPlayer()` → `g_Game.GetPlayer() == this` (`4_world/entities/manbase.c:10`). This is the standard "is this the local player" test inside PlayerBase code.
- The server makes a character "the player" with `g_Game.SelectPlayer(identity, player)`, called in `CreateCharacter` (`missionserver.c:492`) and `OnClientReadyEvent` (`:588`). The client's `GetPlayer()` becomes non-null some time after that [U exact timing].

### 1.4 Respawn / death on the client [V]
- Every `PlayerBase` constructor queues `OnPlayerLoaded` (`playerbase.c:597`). Inside it, `if (IsControlledPlayer())` → `mission.SetPlayerRespawning(false); mission.OnPlayerRespawned(this);` (`playerbase.c:2419-2430`). So `MissionGameplay.OnPlayerRespawned(Man)` (`missiongameplay.c:1613`) is the client hook for "a new local character exists" [U whether it also fires on the very first spawn; the code path suggests yes if `GetPlayer()` is already set by then].
- Robust approach (no reliance on hook timing): in `OnUpdate`, cache the `PlayerBase` you last saw. When `GetPlayer()` returns a different object, treat it as a new character. When it's null or `!IsAlive()`, pause adaptation.

---

## 2. Connect / disconnect on the server

### 2.1 Event dispatch [V] `missionserver.c:299-420`
| Engine event (`3_game/gameplay.c:411-435` param typedefs) | What MissionServer does |
|---|---|
| `ClientPrepareEventTypeID` `Param5<PlayerIdentity,bool useDB,vector pos,float yaw,int preloadTimeout>` | **Syncs configs**: `CfgGameplayHandler.SyncDataSendEx(identity)`, `UndergroundAreaLoader.SyncDataSend`, `CfgPlayerRestrictedAreaHandler.SyncDataSend`; then `OnClientPrepareEvent(...)`. There is no player entity yet, only the identity. |
| `ClientNewEventTypeID` `Param3<PlayerIdentity,vector,Serializer>` | `player = OnClientNewEvent(identity,pos,ctx)` creates a **fresh character** (new spawn or after death); then `InvokeOnConnect(player, identity)`, `SyncEvents.SendPlayerList()`, `ControlPersonalLight`, `SyncGlobalLighting` |
| `ClientReadyEventTypeID` `Param2<PlayerIdentity,Man>` | **existing character loaded from DB**: `OnClientReadyEvent(identity, player)` (does `SelectPlayer`), then `InvokeOnConnect`, player list, lights |
| `ClientRespawnEventTypeID` `Param2<PlayerIdentity,Man>` | `OnClientRespawnEvent`: kills the old char if unconscious or restrained. [U] a `ClientNewEvent` for the new char follows. |
| `ClientReconnectEventTypeID` `Param2<PlayerIdentity,Man>` | `OnClientReconnectEvent` → `player.OnReconnect()`. Reconnect to a character still in the world. **Does not call `InvokeOnConnect`.** |
| `ClientDisconnectedEventTypeID` `Param4<PlayerIdentity,Man,int logoutTime,bool authFailed>` | `OnClientDisconnectedEvent(...)`, which **starts** the logout. |
| `LogoutCancelEventTypeID` `Param1<Man>` | removes player from `m_LogoutPlayers`/`m_NewLogoutPlayers` |

`InvokeOnConnect(PlayerBase player, PlayerIdentity identity)` (`:422`) is called for **both** New and Ready and calls `player.OnConnect()`. This makes it a good single "player is in and has an identity" hook. `InvokeOnDisconnect(player)` (`:429`) is called only from `PlayerDisconnected`.

### 2.2 Logout countdown [V] `missionserver.c:628-711`
`OnClientDisconnectedEvent`: if a hive exists (`GetHive()`), auth didn't fail and the player is alive, the server sends `SendLogoutTime(player, logoutTime)`. It then stores `LogoutInfo(g_Game.GetTime() + logoutTime*1000, identity.GetId())` in `m_NewLogoutPlayers` → `m_LogoutPlayers`, and **the body stays in the world**. `UpdateLogoutPlayers()` (`:267`, called each `OnUpdate`) calls `PlayerDisconnected(player, identity, info.param2)` once the timer expires. Otherwise (no hive, dead, auth failed) it calls `PlayerDisconnected` immediately.

`PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)` (`:677`):
```c
// Note: At this point, identity can be already deleted
if (!player) { Print("[Logout]: Skipping player " + uid + ", already removed"); return; }
InvokeOnDisconnect(player);
if (GetHive()) { player.Save(); GetHive().CharacterExit(player); }
player.ReleaseNetworkControls();
HandleBody(player);          // alive: kill if unconscious/restrained (except server-exit kicks), else player.Delete()
g_Game.DisconnectPlayer(identity, uid);
```
- `uid` is `identity.GetId()` captured at logout start, so it is the **hashed** ID.
- `PlayerBase.GetCachedID()` / `GetCachedName()` (`playerbase.c:9667-9675`) are filled in `OnSelectPlayer` on the server (`:6037-6040`, `identity.GetId()`/`GetName()`). They are usable when `identity` is gone.

### 2.3 Which hook to use [V reasoning from the code above]
- **Restore (send the saved value to the client):** override `InvokeOnConnect(player, identity)` (covers New + Ready), **plus** `OnClientReconnectEvent` (reconnect skips InvokeOnConnect). Distinguish fresh characters by overriding `OnClientNewEvent`. It returns the new `PlayerBase`, and for a fresh character the record should be reset to "full vision" (design §3.6).
- **Save:** override `PlayerDisconnected(player, identity, uid)`. It is the definitive "gone" point, after the countdown and after any logout cancel. Use the `uid` param or a value cached at connect, **never `identity.X()` without a null check**. Optionally also snapshot at `OnClientDisconnectedEvent` (identity is valid there). The client keeps reporting during the countdown [U whether a client that alt-F4s still reports; it won't, so the last cached value is what gets saved].
- **Server shutdown:** `MissionServer` has no `OnMissionFinish` override, but `Mission.OnMissionFinish` exists. Override it to flush all in-memory records [U whether `PlayerDisconnected` fires for everyone at shutdown; `ShouldPlayerBeKilled` lists `EClientKicked.SERVER_SHUTDOWN`, which suggests a disconnect path exists].
- Death: `PlayerDisconnected` with a dead player is still called (instant path). The record should not matter, because the next character comes through `OnClientNewEvent` → reset.

### 2.4 Identity [V] `3_game/gameplay.c:338-391`
```c
proto string GetName();       // nick
proto string GetPlainName();
proto string GetFullName();
proto string GetId();         // "unique id of player (hashed steamID, database Xbox id...) can be used in database or logs"
proto string GetPlainId();    // "plaintext unique id of player (cannot be used in database or logs)"
proto int    GetPlayerId();   // session id, reused after disconnect
proto Man    GetPlayer();
```
`class PlayerIdentity : PlayerIdentityBase` is moddable. Vanilla uses `GetPlainId()` for player list sync and mute lists (`3_game/client/syncplayerlist.c:18`, `dayzgame.c:3305`). **Steam64 = `GetPlainId()`** on PC [U that it is literally the 17-digit Steam64 string; community-known].

### 2.5 Client-side logout signal [V]
`MissionGameplay.StartLogoutMenu(int time)` (`missiongameplay.c:1359`) runs on the client when the server calls `g_Game.SendLogoutTime(player, t)` (`missionserver.c:646/671`, `game.c:392`). It's a good moment for the client to send one final adaptation report.

---

## 3. RPC

### 3.1 API [V] `3_game/global/game.c:971-1018`, `3_game/gameplay.c:15-136`
```c
proto native void RPC(Object target, int rpcType, notnull array<ref Param> params, bool guaranteed, PlayerIdentity recipient = null);
proto native void RPCSingleParam(Object target, int rpc_type, Param param, bool guaranteed, PlayerIdentity recipient = null);
// doc: "When called on client, RPC is evaluated on server; When called on server, RPC is executed on all clients"
// "target ... when NULL, RPC is evaluated by CGame as global"
// "recipient ... If NULL, RPC will be send to all clients (specifying recipient increase security and decrease network traffic)"

class ScriptRPC: ParamsWriteContext { void Reset(); void Send(Object target, int rpc_type, bool guaranteed, PlayerIdentity recipient = NULL); }
typedef Serializer ParamsReadContext;   typedef Serializer ParamsWriteContext;
```
`Serializer` (`1_core/proto/serializer.c`) has `Write(void)` / `Read(void)` for int, float, string, bool, vector, arrays, set, map, static arrays and **classes (deep)**. `[NonSerialized()]` excludes a field. `Param1..Param10<...>` (`1_core/param.c`) are just typed holders whose `Serialize` writes `param1, param2, ...` in order. Reading `ctx.Read(Param1<float>)` and reading the raw `float` are equivalent [U byte-identical, but vanilla mixes both styles; always read with the same shape you wrote].

### 3.2 Where RPCs arrive [V]
- **Global (target == null):** `DayZGame.OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)` (`3_game/dayzgame.c:3104`). It first calls `Event_OnRPC.Invoke(sender, target, rpc_type, ctx)`, where `static ref ScriptInvoker Event_OnRPC` is at `dayzgame.c:987`. Then: `if (target) target.OnRPC(sender, rpc_type, ctx); else switch (rpc_type) {...}`. The switch ignores unknown IDs, so a custom ID is harmless to vanilla.
- **Object target:** `Object.OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)` (`3_game/entities/object.c:856`), overridden in `EntityAI` and `PlayerBase.OnRPC` (`playerbase.c:5452`, which calls `super` first).
- Vanilla subscriber example: `SyncEvents.RegisterEvents()` → `DayZGame.Cast(g_Game).Event_OnRPC.Insert(Event_OnRPC)` (`3_game/syncevents.c:3-21`), called from the `MissionGameplay` constructor (`missiongameplay.c:76`). The handler filters on `rpc_type` first, which is **mandatory**, because `Event_OnRPC` fires for every RPC and reading someone else's ctx corrupts it.

### 3.3 Direction examples in vanilla [V]
- **Server → one client, no object:** `CfgGameplayHandler.SyncDataSendEx(identity)`: `g_Game.RPCSingleParam(null, ERPCs.RPC_CFG_GAMEPLAY_SYNC, new Param1<CfgGameplayJson>(m_Data), true, identity);` (`cfggameplayhandler.c:94-98`). The client handles it in `DayZGame.OnRPC` under `#ifndef SERVER` → `CfgGameplayHandler.OnRPC` (`dayzgame.c:3122-3126`, `cfggameplayhandler.c:102-114`):
  ```c
  Param1<CfgGameplayJson> data = new Param1<CfgGameplayJson>(null);
  if (ctx.Read(data)) m_Data = data.param1; else ErrorEx("... client failed to read incoming data");
  ```
- **Server → one client, on player object:** `g_Game.RPCSingleParam(player, ERPCs.RPC_SEND_LIGHTING_SETUP, new Param1<int>(id), true, player.GetIdentity());` (`missionserver.c:468-475`).
- **Client → server, no object:** `MissionGameplay.SendMuteListToServer`: `ScriptRPC rpc = new ScriptRPC(); rpc.Write(mute_list); rpc.Send(null, ERPCs.RPC_USER_SYNC_PERMISSIONS, true, null);` (`missiongameplay.c:239-247`). The server reads it in `DayZGame.OnRPC` (outside the `#ifndef SERVER` block) and uses `sender.GetPlainId()` (`dayzgame.c:3296-3309`).

### 3.4 Sender verification
- `sender` is supplied by the engine, not by the payload [U formally, but that's the design: vanilla trusts it in the mute-list handler]. On the server, map it to the player with `sender.GetPlayer()` (`gameplay.c:375`) and key records by `sender.GetId()` / `GetPlainId()`. **Never** trust an ID written into the payload.
- Vanilla has no explicit `sender == player.GetIdentity()` check anywhere (grep found none). If you target the player object instead of null, add `if (sender != GetIdentity()) return;` yourself.
- Validate payload: clamp the float to 0..1, reject NaN, rate-limit (ignore reports more frequent than ~1/s per sender).

### 3.5 RPC IDs [V enum / U convention]
`enum ERPCs` (`3_game/enums/erpcs.c`) starts with `RPC_SYNC_ITEM_VAR = 0`, auto-increments, has `#ifdef DEVELOPER/DIAG` blocks (so values shift between builds), and ends with `RPC_END, // DO NOT USE OR APPEND AFTER`. No other RPC enum exists in vanilla. **Don't** append to `ERPCs` (modded enums are possible but numbering would shift). Use your own constants well above the vanilla range, e.g.
```c
class EyeAdaptRPC   // 3_Game
{
    // decimal literals (hex support in Enforce not checked here)
    static const int SYNC_CONFIG    = 714200101;  // server -> client: settings + restored adaptation
    static const int REPORT_ADAPT   = 714200102;  // client -> server: current adaptation
}
```
Collisions with other mods are possible only by coincidence. Pick a project-specific random 9-digit base [U community convention].

---

## 4. JSON & files

### 4.1 `JsonFileLoader<T>` [V] `3_game/tools/jsonfileloader.c`
```c
static bool LoadFile(string filename, out T data, out string errorMessage);   // FileExist + ReadFile + JsonSerializer.ReadFromString
static bool SaveFile(string filename, T data, out string errorMessage);       // pretty-printed WriteToString + FPrint
static bool LoadData(string json, out T data, out string err);  static bool MakeData(T in, out string json, out string err, bool pretty = true);
// DEPRECATED: JsonLoadFile / JsonSaveFile / JsonLoadData / JsonMakeData (no error reporting)
```
**Pattern used by vanilla** (`5_mission/gui/cameratools/cameratoolsmenu.c:208-220`, `cfggameplayhandler.c:44-78`): create the object first (field initializers = defaults), then `LoadFile` into it, and on failure write the defaults back:
```c
if (!FileExist(dir)) MakeDirectory(dir);           // vanilla checks dir with FileExist
CTSaveStructure saveData = new CTSaveStructure();
if (!JsonFileLoader<ref CTSaveStructure>.LoadFile(path, saveData, errorMessage)) SaveData();
```
[U] keys missing from the JSON keep the pre-set default values. That is the behaviour vanilla's "defaults then load" pattern relies on, but verify with a partial file.

### 4.2 Serializable field types [V] `3_game/gameplay.c:48-104` (JsonSerializer docs), `serializer.c`
int, float, bool, string, vector (as `[x,y,z]`), static arrays, `array<T>`, `set<T>`, `map<K,V>` (key only int, enum or string), nested classes (`ref MyClass x = new MyClass;`). `[NonSerialized()]` to skip. JSON keys = field names exactly (vanilla `CfgGameplayJson` uses `int version = -1;` plus public fields with no `m_` prefix). [U] protected/private fields: keep config fields public.

### 4.3 File API [V] `1_core/proto/ensystem.c:382-531`
`FileExist(name)`, `OpenFile(name, FileMode.READ|WRITE|APPEND)`, `CloseFile`, `FPrint/FPrintln`, `FGets`, `ReadFile`, `FindFile/FindNextFile/CloseFindFile`, `MakeDirectory(name)`, `DeleteFile(name)` ("Works only on `$profile:` and `$saves:`"), `CopyFile(src, dst)` (dest must be `$profile:`/`$saves:`).
Prefixes documented: `$profile`, `$saves`, `$mission` (e.g. `"$mission:cfggameplay.json"`). Vanilla writes `"$profile:/CameraTools"` (`cameratoolsmenu.c:66`). On the server, `$profile:` = the `-profiles=` folder [U, standard].

---

## 5. Real-world time [V functions / U helper]
Vanilla has **no** unix-time function. Available (`1_core/proto/ensystem.c:28-106`): `GetYearMonthDay`, `GetHourMinuteSecond` (local), `GetYearMonthDayUTC`, `GetHourMinuteSecondUTC`, `TickCount(prev)` (CPU ticks, profiling only). `g_Game.GetTime()` = mission ms (resets at restart), and `GetTickTime()` = seconds since game start. Neither is usable for persistence.

Helper (days-from-civil, UTC, fits in 32-bit int until 2038) [U compile, the algorithm is standard]:
```c
static int EyeAdapt_UnixNow()
{
    int y, mo, d, h, mi, s;
    GetYearMonthDayUTC(y, mo, d);  GetHourMinuteSecondUTC(h, mi, s);
    if (mo <= 2) y -= 1;
    int era = y / 400;  if (y < 0 && y % 400 != 0) era -= 1;
    int yoe = y - era * 400;
    int mp = (mo + 9) % 12;                       // Mar=0 .. Feb=11
    int doy = (153 * mp + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int days = era * 146097 + doe - 719468;
    return days * 86400 + h * 3600 + mi * 60 + s;
}
```
Community Framework has date helpers, but it's an extra dependency. Not needed.

---

## 6. config.cpp / script modules

### 6.1 What vanilla shows [V]
- `P:\scripts\config.cpp`: `class CfgPatches { class DZ_Scripts { requiredAddons[] = {}; ... } }` with no CfgMods. The core script modules are defined by the engine.
- `P:\DZ\data\config.cpp`: `CfgPatches.DZ_Data`; `CfgMods.DayZ` has no script defs.
- **Vanilla DLC as a mod template**, `P:\DZ\data_sakhal\config.cpp`:
  ```cpp
  class CfgPatches { class DZ_Data_Sakhal { units[]={}; weapons[]={}; requiredVersion=0.1; requiredAddons[]={"DZ_Data"}; }; };
  class CfgMods { class sakhal { type="mod"; dir="sakhal"; name="...";
      class defs { class worldScriptModule { value=""; files[]={"DZ/data_sakhal/scripts/4_World"}; }; }; }; };
  ```
  `files[]` paths are **PBO-prefix-relative** (`DZ/data_sakhal` is that PBO's prefix). Sakhal has **no `dependencies[]`**.
- Module class names: `gameScriptModule` (3_Game), `worldScriptModule` (4_World), `missionScriptModule` (5_Mission) [V for world from Sakhal; others U but standard].
- `dependencies[] = {"Game","World","Mission"}` is a community convention, not seen in vanilla [U]. Harmless and widely used.

### 6.2 Layer visibility (from CLAUDE.md §5.1, consistent with source)
Each layer only sees earlier ones. Consequences for EyeAdapt:
- **3_Game:** settings class (JSON-serializable), RPC ID constants, unix-time helper, the client-side receive of config (via `modded class DayZGame { override void OnRPC(...) }` or an `Event_OnRPC` subscriber), and the server-side record store (needs only `PlayerIdentity` + file IO, both visible here).
- **4_World:** anything touching `PlayerBase`, `ScriptedLightBase` and the adaptation state/dose (it can read the 3_Game settings).
- **5_Mission:** `modded class MissionServer` (event hooks) and `modded class MissionGameplay` (client tick, reporting, PPE apply).
- A 3_Game handler cannot call 4_World/5_Mission code directly. Pass data through static fields in 3_Game classes, or use a `ScriptInvoker` that later layers subscribe to.

---

## 7. Modded class rules (relevant subset)
- [V] `modded class X { override T M(...) { super.M(...); ... } }`. Vanilla's own example (`4_world/plugins/pluginbase/plugindiagmenu/plugindiagmenumodding.c:21-40,116-130`, live code) **adds new member fields with initializers** and overrides `protected` methods with `override protected void BindCallbacks() { super.BindCallbacks(); ... }`.
- [V] Commented examples show `modded class ParticleList { static const int MODDED_PARTICLE = RegisterParticle(...); }` (`3_game/particles/particlelist.c:4-8`), so static members in modded classes are intended.
- [U] Modded constructors/destructors chain with the vanilla ones (see `lights.md` §9). Prefer overriding `OnInit` etc. over constructors.
- Overriding a non-virtual-looking method is fine. All script methods are overridable with `override`, e.g. `MissionServer.InvokeOnConnect`, `PlayerDisconnected`, `OnClientNewEvent` (returns `PlayerBase`, so the override must return `super`'s result).
- Prefix every added member and method (`m_EyeAdapt...`, `EyeAdapt_...`) to avoid collisions with other mods modding the same class.

---

## 8. Logging [V] `1_core/proto/endebug.c`, `3_game/tools/debug.c`
| Function | Behaviour |
|---|---|
| `Print(void var)` | "Prints content of variable to console/log". Use `Print("[EyeAdapt] " + x)` |
| `PrintFormat(fmt, p1..p9)` | `%1..%9`. **Strings get quoted**: example output `Hello 'Peter'` |
| `string.Format(fmt, ...)` | `1_core/proto/enstring.c:526`. Use `Print(string.Format(...))` for clean output |
| `ErrorEx(err, ErrorExSeverity.INFO/WARNING/ERROR)` | prefixed `[Class::Method] :: [SEVERITY] ::`; "above 'INFO' will show a messagebox" (in diag builds) |
| `PrintToRPT(var)` | to the RPT file; "each write means fflush! use with care" |
| `Debug.Log(msg, plugin, ...)` | only when `Debug.m_DoLogs`, which is `IsCLIParam("doLogs")` unless built with `ENABLE_LOGGING` (`debug.c:707-712`) |
Own debug toggle: a `DebugLogging` bool in the server JSON (synced), and/or `IsCLIParam("eyeAdaptDebug")` (`1_core/proto/ensystem.c:141`). Output locations: client `%LocalAppData%\DayZ\script_*.log`, server in the `-profiles` folder (per CLAUDE.md) [U here].

---

## 9. Starter skeleton review (`EyeAdapt_starter\`)

The skeleton compiles in principle and is fine for milestone 0. Concrete points:

1. **config.cpp** is correct in shape. `requiredAddons {"DZ_Data","DZ_Scripts"}` both exist [V]. `type="mod"` and `class defs { gameScriptModule/worldScriptModule/missionScriptModule }` match the Sakhal pattern [V]. `dependencies[]` is a convention, not vanilla, but harmless. **Important:** `files[] = {"EyeAdapt/scripts/3_Game"}` only resolves if the **PBO prefix is `EyeAdapt`**. In Addon Builder, pack the folder `P:\EyeAdapt` (prefix defaults to the folder name) and check that the prefix isn't something like `EyeAdapt_starter\EyeAdapt`.
2. **`OnUpdate` ticks with no player.** `MissionGameplay.OnUpdate` runs during the loading and dead screens, so the ramp advances before you spawn. Gate on `PlayerBase p = PlayerBase.Cast(g_Game.GetPlayer()); if (!p || !p.IsAlive()) return;`, and reset on character change (§1.4).
3. **`FULL_ADAPT_SECONDS = 1800`** contradicts the brief's default (fixed real time, 300–600 s). Also, `static const` values in 3_Game are hard-coded; they'll become fields of a JSON settings class with defaults (§10).
4. **OnInit idempotency.** Vanilla guards `OnInit` with `m_Initialized`. Use `if (!m_EyeAdapt) m_EyeAdapt = new EyeAdaptState();` so a second call doesn't reset state.
5. **No `OnMissionFinish` cleanup.** Harmless now; once a PPE requester exists, vanilla's `StopAllEffects(ALL)` covers it, but null your refs.
6. **Logging:** `Print("..." + float.ToString())` is fine. Consider `string.Format` and a debug toggle so release builds aren't spammy every 5 s.
7. **start_local.bat:**
   - Server `serverDZ.cfg` needs `verifySignatures = 0` (unsigned PBO), otherwise the client gets kicked. `-profiles=profiles` → server `$profile:` = `<DayZServer>\profiles\`, so the JSON files will land in `profiles\EyeAdapt\`.
   - `-mod=` on **both** server and client is correct (the mod has client and server code). Don't move it to `-serverMod`, which is server-only and would leave the client without the mod.
   - Fixed `timeout /t 20` can be too short on first start (the client then fails to connect). Just retry, or raise it to 30–40.
   - A local server without a hive (`GetHive()` null) uses the **instant logout** path (`missionserver.c:666-674`), and `ClientReadyEvent` vs `ClientNewEvent` behaviour depends on character persistence. Use the normal mission with `init.c` that creates the hive [U standard] to test logout timers realistically.
   - Paths with spaces inside `set DAYZ=...` are fine because they are quoted at use.

---

## 10. Implications for EyeAdapt (recommended architecture)

> **Changed 2026-10-23:** one save file `$profile:EyeAdapt/adaptation.json` (all players, offline seconds instead of a timestamp, saved every 60 s and in `OnMissionFinish`), `LogoutMode` keep/fixed/sleep. See `EyeAdaptPersistence.c`.
>
> **Changed 2026-10-11:** persistence is now memory only (no JSON files, no Unix time, no periodic save). The server keeps a record per `GetId()` with a `g_Game.GetTime()` timestamp. A restart forgets everyone, who then start at `StartAdaptation` (default 1.0 = "slept"). The file-based plan below is kept for reference.

### 10.1 Files / layers
```
scripts/3_Game/EyeAdapt/
  EyeAdaptSettings.c   // class EyeAdaptSettings { int Version = 1; bool Enabled = true; float FullAdaptationSeconds = 420; ... } + static instance + Load()/Save()
  EyeAdaptRPC.c        // const IDs, EyeAdapt_UnixNow()
  EyeAdaptRecord.c     // class EyeAdaptRecord { float Adaptation; int SavedAtUnix; string Name; }  (server persistence DTO)
  EyeAdaptDayZGame.c   // modded class DayZGame: client receive of SYNC_CONFIG -> stores static settings + pending restored adaptation
scripts/4_World/EyeAdapt/  EyeAdaptState.c, light registry, dose
scripts/5_Mission/EyeAdapt/
  MissionServer.c      // load config, connect/disconnect hooks, REPORT handler, record store, flush
  MissionGameplay.c    // client tick, periodic report, apply PPE
```

### 10.2 Config sync
- Server: in `modded MissionServer.OnInit()` (after `super`) do `EyeAdaptSettings.LoadServer()`, then `MakeDirectory("$profile:EyeAdapt")`. Next, `JsonFileLoader<EyeAdaptSettings>.LoadFile("$profile:EyeAdapt/config.json", s, err)`, and if that fails `SaveFile` the defaults (same as vanilla cameratools). Validate/clamp after loading. Bump `Version` to add new keys and rewrite the file.
- Send to the client together with the restored adaptation in **one** RPC from `InvokeOnConnect` and `OnClientReconnectEvent`: `g_Game.RPCSingleParam(null, EyeAdaptRPC.SYNC_CONFIG, new Param2<EyeAdaptSettings, float>(settings, restored), true, identity)`. Target `null` + recipient means it is handled by `DayZGame.OnRPC` on the client, which exists regardless of mission/player state (exactly like `RPC_CFG_GAMEPLAY_SYNC`). Optionally also send the settings alone at `ClientPrepareEvent` (override `OnClientPrepareEvent`) to have them as early as vanilla does.
- Client: `modded class DayZGame { override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx) { if (!target && rpc_type == EyeAdaptRPC.SYNC_CONFIG) { read...; return; } super.OnRPC(sender, target, rpc_type, ctx); } }`. Note that returning early skips `Event_OnRPC.Invoke`, which is fine for our own ID. Alternatively, subscribe to `DayZGame.Event_OnRPC` and filter on ID. Until the RPC arrives the client uses defaults, and `Enabled` should default to *off on the client* until synced, so a mod-less server config can't surprise anyone.

### 10.3 Client reporting
- In `MissionGameplay.OnUpdate`, every `ReportIntervalSeconds` (e.g. 15–30 s), and also once in an `override StartLogoutMenu(int time)`: `ScriptRPC rpc = new ScriptRPC(); rpc.Write(adaptation); rpc.Send(null, EyeAdaptRPC.REPORT_ADAPT, true, null);` (mirrors `SendMuteListToServer`). Only send when a live local player exists.

### 10.4 Server store, save and restore
- Keep `ref map<string, ref EyeAdaptRecord> m_EyeAdaptRecords` keyed by `identity.GetId()` (hashed, the same key `PlayerDisconnected`'s `uid` gives you). Cache `GetPlainId()` in the record at connect if filenames use the Steam64.
- **REPORT handler** (server, `Event_OnRPC` subscriber registered in `modded MissionServer` ctor/OnInit, or `modded DayZGame.OnRPC` under `#ifdef SERVER`): `if (rpc_type != REPORT_ADAPT || !sender) return;` then read the float, clamp to 0..1, rate-limit, and update the in-memory record with `SavedAtUnix = EyeAdapt_UnixNow()`.
- **Restore** in `override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)` (after `super`): load `$profile:EyeAdapt/<id>.json` if it isn't in memory, then compute `restored = Clamp(rec.Adaptation + offlineSeconds * darkRate, 0, 1)`. If no file exists, or `offlineSeconds > LoginFullVisionAfterSeconds`, use 1 (full vision). Send SYNC_CONFIG.
- **Fresh character:** `override PlayerBase OnClientNewEvent(...) { PlayerBase p = super.OnClientNewEvent(...); /* mark record id as fresh -> 1.0 */ return p; }`. `InvokeOnConnect` runs right after it (same event case), so set a flag there.
- **Reconnect** during the logout countdown: `override OnClientReconnectEvent` → resend from the in-memory record (no file read).
- **Save** in `override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)`. Do this *before* `super`, because super deletes the body. Use `uid` (not `identity`, which may be null) → `JsonFileLoader<EyeAdaptRecord>.SaveFile("$profile:EyeAdapt/" + fileKey + ".json", rec, err)`, then remove from the map. Also flush everything in `override OnMissionFinish()` and optionally periodically (every few minutes) to survive crashes.
- **Filename key:** the brief wants `<steamID>.json` → `GetPlainId()` (digits only, filename-safe). Vanilla labels it "cannot be used in database or logs" (privacy guidance, not a technical limit). If that matters, use `GetId()`, but it is base64-like and may contain `/` or `+` [U], so sanitize it (replace `/`→`_`, `+`→`-`, strip `=`) before using it as a filename. Either way, capture the chosen key at connect time and store it in the record, because `identity` may be dead at `PlayerDisconnected`.

### 10.5 Unverified, test in game
- Exact client timing of `GetPlayer()` becoming non-null and whether `OnPlayerRespawned` fires on first spawn.
- Whether an RPC sent at `ClientPrepareEvent` reliably arrives before the client's mission is up (vanilla relies on it, so likely yes).
- Event sequence for death → respawn (`ClientRespawnEvent` then `ClientNewEvent`?) and for server shutdown (does `PlayerDisconnected` run for each player?).
- JsonSerializer behaviour with missing keys (defaults kept?) and with non-public fields.
- `GetId()` character set; `GetPlainId()` = Steam64 on PC.
- Modded constructor/destructor chaining (see `lights.md` §9).

## Local test setup: lessons learned (verified 2026-10-07)

- **The client must run with BattlEye.** The local DayZServer kicked every client started as plain `DayZ_x64.exe` with `BattlEye kicked player ... for 'Game restart required'` (client: `Warning (0x000400F0)`), even with `BattlEye = 0` in `serverDZ.cfg`. Joining through the DayZ Launcher (which starts `DayZ_BE.exe` + `BEService`) works. The kick is unrelated to the mod: it happened with a broken build (no scripts loaded), a fixed build, signed/unsigned, and mod loaded from `P:\` or from the game folder.
- **The Addon Builder prefix must be exactly `EyeAdapt`** (a stray `EyeAdapt,` produced a PBO whose `config.cpp` `files[]` paths didn't resolve: mod listed in defines, but no scripts ran). Check with: the PBO header shows `prefix|EyeAdapt|`.
- **Signing:** the key pair lives in `D:\DayZ-Mods\keys\` (outside the repo); `EyeAdapt.bikey` is in `DayZServer\keys\`, `verifySignatures = 2`.
- **Old persistence breaks when the mod set changes:** `storage_1` saved with CF/VPP produced thousands of "Scripted variables corrupted" errors without them. Use a fresh storage per mod set.
- Don't run two Addon Builder instances at once (shared temp folder; a parallel pack failed with "CfgConvert task failed").
