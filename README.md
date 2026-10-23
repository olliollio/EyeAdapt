# EyeAdapt - Eye Adaptation for DayZ

EyeAdapt simulates how eyes adapt to darkness and light at night. Night becomes something the player manages, not a fixed server setting.

**Steam Workshop (test build):** https://steamcommunity.com/sharedfiles/filedetails/?id=3817055126

## What it does

- Stay in the dark and your vision slowly improves: brighter image, less colour, a slight blue tint.
- Light costs night vision: campfires, flashlights, headlights, flares. Red light barely costs any.
- Optional glare when dark-adapted eyes face bright light, and optional flashlight dazzle from other players.
- Night only. By day nothing changes.
- Never darker than the server's own night. The mod only adds brightness and leaves your lighting config alone.
- Adaptation is saved per player on the server, also across restarts. By default you log back in with the value you logged off with (configurable), so relogging gives no advantage.
- Fully configurable by server admins through `config.json`.

## Documentation

| Guide | For |
|---|---|
| [Admin guide](docs/ADMIN.md) | Server owners: install, `config.json`, every setting, light table, example presets, performance |
| [Tester guide](docs/TESTER.md) | Testers: test keys and what to try |
| [Ideas](docs/ideas.md) | Things discussed but not built |

## Install (server owners)

1. Subscribe to the Workshop item and add the mod to the `-mod=` list of the server **and** the clients.
2. Copy `keys/EyeAdapt.bikey` from the mod into the server's `keys` folder.
3. Start the server once. It creates `profiles/EyeAdapt/config.json` with the defaults.
4. Edit `config.json` and restart the server. See the [admin guide](docs/ADMIN.md).

The debug keys (Numpad) are on by default in the test build. Set `DebugKeys` to `false` on live servers.

## Status

Test build (0.1.0). Mechanics work in game, tuning and multiplayer tests are ongoing. Feedback is welcome, see the [tester guide](docs/TESTER.md).

## Repository layout

| Path | Content |
|---|---|
| `EyeAdapt_starter/EyeAdapt/` | Mod source: `config.cpp` and the scripts (3_Game, 4_World, 5_Mission) |
| `docs/` | Admin guide, tester guide, setup notes, engine notes |
| `release/` | `mod.cpp` and the launcher logos for the packed mod |
| `Artwork/` | Logo source files |
| `dev/server/` | Server config files used for local testing |

To build and test locally, see [docs/SETUP.md](docs/SETUP.md).

## License

[MIT](LICENSE) - Copyright (c) 2026 ollio
