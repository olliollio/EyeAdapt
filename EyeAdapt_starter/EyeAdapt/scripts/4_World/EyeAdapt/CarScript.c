// 4_World: debug helper for testing headlights alone (debug key KEY_CAR_LIGHTS).
// Car lights are client-only objects anyway (lights.md §3), so a light created here behaves for the dose scan
// exactly like real headlights, but doesn't depend on the synced light state (which goes off when you get out).
modded class CarScript
{
	protected CarLightBase m_EyeAdaptTestLight;

	// Both bulbs on, built like vanilla's UpdateLightsClient (CarHeadlightBulbsState.BOTH). Returns true if now on
	bool EyeAdapt_ToggleTestHeadlights()
	{
		if (m_EyeAdaptTestLight)
		{
			m_EyeAdaptTestLight.Destroy();
			m_EyeAdaptTestLight = null;
			return false;
		}

		m_EyeAdaptTestLight = CreateFrontLight();
		if (!m_EyeAdaptTestLight)
			return false;

		vector left = GetMemoryPointPos(m_LeftHeadlightPoint);
		vector right = GetMemoryPointPos(m_RightHeadlightPoint);
		m_EyeAdaptTestLight.AttachOnObject(this, (left + right) * 0.5);
		m_EyeAdaptTestLight.AggregateLight();
		return true;
	}

	override void CleanupEffects()
	{
		super.CleanupEffects();
		if (m_EyeAdaptTestLight)
			m_EyeAdaptTestLight.Destroy();
	}
}
