# Ideas (parked)

Things discussed and deliberately not built (yet). The mod should stay as light as possible: only build these when testing shows they matter.

## Own car: weight the dose by view direction

Today every seat in a car with headlights on gets `OwnVehicleLightDose` (0.5), regardless of where the player looks.

Idea: multiply it by how much the camera looks along the car (`Lerp(ViewMinFactor, 1, dot(cameraDir, carForward))`), like other lights. Looking forward = full 0.5, looking out the side window or back = down to 0.25 of it.

- Effect: a passenger watching the dark side keeps more NA than the driver and can spot things on arrival. Also affects the driver in freelook.
- Cost: one dot product per scan, ~5 lines in `EyeAdaptLightDose.Scan`.
- Decision (2026-10-09): leave all seats equal, revisit after a test with a second player.

## Remove grass occlusion again

`GetGrassOcclusionFactor` (settings `UseGrassOcclusion`, `Grass*`) samples the line eye → light because grass clutter has no geometry. Added for headlights behind a small grassy crest.

- Edge case, costs a few `SurfaceY`/`SurfaceGetType` calls per visible light per scan.
- Decision (2026-10-09): keep for now, candidate for removal at release prep / performance check.

## Admin option: personal light in bunkers

Test on Livonia (2026-10-09/10): the mod works as expected, but inside the bunker everything is pitch black. Realistic (design rule 4: nothing to amplify underground, so the EV boost is 0), but some players/admins may find it unplayable.

Idea: server setting that gives the player a dim personal light while underground. Default stays off (pitch black = current behaviour).

- Settings (config.json): `UndergroundLightEnabled` (default false), `UndergroundLightStrength` (0..1, ~0.3 suggested, dim instead of full light), optional `UndergroundLightFadeSeconds`.
- Detection: reuse `PlayerBase.m_UndergroundPresence` (already used for the underground ambient gain, Livonia and Sakhal only, see `docs/engine/world.md`). Fallback for other maps: depth check `player.Y < SurfaceY(x,z) - depth` (`UndergroundDepth`), optionally plus an admin list of zones (position + radius). A raycast "no sky above" is too broad (would also light ordinary indoor spaces).
- Open: fade in/out when entering or leaving? Skip it while NVG or a flashlight is active (no stacking)? Server config is the source of truth, synced to clients like all other settings.
- Cheapest implementation to check first: instead of a new light, raise the ambient floor (EV boost) underground by `UndergroundLightStrength` — no extra light entity needed.
- Decision (2026-10-10): parked, do it later. Document in `docs/ADMIN.md` when built.

## Short flashes (explosions, flashbang, fireworks)

`ExplosiveLight` and `ShellLight` live only 0.15-0.35 s, so the 0.25 s light scan often misses them. A real blast at night costs night vision. Already handled: flash grenades (hook on vanilla's flashbang hit, `FlashbangAdaptationLoss`) and fireworks (`ExplosionLight` lasts 2.1 s, in the light table).

- Fix would be a hook when such a light is created (registry `EOnInit`): compute its dose once and apply it as a short dose spike.
- The flashbang already has its own vanilla white-out.
- Decision (2026-10-09): edge case, not built.
