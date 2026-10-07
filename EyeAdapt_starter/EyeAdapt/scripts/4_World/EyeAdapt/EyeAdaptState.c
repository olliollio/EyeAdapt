// 4_World: logic that needs world/player classes. Adaptation state lives here later.
class EyeAdaptState
{
	protected float m_Adaptation; // 0 = light-adapted, 1 = fully dark-adapted

	void Update(float timeslice)
	{
		// Placeholder: linear ramp. Replaced later with a proper curve + light exposure.
		m_Adaptation = Math.Clamp(m_Adaptation + timeslice / EyeAdaptConfig.FULL_ADAPT_SECONDS, 0, 1);
	}

	float Get()
	{
		return m_Adaptation;
	}
}
