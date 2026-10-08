// 5_Mission: hooks into the running client mission. Runs only on the client.
modded class MissionGameplay
{
	protected ref EyeAdaptState m_EyeAdapt;
	protected PPERequester_EyeAdapt m_EyeAdaptPPE;
	protected bool m_EyeAdaptBypass; // debug: visual effect switched off for A/B comparison
	protected float m_EyeAdaptNight;  // night gate 0 (day) .. 1 (night)
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

		if (EyeAdaptConfig.DEBUG_KEYS)
			UpdateEyeAdaptDebugKeys(player);

		// Adaptation keeps running by day (eyes keep pace through dusk), only the visuals are gated
		m_EyeAdapt.Update(timeslice);
		m_EyeAdaptNight = EyeAdaptEnvironment.GetNightFactor();
		ApplyEyeAdaptVisuals();

		m_EyeAdaptLogTimer += timeslice;
		if (m_EyeAdaptLogTimer >= EyeAdaptConfig.LOG_INTERVAL)
		{
			m_EyeAdaptLogTimer = 0;
			World world = g_Game.GetWorld();
			Print(string.Format("[EyeAdapt] adaptation %1 | night %2 | boost %3 EV | engine EV %4 | bypass %5 | daytime %6 | IsNight %7 | sunOrMoon %8 | sceneHDRMul %9",
				m_EyeAdapt.Get(), m_EyeAdaptNight, GetEyeAdaptEVBoost(), g_Game.GetCurrentEVValue(), m_EyeAdaptBypass,
				g_Game.GetDayTime(), world.IsNight(), world.GetSunOrMoon(), GetSceneHDRMul(0)));
		}
	}

	// Effect strength 0..1 that drives all visuals. Rounded to 1/500 so the PPE requests
	// only resend when a value visibly changes (EV steps of ~0.01).
	protected float GetEyeAdaptStrength()
	{
		if (m_EyeAdaptBypass)
			return 0;

		return Math.Round(m_EyeAdapt.Get() * m_EyeAdaptNight * 500) / 500;
	}

	protected float GetEyeAdaptEVBoost()
	{
		return GetEyeAdaptStrength() * EyeAdaptConfig.MAX_EV_BOOST;
	}

	protected void ApplyEyeAdaptVisuals()
	{
		if (!m_EyeAdaptPPE)
			m_EyeAdaptPPE = PPERequester_EyeAdapt.Cast(PPERequesterBank.GetRequester(PPERequester_EyeAdapt));

		// Vanilla stops all gameplay requesters on spawn/respawn, so (re)start when needed
		if (!m_EyeAdaptPPE.IsRequesterRunning())
			m_EyeAdaptPPE.Start();

		float s = GetEyeAdaptStrength();
		float saturation = 1 - s * EyeAdaptConfig.MAX_DESATURATION;
		float contrast = 1 + s * EyeAdaptConfig.MAX_CONTRAST_BOOST;

		// Blue shift: keep blue, pull red down fully and green halfway
		float shift = s * EyeAdaptConfig.BLUE_SHIFT;
		vector tint = Vector(1 - shift, 1 - shift * 0.5, 1);

		m_EyeAdaptPPE.Apply(GetEyeAdaptEVBoost(), saturation, contrast, tint);
	}

	// ---- Debug keys (see EyeAdaptConfig) ----

	protected void UpdateEyeAdaptDebugKeys(PlayerBase player)
	{
		// Ignore keys while typing in chat or using any menu
		if (GetUIManager().GetMenu())
			return;

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_RESET))
		{
			m_EyeAdapt.Set(0);
			player.MessageImportant("[EyeAdapt] adaptation reset to 0");
		}

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_FULL))
		{
			m_EyeAdapt.Set(1);
			player.MessageImportant("[EyeAdapt] adaptation set to 1");
		}

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_BYPASS))
		{
			m_EyeAdaptBypass = !m_EyeAdaptBypass;
			if (m_EyeAdaptBypass)
				player.MessageImportant("[EyeAdapt] effect OFF (vanilla)");
			else
				player.MessageImportant("[EyeAdapt] effect ON");
		}
	}

	// True once per key press (ClearKey suppresses auto-repeat until the key is released)
	protected bool EyeAdaptKeyPressed(int key)
	{
		if (KeyState(key) == 0)
			return false;

		ClearKey(key);
		return true;
	}
}
