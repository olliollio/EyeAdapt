// 5_Mission: hooks into the running client mission. Runs only on the client.
modded class MissionGameplay
{
	protected ref EyeAdaptState m_EyeAdapt;
	protected ref EyeAdaptLightDose m_EyeAdaptDose;
	protected float m_EyeAdaptScanTimer;
	protected float m_EyeAdaptReportTimer;
	protected float m_EyeAdaptNightTimer = 1e6; // seconds since the night gate was read (it changes over minutes, so once a second is plenty)
	protected PPERequester_EyeAdapt m_EyeAdaptPPE;
	protected bool m_EyeAdaptBypass; // debug: visual effect switched off for A/B comparison
	protected float m_EyeAdaptNight;  // night gate 0 (day) .. 1 (night)
	protected float m_EyeAdaptAmbient = 1;       // share of the max EV boost the sky allows (moon/overcast/underground), smoothed
	protected float m_EyeAdaptAmbientTarget = 1; // latest value from the scan
	protected float m_EyeAdaptGlare;             // glare phase 1 (blowout envelope), 0..GlareStrength
	protected float m_EyeAdaptGlareLevel;        // strength of the running blowout (set when it starts)
	protected float m_EyeAdaptGlareTime = -1;    // seconds since the blowout started, -1 = none
	protected float m_EyeAdaptSinceGlare = 1e6;  // seconds since the last blowout/dazzle ended (shared recovery, GlareRecoverySeconds)
	protected float m_EyeAdaptGlow;              // glare phase 2 (bloom halo fading out), 0..GlareStrength
	protected float m_EyeAdaptGlarePeak;         // highest glare since the last log line (glare is shorter than the log interval)
	protected bool m_EyeAdaptGlareTest;          // debug: force full glare
	protected float m_EyeAdaptDazzle;            // optional flashlight dazzle, 0..1
	protected float m_EyeAdaptDazzleLevel;       // strength of the running dazzle (set when it starts)
	protected float m_EyeAdaptDazzleTime = -1;   // seconds since the dazzle started, -1 = none
	protected bool m_EyeAdaptDazzleArmed = true; // false after a dazzle until the beam was out of the face for DazzleRearmSeconds
	protected float m_EyeAdaptDazzleClear;       // seconds the beam has been out of the face while disarmed
	protected float m_EyeAdaptColorDose;         // light dose smoothed for the colour return
	protected float m_EyeAdaptLogTimer;
	protected float m_EyeAdaptTextTimer;
	protected bool m_EyeAdaptNightVision; // powered NVG/NV scope in use: counts as light (NIGHT_VISION_DOSE)
	protected TextWidget m_EyeAdaptText; // optional on-screen adaptation readout (ShowAdaptation)
	protected ref EyeAdaptPerf m_EyeAdaptPerf; // performance test log (PerfLog)

	override void OnInit()
	{
		super.OnInit();

		// Guard against a second OnInit call wiping the state
		if (!m_EyeAdapt)
		{
			m_EyeAdapt = new EyeAdaptState();
			m_EyeAdaptDose = new EyeAdaptLightDose();
			if (EyeAdaptSettings.Get().DebugLogging)
				Print("[EyeAdapt] loaded");
		}
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!EyeAdaptSettings.Get().PerfLog)
		{
			UpdateEyeAdapt(timeslice);
			return;
		}
		if (!m_EyeAdaptPerf)
			m_EyeAdaptPerf = new EyeAdaptPerf();
		int start = TickCount(0);
		UpdateEyeAdapt(timeslice);
		m_EyeAdaptPerf.AddFrame(TickCount(start), timeslice);
	}

	protected void UpdateEyeAdapt(float timeslice)
	{
		if (!m_EyeAdapt)
			return;

		// Only tick while a living local player exists (not on loading/death screens)
		PlayerBase player = PlayerBase.Cast(GetGame().GetPlayer());
		if (!player || !player.IsAlive())
		{
			// Bloom lives on the material, not in a PPE requester, so vanilla's death reset doesn't cover it
			EyeAdaptBloom.Reset();
			ShowEyeAdaptText(false);
			return;
		}

		// Value restored by the server after login (offline time counted as darkness)
		if (EyeAdaptClient.s_PendingRestore >= 0)
		{
			m_EyeAdapt.Set(EyeAdaptClient.s_PendingRestore);
			if (EyeAdaptSettings.Get().DebugLogging)
				Print(string.Format("[EyeAdapt] adaptation restored from server: %1", EyeAdaptClient.s_PendingRestore));
			EyeAdaptClient.s_PendingRestore = -1;
		}

		// Flash grenade hit: the eyes are blinded, night vision is gone (the visual white-out is vanilla's)
		if (EyeAdaptClient.s_FlashbangHit)
		{
			EyeAdaptClient.s_FlashbangHit = false;
			float loss = Math.Clamp(EyeAdaptSettings.Get().FlashbangAdaptationLoss, 0, 1);
			m_EyeAdapt.Set(m_EyeAdapt.Get() * (1 - loss));
			if (EyeAdaptSettings.Get().DebugLogging)
				Print(string.Format("[EyeAdapt] flash grenade hit, adaptation -> %1", m_EyeAdapt.Get()));
		}

		if (EyeAdaptSettings.Get().DebugKeys)
			UpdateEyeAdaptDebugKeys(player);

		EyeAdaptNightClock.Update(timeslice);

		// Night gate, read once a second (an in-game hour takes minutes even at 48x)
		m_EyeAdaptNightTimer += timeslice;
		if (m_EyeAdaptNightTimer >= EyeAdaptConfig.NIGHT_INTERVAL)
		{
			m_EyeAdaptNightTimer = 0;
			m_EyeAdaptNight = EyeAdaptEnvironment.GetNightFactor();
		}

		// Light scan every ScanInterval, adaptation smooths in between. By day only the adaptation value needs
		// it (glare and dazzle are off), so a slower scan is enough
		float scanInterval = EyeAdaptSettings.Get().ScanInterval;
		if (m_EyeAdaptNight <= 0)
			scanInterval = Math.Max(scanInterval, EyeAdaptConfig.DAY_SCAN_INTERVAL);

		m_EyeAdaptScanTimer += timeslice;
		if (m_EyeAdaptScanTimer >= scanInterval)
		{
			m_EyeAdaptScanTimer = 0;
			int scanStart = TickCount(0);
			m_EyeAdaptDose.Scan();
			if (m_EyeAdaptPerf)
				m_EyeAdaptPerf.AddScan(TickCount(scanStart), m_EyeAdaptDose.GetLightCount(), m_EyeAdaptDose.GetPassedCount(), m_EyeAdaptDose.GetRaycastCount());
			m_EyeAdaptAmbientTarget = EyeAdaptEnvironment.GetAmbientGain(player);
		}

		// Adaptation keeps running by day (eyes keep pace through dusk), only the visuals are gated.
		// Powered night vision puts a bright green screen in front of the eyes (green is what rods are most
		// sensitive to), so it counts as light: after taking the goggles off you're partly night-blind.
		// The visuals already step aside for night vision in the requester.
		float dose = m_EyeAdaptDose.Get();
		m_EyeAdaptNightVision = EyeAdaptEnvironment.IsNightVisionActive(player);
		dose = Math.Max(dose, EyeAdaptEnvironment.GetNightVisionDose(player));

		float target = 1 - dose * EyeAdaptSettings.Get().MaxDoseEffect;
		m_EyeAdapt.Update(timeslice, target);

		// Report to the server, which saves the latest value on logout. Not by day: the effect is off and the
		// value is irrelevant by the next night (the logout report in StartLogoutMenu is always sent).
		// The timer keeps running by day, so the first report comes right at dusk
		m_EyeAdaptReportTimer += timeslice;
		if (m_EyeAdaptReportTimer >= EyeAdaptSettings.Get().ReportIntervalSeconds && m_EyeAdaptNight > 0)
		{
			m_EyeAdaptReportTimer = 0;
			EyeAdaptClient.Report(m_EyeAdapt.Get());
		}

		// Smooth the ambient gain so scan steps and underground state switches don't jump the brightness
		float k = Math.Clamp(timeslice / EyeAdaptSettings.Get().AmbientSmoothSeconds, 0, 1);
		m_EyeAdaptAmbient += (m_EyeAdaptAmbientTarget - m_EyeAdaptAmbient) * k;
		// Colour returns near light: smooth the dose so saturation doesn't step with each scan
		float colorK = Math.Clamp(timeslice / Math.Max(EyeAdaptSettings.Get().ColorReturnSmoothSeconds, 0.01), 0, 1);
		m_EyeAdaptColorDose += (m_EyeAdaptDose.Get() - m_EyeAdaptColorDose) * colorK;

		if (IsEyeAdaptGlareIdle())
		{
			// Day and nothing left running: skip the glare math. Keep the shared recovery clock going,
			// re-arm the dazzle (the beam doesn't matter by day) and make sure no bloom stays on the material
			m_EyeAdaptSinceGlare += timeslice;
			m_EyeAdaptDazzleArmed = true;
			EyeAdaptBloom.Reset();
		}
		else
		{
			UpdateEyeAdaptGlare(timeslice);
			UpdateEyeAdaptDazzle(timeslice);
		}
		ApplyEyeAdaptVisuals();

		m_EyeAdaptTextTimer += timeslice;
		if (m_EyeAdaptTextTimer >= EyeAdaptConfig.TEXT_INTERVAL)
		{
			m_EyeAdaptTextTimer = 0;
			UpdateEyeAdaptText();
		}

		m_EyeAdaptLogTimer += timeslice;
		if (m_EyeAdaptLogTimer >= EyeAdaptConfig.LOG_INTERVAL)
		{
			m_EyeAdaptLogTimer = 0;
			if (EyeAdaptSettings.Get().DebugLogging)
			{
				Print(string.Format("[EyeAdapt] adaptation %1 | dose %2 | strongest %3 (%4) | lights %5 | night %6 | boost %7 EV | bypass %8 | daytime %9",
					m_EyeAdapt.Get(), m_EyeAdaptDose.Get(), m_EyeAdaptDose.GetStrongestName(), m_EyeAdaptDose.GetStrongestDose(),
					m_EyeAdaptDose.GetLightCount(), m_EyeAdaptNight, GetEyeAdaptEVBoost(), m_EyeAdaptBypass, g_Game.GetDayTime()));
				Print(string.Format("[EyeAdapt] env | moon raw %1 | moon light %2 | overcast %3 | sky light %4 | ambient gain %5 | underground %6 | NV type %7 (active %8) | glare peak %9",
					EyeAdaptEnvironment.GetMoonIntensityRaw(), EyeAdaptEnvironment.GetMoonLight(), EyeAdaptEnvironment.GetOvercast(),
					EyeAdaptEnvironment.GetSkyLight(), m_EyeAdaptAmbient, player.m_UndergroundPresence, player.GetNVType(), m_EyeAdaptNightVision, m_EyeAdaptGlarePeak));
			}
			m_EyeAdaptGlarePeak = 0;
		}
	}

	// ---- On-screen adaptation readout (ShowAdaptation) ----

	protected void UpdateEyeAdaptText()
	{
		if (!EyeAdaptSettings.Get().ShowAdaptation)
		{
			ShowEyeAdaptText(false);
			return;
		}

		if (!m_EyeAdaptText)
		{
			Widget root = g_Game.GetWorkspace().CreateWidget(TextWidgetTypeID, 20, 20, 400, 44, WidgetFlags.VISIBLE | WidgetFlags.HEXACTPOS | WidgetFlags.VEXACTPOS | WidgetFlags.HEXACTSIZE | WidgetFlags.VEXACTSIZE, 0xffffffff, 1000);
			m_EyeAdaptText = TextWidget.Cast(root);
			if (!m_EyeAdaptText)
				return;

			m_EyeAdaptText.SetTextExactSize(28);
		}

		m_EyeAdaptText.SetText("NA " + Math.Round(m_EyeAdapt.Get() * 100).ToString() + " %");
		m_EyeAdaptText.Show(true);
	}

	protected void ShowEyeAdaptText(bool show)
	{
		if (m_EyeAdaptText)
			m_EyeAdaptText.Show(show);
	}

	// Effect strength 0..1 that drives all visuals. Rounded to 1/500 so the PPE requests
	// only resend when a value visibly changes (EV steps of ~0.01).
	protected float GetEyeAdaptStrength()
	{
		// Disabled by the server or switched off with the debug key: all PPE values get released
		if (m_EyeAdaptBypass || !EyeAdaptSettings.Get().Enabled)
			return 0;

		// Ease-in: the first minutes show little, the late phase counts more (AdaptCurvePower)
		return Math.Round(Math.Pow(m_EyeAdapt.Get(), EyeAdaptSettings.Get().AdaptCurvePower) * m_EyeAdaptNight * 500) / 500;
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

		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		float s = GetEyeAdaptStrength();
		// Rods see grey, but near light the cones take over and colour returns (CLAUDE.md §3.5)
		float rodVision = s * (1 - m_EyeAdaptColorDose);
		float saturation = 1 - rodVision * cfg.MaxDesaturation;
		float contrast = 1 + s * m_EyeAdaptAmbient * cfg.MaxContrastBoost; // counters the haze of the EV boost, so it scales with it
		// Pulls the sky lifted by the EV boost back to black. Ambient counts squared: on dark nights / dark server
		// configs the whole image sits close to 0 after the tonemap, a fixed offset would crush it to pitch black
		float offset = s * m_EyeAdaptAmbient * m_EyeAdaptAmbient * cfg.NightBlackLevel;

		// Blue shift: keep blue, pull red down fully and green halfway
		float shift = rodVision * cfg.BlueShift;
		vector tint = Vector(1 - shift, 1 - shift * 0.5, 1);

		// Glare blowout: hard overexposure, white-out and lost detail on top
		float ev = GetEyeAdaptEVBoost() + m_EyeAdaptGlare * cfg.GlareEvBoost;
		contrast -= m_EyeAdaptGlare * cfg.GlareContrastLoss;
		float overlay = m_EyeAdaptGlare * cfg.GlareOverlay;

		// Optional flashlight dazzle: flashbang-like white-out on top
		ev += m_EyeAdaptDazzle * cfg.DazzleEvBoost;
		overlay = Math.Max(overlay, m_EyeAdaptDazzle * cfg.DazzleOverlay);

		// Optional film grain: rod vision only (none near light, none in daylight)
		float grain = rodVision * cfg.NightGrain;

		m_EyeAdaptPPE.Apply(ev, saturation, contrast, tint, overlay, grain, offset);
	}

	// Glare in two phases (CLAUDE.md §3.4), both only for dark-adapted eyes facing light in the view centre:
	// 1. Blowout: starts when the adaptation lags far enough behind the light (GlareTriggerLevel).
	//    Fully blown out for GlareHoldSeconds, then fades over GlareFadeSeconds. Strength = the lag at the start,
	//    so fully adapted = full white-out, half adapted = half. Scaled by the shared glare recovery (no strobe,
	//    no repeat at full strength while walking along a beam).
	//    Uses the whole adaptation, not only the fast stage: the pupil alone closes within a second from any light
	//    nearby, which made the blowout depend on how quickly you turned around.
	// 2. Glow: how far the whole adaptation lags behind. Bloom halo that fades slowly (GlowFadeSeconds)
	//    while the eyes adapt.
	protected void UpdateEyeAdaptGlare(float timeslice)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		float flashTarget = 0;
		float glowTarget = 0;
		if (!m_EyeAdaptBypass && cfg.Enabled && !m_EyeAdaptNightVision)
		{
			float lightLevel = 1 - m_EyeAdaptDose.GetGlareDose();
			float scale = cfg.GlareStrength * m_EyeAdaptNight;
			glowTarget = Math.Clamp(m_EyeAdapt.Get() - lightLevel, 0, 1) * scale;
			flashTarget = glowTarget * GetEyeAdaptGlareRecovery();
		}
		if (m_EyeAdaptGlareTest)
		{
			flashTarget = cfg.GlareStrength;
			glowTarget = cfg.GlareStrength;
		}

		// Shared recovery clock: runs while neither a blowout nor a dazzle is running
		if (m_EyeAdaptGlareTime < 0 && m_EyeAdaptDazzleTime < 0)
			m_EyeAdaptSinceGlare += timeslice;

		// Blowout envelope: hold, fade
		if (m_EyeAdaptGlareTime >= 0)
		{
			m_EyeAdaptGlareTime += timeslice;
			float fadeTime = m_EyeAdaptGlareTime - cfg.GlareHoldSeconds;
			if (fadeTime <= 0)
			{
				m_EyeAdaptGlare = m_EyeAdaptGlareLevel;
			}
			else if (cfg.GlareFadeSeconds > 0 && fadeTime < cfg.GlareFadeSeconds)
			{
				m_EyeAdaptGlare = m_EyeAdaptGlareLevel * (1 - fadeTime / cfg.GlareFadeSeconds);
			}
			else
			{
				m_EyeAdaptGlare = 0;
				m_EyeAdaptGlareTime = -1;
			}
		}
		else if (flashTarget >= cfg.GlareTriggerLevel)
		{
			m_EyeAdaptGlareTime = 0;
			m_EyeAdaptSinceGlare = 0;
			m_EyeAdaptGlareLevel = Math.Clamp(flashTarget, 0, 1);
			m_EyeAdaptGlare = m_EyeAdaptGlareLevel;
		}

		// Glow attack smoothing
		float k = 1;
		if (cfg.GlareSmoothSeconds > 0)
			k = Math.Clamp(timeslice / cfg.GlareSmoothSeconds, 0, 1);

		// Glow: quick in, slow out (~95 % gone after GlowFadeSeconds)
		if (glowTarget >= m_EyeAdaptGlow)
			m_EyeAdaptGlow += (glowTarget - m_EyeAdaptGlow) * k;
		else if (cfg.GlowFadeSeconds > 0)
			m_EyeAdaptGlow += (glowTarget - m_EyeAdaptGlow) * Math.Clamp(3 * timeslice / cfg.GlowFadeSeconds, 0, 1);
		else
			m_EyeAdaptGlow = glowTarget;

		m_EyeAdaptGlarePeak = Math.Max(m_EyeAdaptGlarePeak, m_EyeAdaptGlare);

		// Optional soft look of rod vision (bloom, no blur), off with night vision. Same rod vision as the desaturation
		float soft = 0;
		if (!m_EyeAdaptNightVision)
			soft = GetEyeAdaptStrength() * (1 - m_EyeAdaptColorDose) * cfg.NightSoftness;

		// Halo around bright light and the soft look, set on the glow material directly (see EyeAdaptBloom)
		EyeAdaptBloom.Apply(m_EyeAdaptGlow, soft);
	}

	// By day with no blowout, glow or dazzle still fading out (and no glare test), the glare updates would only compute zeros
	protected bool IsEyeAdaptGlareIdle()
	{
		// Enforce can't continue an expression on a line starting with &&, so one check per line
		if (m_EyeAdaptNight > 0 || m_EyeAdaptGlareTest)
			return false;
		if (m_EyeAdaptGlareTime >= 0 || m_EyeAdaptGlare > 0 || m_EyeAdaptGlow > 0.001)
			return false;
		return m_EyeAdaptDazzleTime < 0 && m_EyeAdaptDazzle <= 0;
	}

	// Shared by blowout and dazzle: 0 right after one ended, 1 after GlareRecoverySeconds (linear)
	protected float GetEyeAdaptGlareRecovery()
	{
		float seconds = EyeAdaptSettings.Get().GlareRecoverySeconds;
		if (seconds <= 0)
			return 1;

		return Math.Clamp(m_EyeAdaptSinceGlare / seconds, 0, 1);
	}

	// Optional (server setting FlashlightDazzle): someone else's spot light in the face at night acts like a short
	// flashbang white-out. Full for DazzleHoldSeconds, fades over DazzleFadeSeconds.
	// The next dazzle also needs the beam out of the face for DazzleRearmSeconds, so a lamp held on you dazzles only once.
	// Strength = adaptation × shared glare recovery at the start, none below DazzleMinAdaptation
	protected void UpdateEyeAdaptDazzle(float timeslice)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		bool beamInFace = m_EyeAdaptDose.GetDazzleLight() >= cfg.DazzleThreshold;
		if (!m_EyeAdaptDazzleArmed)
		{
			if (beamInFace)
				m_EyeAdaptDazzleClear = 0;
			else
				m_EyeAdaptDazzleClear += timeslice;

			if (m_EyeAdaptDazzleClear >= cfg.DazzleRearmSeconds)
				m_EyeAdaptDazzleArmed = true;
		}

		if (m_EyeAdaptDazzleTime >= 0)
		{
			m_EyeAdaptDazzleTime += timeslice;
			float fadeTime = m_EyeAdaptDazzleTime - cfg.DazzleHoldSeconds;
			if (fadeTime <= 0)
			{
				m_EyeAdaptDazzle = m_EyeAdaptDazzleLevel;
			}
			else if (cfg.DazzleFadeSeconds > 0 && fadeTime < cfg.DazzleFadeSeconds)
			{
				m_EyeAdaptDazzle = m_EyeAdaptDazzleLevel * (1 - fadeTime / cfg.DazzleFadeSeconds);
			}
			else
			{
				m_EyeAdaptDazzle = 0;
				m_EyeAdaptDazzleTime = -1;
			}
			return;
		}

		m_EyeAdaptDazzle = 0;
		if (!cfg.FlashlightDazzle || !cfg.Enabled || m_EyeAdaptBypass || m_EyeAdaptNightVision)
			return;
		if (m_EyeAdaptNight < 0.5 || !m_EyeAdaptDazzleArmed)
			return;

		// Only dark-adapted eyes get blinded, and the more adapted, the harder (CLAUDE.md §3.4).
		// Weaker shortly after the last blowout/dazzle (shared recovery)
		float strength = m_EyeAdapt.Get() * GetEyeAdaptGlareRecovery();
		if (beamInFace && strength >= cfg.DazzleMinAdaptation)
		{
			m_EyeAdaptDazzleTime = 0;
			m_EyeAdaptSinceGlare = 0;
			m_EyeAdaptDazzleLevel = strength;
			m_EyeAdaptDazzle = strength;
			m_EyeAdaptDazzleArmed = false;
			m_EyeAdaptDazzleClear = 0;
			if (EyeAdaptSettings.Get().DebugLogging)
				Print(string.Format("[EyeAdapt] dazzled by a flashlight (beam %1, strength %2)", m_EyeAdaptDose.GetDazzleLight(), strength));
		}
	}

	override void OnMissionFinish()
	{
		// Material changes outlive the mission, so give the glow material its own bloom back
		EyeAdaptBloom.Reset();
		if (m_EyeAdaptText)
		{
			m_EyeAdaptText.Unlink();
			m_EyeAdaptText = null;
		}
		super.OnMissionFinish();
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

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_GLARE_TEST))
		{
			m_EyeAdaptGlareTest = !m_EyeAdaptGlareTest;
			if (m_EyeAdaptGlareTest)
				player.MessageImportant("[EyeAdapt] glare test ON (full glare)");
			else
				player.MessageImportant("[EyeAdapt] glare test OFF");
		}

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_CAR_LIGHTS))
		{
			CarScript car = FindNearestEyeAdaptCar(player);
			if (!car)
				player.MessageImportant(string.Format("[EyeAdapt] no car within %1 m", EyeAdaptConfig.CAR_LIGHTS_SEARCH_RADIUS));
			else if (car.EyeAdapt_ToggleTestHeadlights())
				player.MessageImportant("[EyeAdapt] test headlights ON: " + car.GetType());
			else
				player.MessageImportant("[EyeAdapt] test headlights OFF: " + car.GetType());
		}

		if (EyeAdaptKeyPressed(EyeAdaptConfig.KEY_LIGHTS))
		{
			m_EyeAdaptDose.Scan(true);
			player.MessageImportant(string.Format("[EyeAdapt] dose %1, strongest %2 - details in script log", m_EyeAdaptDose.Get(), m_EyeAdaptDose.GetStrongestName()));
		}
	}

	// Debug only: one world query on a key press, so the radius cost doesn't matter
	protected CarScript FindNearestEyeAdaptCar(PlayerBase player)
	{
		array<Object> objects = new array<Object>();
		array<CargoBase> proxies = new array<CargoBase>();
		g_Game.GetObjectsAtPosition(player.GetPosition(), EyeAdaptConfig.CAR_LIGHTS_SEARCH_RADIUS, objects, proxies);

		CarScript nearest;
		float nearestDist = float.MAX;
		foreach (Object obj : objects)
		{
			CarScript car = CarScript.Cast(obj);
			if (!car)
				continue;

			float dist = vector.Distance(player.GetPosition(), car.GetPosition());
			if (dist < nearestDist)
			{
				nearestDist = dist;
				nearest = car;
			}
		}
		return nearest;
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
