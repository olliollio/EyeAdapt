# Dev setup on a new PC

Assumes DayZ and DayZ Server are installed via Steam. Paths below are the ones used on the first dev PC, adjust them if your drives differ.

## 1. Tools

- **Git**, **VS Code**, **Claude Code**
- **Python 3** (Claude uses it for small edit/log scripts)
- **DayZ Tools** (Steam → Library → Tools)
- Steam Workshop: subscribe to **CF** and **VPPAdminTools** (admin tools for testing, optional)

## 2. Repo

```
git clone https://github.com/olliollio/EyeAdapt.git D:\DayZ-Mods\EyeAdapt
```

## 3. Work drive (P:)

1. Start DayZ Tools → Settings: set the project drive path, e.g. `D:\DayZ-Work`, letter `P`. Enable auto-mount if you like.
2. Mount P:, then run **Extract Game Data**. The vanilla scripts end up in `P:\scripts` (= `D:\DayZ-Work\scripts`). The notes in `docs/engine/` refer to them.
3. Link the mod source into P: (cmd as admin is not needed for junctions):
   ```
   mklink /J D:\DayZ-Work\EyeAdapt D:\DayZ-Mods\EyeAdapt\EyeAdapt_starter\EyeAdapt
   ```

## 4. Signing key (not in the repo)

The key pair lives outside the repo in `D:\DayZ-Mods\keys\` (`EyeAdapt.bikey` + `EyeAdapt.biprivatekey`).

- **Copy it from the first PC** (USB stick, password manager). Never commit the `.biprivatekey`.
- Or create a new pair with DayZ Tools → DS Utils → "Create key" (name `EyeAdapt`). Fine for development, but for releases use one key only.

## 5. Addon Builder

- Source: `P:\EyeAdapt`
- Destination: `P:\@EyeAdapt\Addons`
- **Prefix must be exactly `EyeAdapt`** (otherwise no scripts load)
- Sign with `D:\DayZ-Mods\keys\EyeAdapt.biprivatekey`

## 6. Local server

Copy from `dev/server/` in the repo:

| File | To |
|---|---|
| `serverDZ.cfg` (real time, dark night, VPP-ready) | `<DayZServer>\serverDZ.cfg` |
| `serverDZ_fasttime.cfg` (12x day, 48x night) | `<DayZServer>\serverDZ_fasttime.cfg` |
| `config.json` (EyeAdapt settings, debug keys on) | `<DayZServer>\profiles\EyeAdapt\config.json` (optional, created with defaults otherwise) |

Then:

- **Keys:** copy `EyeAdapt.bikey` into `<DayZServer>\keys\`. With VPP also `VPP.bikey` and `Jacob_Mango_V3.bikey` (CF) from the Workshop mods' `Keys` folders.
- **Fast login:** in `<DayZServer>\mpmissions\dayzOffline.chernarusplus\db\globals.xml` set `TimeLogin` = 1, `TimeLogout` = 1, `TimePenalty` = 0.
- **VPP admin:** after the first server start with VPP, put your Steam64 ID into `<DayZServer>\profiles\VPPAdminTools\Permissions\SuperAdmins\SuperAdmins.txt`.
- **VPP password:** first line of `<DayZServer>\profiles\VPPAdminTools\Permissions\credentials.txt` (not `passwordAdmin`), then restart the server (VPP hashes it on startup).

## 7. Start script

`EyeAdapt_starter\start_local.bat`: `ADMIN=1` loads CF + VPP, `SERVERCFG` switches between real time and fast time. Install paths differ per PC: put them into `EyeAdapt_starter\paths.local.bat` (gitignored, called by the start script), e.g.

```
set DAYZ=D:\Steam\steamapps\common\DayZ
set SERVER=D:\Steam\steamapps\common\DayZServer
set TOOLS=D:\Steam\steamapps\common\DayZ Tools
set KEY=G:\Dayz-Modding\keys\EyeAdapt.biprivatekey
```

The start script re-signs `EyeAdapt.pbo` with `DSSignFile` (from `TOOLS`, key `KEY`) on every start, because Addon Builder doesn't always sign even with the key set.

## PCs in use

| PC | Repo | Work drive (P:) | Keys | Steam |
|---|---|---|---|---|
| first | `D:\DayZ-Mods\EyeAdapt` | `D:\DayZ-Work` | `D:\DayZ-Mods\keys` | `D:\SteamLibrary` |
| second | `G:\Dayz-Modding\EyeAdapt` | `G:\Dayz-Modding\DayZ-Work` | `G:\Dayz-Modding\keys` | `D:\Steam` |

On the second PC the existing `serverDZ.cfg` and chernarus `globals.xml` were backed up as `*.bak-pre-eyeadapt` before the dev versions replaced them.

## 8. Daily loop

1. Mount P: (if not auto)
2. Change code → Addon Builder → `start_local.bat`
3. Tuning only: edit `<DayZServer>\profiles\EyeAdapt\config.json` → `start_local.bat` (no repack)
4. Logs: client `%LocalAppData%\DayZ\script_*.log`, server `<DayZServer>\profiles\script_*.log`

## Notes

- Judge night visuals in **fullscreen**. On the first PC windowed mode lifted the blacks (might differ on yours, compare once).
- The client must run through `DayZ_BE.exe` (BattlEye), plain `DayZ_x64.exe` gets kicked.
- Debug keys (no menu open): Numpad 1 / 3 = adaptation 0 / 1, Numpad 7 = effect on/off, Numpad 9 = light list in the log, Numpad * = full glare test.
