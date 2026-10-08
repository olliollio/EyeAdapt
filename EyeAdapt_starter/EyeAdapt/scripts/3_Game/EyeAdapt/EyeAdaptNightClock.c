// 3_Game: measures how long a night lasts in real seconds, for AdaptationMode "relativeToNight" (CLAUDE.md §3.1).
// Compares in-game time progress with real time, so serverTimeAcceleration and serverNightTimeAcceleration are
// picked up automatically and admins don't have to calculate anything (docs/engine/world.md §4).
// Ticked from MissionGameplay (client) and MissionServer (server, for the offline curve in the persistence).
class EyeAdaptNightClock
{
	static const float SAMPLE_SECONDS = 30.0;     // real seconds between samples
	static const float MAX_HOURS_PER_SAMPLE = 2.0; // more game time than this in one sample = time jump (admin, restart), ignored
	static const float SMOOTHING = 0.3;           // weight of a new sample in the running average
	static const float OUTLIER = 0.25;            // a sample more than 25 % off the average needs a second one to confirm it

	protected static float s_Timer;
	protected static float s_LastDayTime = -1;
	protected static float s_HoursPerSecond;      // measured night time speed, 0 = not measured yet
	protected static float s_Candidate;           // unconfirmed outlier sample, 0 = none

	static void Update(float timeslice)
	{
		s_Timer += timeslice;
		if (s_Timer < SAMPLE_SECONDS)
			return;

		float elapsed = s_Timer;
		s_Timer = 0;

		float now = g_Game.GetDayTime();
		float last = s_LastDayTime;
		s_LastDayTime = now;
		if (last < 0)
			return;

		// Only measure at night, the night can run at its own speed (serverNightTimeAcceleration)
		if (!IsDeepNight(last) || !IsDeepNight(now))
			return;

		float hours = now - last;
		if (hours < 0)
			hours += 24; // past midnight
		if (hours <= 0 || hours > MAX_HOURS_PER_SAMPLE)
			return;

		float rate = hours / elapsed;
		if (s_HoursPerSecond <= 0)
		{
			s_HoursPerSecond = rate;
		}
		else if (Math.AbsFloat(rate - s_HoursPerSecond) > s_HoursPerSecond * OUTLIER)
		{
			// Small admin time changes inside one sample look like a slower or faster clock. Only accept a big change
			// when the next sample confirms it (the server speed really changed), otherwise ignore it
			if (s_Candidate > 0 && Math.AbsFloat(rate - s_Candidate) <= s_Candidate * OUTLIER)
			{
				s_HoursPerSecond = (rate + s_Candidate) * 0.5;
				s_Candidate = 0;
			}
			else
			{
				s_Candidate = rate;
				return;
			}
		}
		else
		{
			s_HoursPerSecond += (rate - s_HoursPerSecond) * SMOOTHING;
			s_Candidate = 0;
		}

		Print(string.Format("[EyeAdapt] night clock: %1 game h per real s, night lasts %2 real s, full adaptation %3 s",
			s_HoursPerSecond, GetNightRealSeconds(), EyeAdaptSettings.GetFullAdaptSeconds()));
	}

	// Real seconds from sunset to sunrise at the measured speed, 0 = not measured yet
	static float GetNightRealSeconds()
	{
		if (s_HoursPerSecond <= 0)
			return 0;

		float sunrise, sunset;
		if (!GetSunTimes(sunrise, sunset))
			return 0;

		float nightHours = 24 - sunset + sunrise;
		return nightHours / s_HoursPerSecond;
	}

	// Between the end of dusk and the start of dawn (no twilight, where the server switches speeds)
	protected static bool IsDeepNight(float time)
	{
		float sunrise, sunset;
		if (!GetSunTimes(sunrise, sunset))
			return false;

		return time >= sunset + 1 || time < sunrise - 1;
	}

	protected static bool GetSunTimes(out float sunrise, out float sunset)
	{
		Mission mission = g_Game.GetMission();
		if (!mission || !mission.GetWorldData())
			return false;

		int year, month, day, hour, minute;
		g_Game.GetWorld().GetDate(year, month, day, hour, minute);
		float monthday = month + (day - 1) / 31.0;
		sunrise = mission.GetWorldData().GetApproxSunriseTime(monthday);
		sunset = mission.GetWorldData().GetApproxSunsetTime(monthday);
		return true;
	}
}
