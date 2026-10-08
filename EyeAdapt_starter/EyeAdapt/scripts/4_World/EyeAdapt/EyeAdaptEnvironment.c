// 4_World: reads the world state that limits the effect: night gate, moon/overcast, underground.
// See docs/engine/world.md §1-3 and §6.
class EyeAdaptEnvironment
{
	// Design rule 4: adaptation amplifies existing light, it doesn't create it.
	// Returns how much of the max EV boost the current sky allows, MIN_AMBIENT_GAIN (darkest) .. 1 (clear full moon).
	// Underground there is nothing to amplify, so the gain drops to UNDERGROUND_GAIN.
	static float GetAmbientGain(PlayerBase player)
	{
		float ambient = GetSkyLight();
		float gain = Math.Lerp(EyeAdaptConfig.MIN_AMBIENT_GAIN, 1, ambient);

		if (player && player.m_UndergroundPresence > EUndergroundPresence.OUTER)
			gain *= EyeAdaptConfig.UNDERGROUND_GAIN;
		else if (player && player.m_UndergroundPresence == EUndergroundPresence.OUTER)
			gain *= Math.Lerp(EyeAdaptConfig.UNDERGROUND_GAIN, 1, 0.5);

		return gain;
	}

	// Natural night light 0..1 relative to a clear full-moon night
	static float GetSkyLight()
	{
		// Moon: clear moonless sky still has starlight
		float sky = Math.Lerp(EyeAdaptConfig.STARLIGHT_SHARE, 1, GetMoonLight());

		// Clouds: the engine blends clear (overcast 0.35) into fully overcast (1.0) lighting,
		// and its fully overcast night has no moonlight at all
		float clouds = Math.Clamp(Math.InverseLerp(EyeAdaptConfig.OVERCAST_CLEAR, EyeAdaptConfig.OVERCAST_FULL, GetOvercast()), 0, 1);
		sky *= Math.Lerp(1, EyeAdaptConfig.OVERCAST_MIN_SHARE, clouds);

		return sky;
	}

	// Moon light 0..1. Raw GetMoonIntensity() has no documented range, MOON_INTENSITY_FULL calibrates it (see log)
	static float GetMoonLight()
	{
		if (EyeAdaptConfig.MOON_INTENSITY_FULL <= 0)
			return 0;

		return Math.Clamp(GetMoonIntensityRaw() / EyeAdaptConfig.MOON_INTENSITY_FULL, 0, 1);
	}

	static float GetMoonIntensityRaw()
	{
		return g_Game.GetWorld().GetMoonIntensity();
	}

	static float GetOvercast()
	{
		return g_Game.GetWeather().GetOvercast().GetActual();
	}

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
