// 5_Mission: hooks into the running client mission. Runs only on the client.
modded class MissionGameplay
{
	protected ref EyeAdaptState m_EyeAdapt;
	protected float m_EyeAdaptLogTimer;

	override void OnInit()
	{
		super.OnInit();

		// Guard against a second OnInit call wiping the state
		if (!m_EyeAdapt)
		{
			m_EyeAdapt = new EyeAdaptState();
			Print("[EyeAdapt] loaded");
		}
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!m_EyeAdapt)
			return;

		// Only tick while a living local player exists (not on loading/death screens)
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (!player || !player.IsAlive())
			return;

		m_EyeAdapt.Update(timeslice);

		m_EyeAdaptLogTimer += timeslice;
		if (m_EyeAdaptLogTimer >= EyeAdaptConfig.LOG_INTERVAL)
		{
			m_EyeAdaptLogTimer = 0;
			Print(string.Format("[EyeAdapt] adaptation = %1 | engine EV %2", m_EyeAdapt.Get(), g_Game.GetCurrentEVValue()));
		}

		if (EyeAdaptConfig.TEST_EV_CYCLE)
			UpdateEyeAdaptEVTest(player, timeslice);
	}

	// ---- Phase 1 test: does a script exposure boost brighten the night? ----

	protected PPERequester_EyeAdapt m_EyeAdaptPPE;
	protected float m_EyeAdaptTestTimer = -1; // -1 = first tick, apply step 0 immediately
	protected int m_EyeAdaptTestStep;

	protected void UpdateEyeAdaptEVTest(PlayerBase player, float timeslice)
	{
		if (!m_EyeAdaptPPE)
			m_EyeAdaptPPE = PPERequester_EyeAdapt.Cast(PPERequesterBank.GetRequester(PPERequester_EyeAdapt));

		// Vanilla stops all gameplay requesters on spawn/respawn, so (re)start when needed
		if (!m_EyeAdaptPPE.IsRequesterRunning())
			m_EyeAdaptPPE.Start();

		if (m_EyeAdaptTestTimer >= 0)
		{
			m_EyeAdaptTestTimer += timeslice;
			if (m_EyeAdaptTestTimer < EyeAdaptConfig.TEST_EV_STEP_SECONDS)
				return;
			m_EyeAdaptTestStep = (m_EyeAdaptTestStep + 1) % (EyeAdaptConfig.TEST_EV_MAX + 1);
		}
		m_EyeAdaptTestTimer = 0;

		m_EyeAdaptPPE.SetExposureBoost(m_EyeAdaptTestStep);

		player.MessageImportant("[EyeAdapt] test boost +" + m_EyeAdaptTestStep.ToString() + " EV");
		// PPE applies the request on its next update, so "engine EV" may still show the previous step's value
		Weather weather = g_Game.GetWeather();
		Print(string.Format("[EyeAdapt] test boost +%1 EV | engine EV %2 | sceneHDRMul %3 | daytime %4 | fog %5 | overcast %6 | moon %7",
			m_EyeAdaptTestStep, g_Game.GetCurrentEVValue(), GetSceneHDRMul(0), g_Game.GetDayTime(),
			weather.GetFog().GetActual(), weather.GetOvercast().GetActual(), g_Game.GetWorld().GetMoonIntensity()));
	}
}
