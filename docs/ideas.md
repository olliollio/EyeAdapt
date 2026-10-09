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

## Light table replaces car light settings

`CarLightFactor` / `CarLightRange` are hard-wired to `CarLightBase` (minus rear lights). Once the light table (Claude.md §12 step 2) exists, they become default table entries with a per-class range.
