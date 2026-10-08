// 4_World: reads the world state that limits the effect (night gate now, moon/overcast in Phase 4).
// See docs/engine/world.md §1.
class EyeAdaptEnvironment
{
	// 0 = day (effect off, vanilla untouched), 1 = full night.
	// Fades in after sunset and out before sunrise, using the world's own sunrise/sunset
	// (WorldData, so modded maps with other times are picked up automatically).
	static float GetNightFactor()
	{
		Mission mission = g_Game.GetMission();
		if (!mission || !mission.GetWorldData())
			return 0;

		int year, month, day, hour, minute;
		g_Game.GetWorld().GetDate(year, month, day, hour, minute);

		// Fractional month so sunrise/sunset move smoothly instead of jumping once a month like vanilla
		float monthday = month + (day - 1) / 31.0;
		WorldData worldData = mission.GetWorldData();
		float sunrise = worldData.GetApproxSunriseTime(monthday);
		float sunset = worldData.GetApproxSunsetTime(monthday);

		float time = g_Game.GetDayTime(); // hours, 0..24
		float noon = (sunrise + sunset) * 0.5;

		if (time >= noon)
		{
			// Evening: fade in after sunset
			float duskStart = sunset + EyeAdaptConfig.DUSK_OFFSET_HOURS;
			return SmoothStep((time - duskStart) / EyeAdaptConfig.DUSK_HOURS);
		}

		// Morning (after midnight): fade out towards sunrise
		float dawnStart = sunrise + EyeAdaptConfig.DAWN_OFFSET_HOURS;
		return 1 - SmoothStep((time - dawnStart) / EyeAdaptConfig.DAWN_HOURS);
	}

	// Clamped 0..1 with eased ends, so the fade has no visible start/stop kink
	protected static float SmoothStep(float x)
	{
		x = Math.Clamp(x, 0, 1);
		return x * x * (3 - 2 * x);
	}
}
