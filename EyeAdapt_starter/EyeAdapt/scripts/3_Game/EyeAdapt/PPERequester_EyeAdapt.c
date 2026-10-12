// 3_Game: our post-processing requester. Must live in 3_Game because the
// registration hook (PPERequesterRegistrations) lives here. See docs/engine/ppe.md.
class PPERequester_EyeAdapt extends PPERequester_GameplayBase
{
	// Own priority layers, chosen to sit next to the vanilla users of each parameter (docs/engine/ppe.md §6)
	static const int L_EV = 450;        // exposure: above NVG (100/200) and flashbang (400), below burlap sack (500) and death (1000)
	static const int L_SAT = 150;       // saturation: above blood loss (100), LOWEST so we never re-saturate a bleeding player
	static const int L_CONTRAST = 100;  // contrast: unused by vanilla
	static const int L_OFFSET = 100;    // colour offset (black point): unused by vanilla
	static const int L_TINT = 250;      // colorization: between toxic tint (200) and heavy metal poisoning (300)
	static const int L_OVERLAY_F = 350; // overlay factor: between flashbang (300) and shock (500), HIGHEST
	static const int L_OVERLAY_C = 550; // overlay colour: above flashbang (500). We yield while a flashbang runs anyway
	static const int L_GRAIN = 300;     // film grain: above NVG (100) and toxic tint (200). We yield to NVG anyway
	static const int L_NOISE = 150;     // NV noise multiplier (needed for grain to show): HIGHEST, so NVG's own value wins
	static const float NOISE_PER_GRAIN = 2.0; // noise multiplier at grain 1 (vanilla NV optics use 2)

	// Our values would replace or fight the NVG and flashbang look, so step aside while they run
	protected const ref array<typename> YIELD_TO = {
		PPERequester_CameraNV,
		PPERequester_FlashbangEffects
	};

	static const vector TINT_NEUTRAL = "1 1 1";

	// Last applied colours; colour setters always resend, so skip unchanged values ourselves
	protected float m_LastEv;
	protected float m_LastSaturation = 1;
	protected float m_LastContrast = 1;
	protected vector m_LastTint = TINT_NEUTRAL;
	protected float m_LastOverlay;
	protected float m_LastGrain;

	// ev: extra exposure in EV stops. saturation: 1 = vanilla, 0 = grey.
	// contrast: 1 = vanilla (range 0..2). tint: colour multiplier, TINT_NEUTRAL = vanilla.
	// overlay: white haze for glare, 0 = none. grain: film grain intensity 0..1, 0 = none.
	// offset: black point shift after the tonemap, 0 = vanilla, negative = darker blacks.
	// Neutral values release the parameter, so the vanilla look is untouched.
	void Apply(float ev, float saturation, float contrast, vector tint, float overlay = 0, float grain = 0, float offset = 0)
	{
		if (PPEManagerStatic.GetPPEManager().IsAnyRequesterRunning(YIELD_TO))
		{
			ev = 0;
			saturation = 1;
			contrast = 1;
			tint = TINT_NEUTRAL;
			overlay = 0;
			grain = 0;
			offset = 0;
		}

		// Neutral values are normalised first, so "release" compares equal to "release". Only changed values are sent
		if (ev <= 0.001)
			ev = 0;
		if (ev != m_LastEv)
		{
			m_LastEv = ev;
			if (ev == 0)
				SetTargetValueFloatDefault(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY);
			else
				SetTargetValueFloat(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY, false, ev, L_EV, PPOperators.SET);
		}

		if (saturation >= 0.999)
			saturation = 1;
		if (saturation != m_LastSaturation)
		{
			m_LastSaturation = saturation;
			if (saturation == 1)
				SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION);
			else
				SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION, true, saturation, L_SAT, PPOperators.LOWEST);
		}

		if (Math.AbsFloat(contrast - 1) <= 0.001)
			contrast = 1;
		if (contrast != m_LastContrast)
		{
			m_LastContrast = contrast;
			if (contrast == 1)
				SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_CONTRAST);
			else
				SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_CONTRAST, false, contrast, L_CONTRAST, PPOperators.SET);
		}

		if (offset > -0.0005)
			SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_OFFSET);
		else
			SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_OFFSET, false, offset, L_OFFSET, PPOperators.SET);

		if (tint != m_LastTint)
		{
			m_LastTint = tint;
			if (tint == TINT_NEUTRAL)
				SetTargetValueColorDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR);
			else
				SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR, {tint[0], tint[1], tint[2], 0.0}, L_TINT, PPOperators.MULTIPLICATIVE);
		}

		// Same building blocks as the flashbang white-out: overlay factor + white overlay colour
		overlay = Math.Round(Math.Clamp(overlay, 0, 1) * 200) / 200;
		if (overlay != m_LastOverlay)
		{
			m_LastOverlay = overlay;
			if (overlay <= 0)
			{
				SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYFACTOR);
				SetTargetValueColorDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYCOLOR);
			}
			else
			{
				SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYFACTOR, true, overlay, L_OVERLAY_F, PPOperators.HIGHEST);
				// Alpha stays 1: factor and alpha multiply, so scaling both made 0.6 look like 0.36
				SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYCOLOR, {1.0, 1.0, 1.0, 1.0}, L_OVERLAY_C, PPOperators.SET);
			}
		}

		ApplyGrain(grain);
	}

	// Film grain only shows with a non-zero NV noise multiplier (vanilla comment in ppefilmgrain.c). Resent only when the value changes
	protected void ApplyGrain(float grain)
	{
		grain = Math.Round(Math.Clamp(grain, 0, 1) * 200) / 200;
		if (grain == m_LastGrain)
			return;

		m_LastGrain = grain;
		if (grain <= 0)
		{
			SetTargetValueFloatDefault(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_INTENSITY);
			SetTargetValueFloatDefault(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_GRAINSIZE);
			SetTargetValueFloatDefault(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_FREQUENCY);
			SetTargetValueFloatDefault(PPEExceptions.NVLIGHTPARAMS, PPELightIntensityParamsNative.PARAM_NOISE_MULT);
		}
		else
		{
			EyeAdaptSettings cfg = EyeAdaptSettings.Get();
			SetTargetValueFloat(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_INTENSITY, false, grain, L_GRAIN, PPOperators.HIGHEST);
			SetTargetValueFloat(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_GRAINSIZE, false, Math.Clamp(cfg.NightGrainSize, 1, 9), L_GRAIN, PPOperators.SET);
			SetTargetValueFloat(PostProcessEffectType.FilmGrain, PPEFilmGrain.PARAM_FREQUENCY, false, Math.Clamp(cfg.NightGrainFrequency, 1, 1000), L_GRAIN, PPOperators.SET);
			// The noise multiplier is what really drives the grain amount (vanilla NV: 1..2), so it scales with the grain too.
			// A constant value made the grain equally strong at every adaptation level
			SetTargetValueFloat(PPEExceptions.NVLIGHTPARAMS, PPELightIntensityParamsNative.PARAM_NOISE_MULT, false, grain * NOISE_PER_GRAIN, L_NOISE, PPOperators.HIGHEST);
		}
	}

	// Vanilla Stop() releases all our values, so the cached tint must be forgotten too
	override protected void OnStop(Param par = null)
	{
		super.OnStop(par);
		m_LastEv = 0;
		m_LastSaturation = 1;
		m_LastContrast = 1;
		m_LastTint = TINT_NEUTRAL;
		m_LastOverlay = 0;
		m_LastGrain = 0;
	}
}

modded class PPERequesterRegistrations
{
	override protected void RegisterAdditionalRequesters()
	{
		super.RegisterAdditionalRequesters();
		PPERequesterBank.RegisterRequester(PPERequester_EyeAdapt);
	}
}
