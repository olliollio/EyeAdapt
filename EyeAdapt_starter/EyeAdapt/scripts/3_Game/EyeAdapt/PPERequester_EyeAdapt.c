// 3_Game: our post-processing requester. Must live in 3_Game because the
// registration hook (PPERequesterRegistrations) lives here. See docs/engine/ppe.md.
class PPERequester_EyeAdapt extends PPERequester_GameplayBase
{
	// Own priority layers, chosen to sit next to the vanilla users of each parameter (docs/engine/ppe.md §6)
	static const int L_EV = 450;        // exposure: above NVG (100/200) and flashbang (400), below burlap sack (500) and death (1000)
	static const int L_SAT = 150;       // saturation: above blood loss (100), LOWEST so we never re-saturate a bleeding player
	static const int L_CONTRAST = 100;  // contrast: unused by vanilla
	static const int L_TINT = 250;      // colorization: between toxic tint (200) and heavy metal poisoning (300)
	static const int L_OVERLAY_F = 350; // overlay factor: between flashbang (300) and shock (500), HIGHEST
	static const int L_OVERLAY_C = 550; // overlay colour: above flashbang (500). We yield while a flashbang runs anyway

	// Our values would replace or fight the NVG and flashbang look, so step aside while they run
	protected const ref array<typename> YIELD_TO = {
		PPERequester_CameraNV,
		PPERequester_FlashbangEffects
	};

	static const vector TINT_NEUTRAL = "1 1 1";

	// Last applied colours; colour setters always resend, so skip unchanged values ourselves
	protected vector m_LastTint = TINT_NEUTRAL;
	protected float m_LastOverlay;

	// ev: extra exposure in EV stops. saturation: 1 = vanilla, 0 = grey.
	// contrast: 1 = vanilla (range 0..2). tint: colour multiplier, TINT_NEUTRAL = vanilla.
	// overlay: white haze for glare, 0 = none.
	// Neutral values release the parameter, so the vanilla look is untouched.
	void Apply(float ev, float saturation, float contrast, vector tint, float overlay = 0)
	{
		if (PPEManagerStatic.GetPPEManager().IsAnyRequesterRunning(YIELD_TO))
		{
			ev = 0;
			saturation = 1;
			contrast = 1;
			tint = TINT_NEUTRAL;
			overlay = 0;
		}

		if (ev <= 0.001)
			SetTargetValueFloatDefault(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY);
		else
			SetTargetValueFloat(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY, false, ev, L_EV, PPOperators.SET);

		if (saturation >= 0.999)
			SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION);
		else
			SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_SATURATION, true, saturation, L_SAT, PPOperators.LOWEST);

		if (Math.AbsFloat(contrast - 1) <= 0.001)
			SetTargetValueFloatDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_CONTRAST);
		else
			SetTargetValueFloat(PostProcessEffectType.Glow, PPEGlow.PARAM_CONTRAST, false, contrast, L_CONTRAST, PPOperators.SET);

		if (tint != m_LastTint)
		{
			m_LastTint = tint;
			if (tint == TINT_NEUTRAL)
				SetTargetValueColorDefault(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR);
			else
				SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_COLORIZATIONCOLOR, {tint[0], tint[1], tint[2], 0.0}, L_TINT, PPOperators.MULTIPLICATIVE);
		}

		// Same building blocks as the flashbang white-out, just much softer: factor + white colour with alpha
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
				SetTargetValueColor(PostProcessEffectType.Glow, PPEGlow.PARAM_OVERLAYCOLOR, {1.0, 1.0, 1.0, overlay}, L_OVERLAY_C, PPOperators.SET);
			}
		}
	}

	// Vanilla Stop() releases all our values, so the cached tint must be forgotten too
	override protected void OnStop(Param par = null)
	{
		super.OnStop(par);
		m_LastTint = TINT_NEUTRAL;
		m_LastOverlay = 0;
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
