# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/). The version is `version` in `release/mod.cpp`.
Config changes: new keys are added to an existing `config.json` automatically, existing values stay.

## [0.2.0] - 2026-10-15

### Added
- `FullMoonShare` (default 0.8): share of `MaxEvBoost` under a clear full moon. Real-time servers with dark nights found full-moon nights too bright. `StarlightShare` uses the same scale. Config version 24.
- Light table in `config.json` (`Lights`): per light class a brightness factor, colour handling (`auto` / `red` / `full`) and range. Muzzle flashes are ignored, flares, headlights and fireworks count more. Numpad 9 (with `DebugKeys`) prints the class names and matched entries, to add lights from other mods.
- Flash grenade hit drops adaptation (`FlashbangAdaptationLoss`, 1 = to zero, 0 = off).

### Changed
- `FullAdaptSeconds` default 180 -> 600 (10 min).
- `StartAdaptation` default 0.2: new players and fresh characters start dark-adapted at 20 %, not at full night vision.
- Adaptation is kept in memory on the server, no player files. Relogging still gains nothing, a server restart counts as sleep.
- `DebugKeys` default `false`, so players can't cheat with the Numpad on new configs.

- Adaptation curve ("NA") with the new defaults. Two stages move towards full darkness independently:
  - Fast stage (pupil/cones): ~95 % after `FastCurveSeconds` (20 s), `FastCurveWeight` 0.3 of the total.
  - Slow stage (rods): ~95 % after `FullAdaptSeconds` (600 s), the remaining 0.7.
  - Visual strength = NA ^ `AdaptCurvePower` (1.5), so the first minutes show little and the late phase counts more.
  - Light pulls both stages down much faster than darkness raises them (`LightFastSeconds` 2 s, `LightSlowSeconds` 8 s).
  - Example from a 20 % start (defaults, in darkness): NA 0.46 / strength 0.31 after 15 s, 0.59 / 0.45 after 1 min, 0.69 / 0.58 after 2 min, 0.88 / 0.82 after 5 min, 0.97 / 0.96 after 10 min.

### Fixed
- Log spam: night clock, login restore, glare bloom, "settings received" and "loaded" lines are now behind `DebugLogging` (default `false`). Only the one-time server config lines remain.

## [0.1.0] - first test release

- Eye adaptation at night: fast + slow stage, asymmetric speeds, EV boost, desaturation, contrast, blue shift.
- Light exposure from fires, flashlights, headlights and others, with occlusion. Red light costs little.
- Glare, optional flashlight dazzle, ambient scaling by moon, overcast and underground.
- Server config with sync to clients.
