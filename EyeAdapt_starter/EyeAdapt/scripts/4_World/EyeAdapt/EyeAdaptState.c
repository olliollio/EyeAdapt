// 4_World: client-side adaptation state.
// Two stages approach the target independently: a fast one (pupil/cones, seconds)
// and a slow one (rods, minutes). The result is "quickly a bit better, then slowly much better".
// Light makes both drop much faster than darkness makes them rise. A short flash mostly costs
// the fast stage (back in seconds), long exposure also drains the slow stage (minutes to recover).
class EyeAdaptState
{
	protected float m_Fast; // 0 = light-adapted, 1 = fully dark-adapted
	protected float m_Slow;

	void EyeAdaptState()
	{
		Set(EyeAdaptConfig.START_ADAPTATION);
	}

	// target: 1 = darkness, lower = light exposure (from the light dose)
	void Update(float timeslice, float target)
	{
		m_Fast = Approach(m_Fast, target, timeslice, EyeAdaptConfig.FAST_CURVE_SECONDS, EyeAdaptConfig.LIGHT_FAST_SECONDS);
		m_Slow = Approach(m_Slow, target, timeslice, EyeAdaptConfig.FULL_ADAPT_SECONDS, EyeAdaptConfig.LIGHT_SLOW_SECONDS);
	}

	// 0 = light-adapted, 1 = fully dark-adapted
	float Get()
	{
		float w = Math.Clamp(EyeAdaptConfig.FAST_CURVE_WEIGHT, 0, 1);
		return w * m_Fast + (1 - w) * m_Slow;
	}

	void Set(float value)
	{
		m_Fast = Math.Clamp(value, 0, 1);
		m_Slow = m_Fast;
	}

	// Exponential approach, frame-rate independent. Covers ~95 % of the distance after the given seconds
	// (3 time constants): secondsUp when getting darker-adapted, secondsDown under light.
	protected static float Approach(float current, float target, float dt, float secondsUp, float secondsDown)
	{
		float seconds = secondsUp;
		if (target < current)
			seconds = secondsDown;

		if (seconds <= 0)
			return target;

		float k = 1 - Math.Pow(Math.EULER, -3 * dt / seconds);
		return current + (target - current) * k;
	}
}
