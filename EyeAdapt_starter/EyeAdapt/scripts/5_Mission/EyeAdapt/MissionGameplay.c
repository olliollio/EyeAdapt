// 5_Mission: hooks into the running client mission. Runs only on the client.
modded class MissionGameplay
{
	protected ref EyeAdaptState m_EyeAdapt;
	protected ref EyeAdaptLightDose m_EyeAdaptDose;
	protected float m_EyeAdaptScanTimer;
	protected float m_EyeAdaptReportTimer;
	protected PPERequester_EyeAdapt m_EyeAdaptPPE;
	protected bool m_EyeAdaptBypass; // debug: visual effect switched off for A/B comparison
	protected float m_EyeAdaptNight;  // night gate 0 (day) .. 1 (night)
	protected float m_EyeAdaptAmbient = 1;       // share of the max EV boost the sky allows (moon/overcast/underground), smoothed
	protected float m_EyeAdaptAmbientTarget = 1; // latest value from the scan
	protected float m_EyeAdaptLogTimer;
	protected bool m_EyeAdaptNightVision; // powered NVG/NV scope in use: counts as light (NIGHT_VISION_DOSE)

	override void OnInit()
	{
		super.OnInit();

		// Guard against a second OnInit call wiping the state
		if (!m_EyeAdapt)
		{
			m_EyeAdapt = new EyeAdaptState();
			m_EyeAdaptDose = new EyeAdaptLightDose();
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

		// Value restored by the server after login (offline time counted as darkness)
		if (EyeAdaptClient.s_PendingRestore >= 0)
		{
			m_EyeAdapt.Set(EyeAdaptClient.s_PendingRestore);
			Print(string.Format("[EyeAdapt] adaptation restored from server: %1", EyeAdaptClient.s_PendingRestore));
			EyeAdaptClient.s_PendingRestore = -1;
		}

		if (EyeAdaptSettings.Get().DebugKeys)
			UpdateEyeAdaptDebugKeys(player);

		// Light scan every SCAN_INTERVAL, adaptation smooths in between
		m_EyeAdaptScanTimer += timeslice;
		if (m_EyeAdaptScanTimer >= EyeAdaptSettings.Get().ScanInterval)
		{
			m_EyeAdaptScanTimer = 0;
			m_EyeAdaptDose.Scan();
			m_EyeAdaptAmbientTarget = EyeAdaptEnvironment.GetAmbientGain(player);
		}

		// Adaptation keeps running by day (eyes keep pace through dusk), only the visuals are gated.
		// Powered night vision puts a bright green screen in front of the eyes (green is what rods are most
		// sensitive to), so it counts as light: after taking the goggles off you're partly night-blind.
		// The visuals already step aside for night vision in the requester.
		float dose = m_EyeAdaptDose.Get();
		m_EyeAdaptNightVision = EyeAdaptEnvironment.IsNightVisionActive(player);
		if (m_EyeAdaptNightVision)
			dose = Math.Max(dose, EyeAdaptSettings.Get().NightVisionDose);

		float target = 1 - dose * EyeAdaptSettings.Get().MaxDoseEffect;
		m_EyeAdapt.Update(timeslice, target);

		// Report to the server, which saves the latest value on logout
		m_EyeAdaptReportTimer += timeslice;
		if (m_EyeAdaptReportTimer >= EyeAdaptSettings.Get().ReportIntervalSeconds)
		{
			m_EyeAdaptReportTimer = 0;
			EyeAdaptClient.Report(m_EyeAdapt.Get());
		}

		// Smooth the ambient gain so scan steps and underground state switches don't jump the brightness
		float k = Math.Clamp(timeslice / EyeAdaptSettings.Get().AmbientSmoothSeconds, 0, 1);
		m_EyeAdaptAmbient += (m_EyeAdaptAmbientTarget - m_EyeAdaptAmbient) * k;
		m_EyeAdaptNight = EyeAdaptEnvironment.GetNightFactor();
		ApplyEyeAdaptVisuals();

		m_EyeAdaptLogTimer += timeslice;
		if (m_EyeAdaptLogTimer >= EyeAdaptConfig.LOG_INTERVAL)
		{
			m_EyeAdaptLogTimer = 0;
			Print(string.Format("[EyeAdapt] adaptation %1 | dose %2 | strongest %3 (%4) | lights %5 | night %6 | boost %7 EV | bypass %8 | daytime %9",
				m_EyeAdapt.Get(), m_EyeAdaptDose.Get(), m_EyeAdaptDose.GetStrongestName(), m_EyeAdaptDose.GetStrongestDose(),
				m_EyeAdaptDose.GetLightCount(), m_EyeAdaptNight, GetEyeAdaptEVBoost(), m_EyeAdaptBypass, g_Game.GetDayTime()));
			Print(string.Format("[EyeAdapt] env | moon raw %1 | moon light %2 | overcast %3 | sky light %4 | ambient gain %5 | underground %6 | NV type %7 (active %8)",
				EyeAdaptEnvironment.GetMoonIntensityRaw(), EyeAdaptEnvironment.GetMoonLight(), EyeAdaptEnvironment.GetOvercast(),
				EyeAdaptEnvironment.GetSkyLight(), m_EyeAdaptAmbient, player.m_UndergroundPresence, player.GetNVType(), m_EyeAdaptNightVision));
		}
	}

	// Effect strength 0..1 that drives all visuals. Rounded to 1/500 so the PPE requests
	// only resend when a value visibly changes (EV steps of ~0.01).
	protected float GetEyeAdaptStrength()
	{
		// Disabled by the server or switched off with the debug key: all PPE values get released
		if (m_EyeAdaptBypass || !EyeAdaptSettings.Get().Enabled)
			return 0;

		return Math.Round(m_EyeAdapt.Get() * m_EyeAdaptNight * 500) / 500;
	}

	// Brightness is limited by what the sky offers (design rule 4), colour loss is not:
	// fully adapted eyes see in grey even on the darkest night
	protected float GetEyeAdaptEVBoost()
	{
		return GetEyeAdaptStrength() * m_EyeAdaptAmbient * EyeAdaptSettings.Get().MaxEvBoost;
	}

	protected void ApplyEyeAdaptVisuals()
	{
		if (!m_EyeAdaptPPE)
			m_EyeAdaptPPE = PPERequester_EyeAdapt.Cast(PPERequesterBank.GetRequester(PPERequester_EyeAdapt));

		// Vanilla stops all gameplay requesters on spawn/respawn, so (re)start when needed
		if (!m_EyeAdaptPPE.IsRequesterRunning())
			m_EyeAdaptPPE.Start();

		float s = GetEyeAdaptStrength();
		float saturation = 1 - s * EyeAdaptSettings.Get().MaxDesaturation;
		float contrast = 1 + s * m_EyeAdaptAmbient * EyeAdaptSettings.Get().MaxContrastBoost; // counters the haze of the EV boost, so it scales with it

		// Blue shift: keep blue, pull red down fully and green halfway
		float shift = s * EyeAdaptSettings.Get().BlueShift;
		vector tint = Vector(1 - shift, 1 - shift * 0.5, 1);

		m_EyeAdaptPPE.Apply(GetEyeAdaptEVBoost(), saturation, contrast, tint);
	}

	// Server started our logout countdown: send one final report so the saved value is current
	override void StartLogoutMenu(int time)
	{
		super.StartLogoutMenu(time);
		if (m_EyeAdapt)
			EyeAdaptClient.Report(m_EyeAdapt.Get());
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

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_LIGHTS))
		{
			m_EyeAdaptDose.Scan(true);
			player.MessageImportant(string.Format("[EyeAdapt] dose %1, strongest %2 - details in script log", m_EyeAdaptDose.Get(), m_EyeAdaptDose.GetStrongestName()));
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
