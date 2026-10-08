// 3_Game: our post-processing requester. Must live in 3_Game because the
// registration hook (PPERequesterRegistrations) lives here. See docs/engine/ppe.md.
class PPERequester_EyeAdapt extends PPERequester_GameplayBase
{
	// Own priority layers, chosen to sit next to the vanilla users of each parameter (docs/engine/ppe.md §6)
	static const int L_EV = 450;        // exposure: above NVG (100/200) and flashbang (400), below burlap sack (500) and death (1000)
	static const int L_SAT = 150;       // saturation: above blood loss (100), LOWEST so we never re-saturate a bleeding player
	static const int L_CONTRAST = 100;  // contrast: unused by vanilla
	static const int L_TINT = 250;      // colorization: between toxic tint (200) and heavy metal poisoning (300)

	// Our values would replace or fight the NVG and flashbang look, so step aside while they run
	protected const ref array<typename> YIELD_TO = {
		PPERequester_CameraNV,
		PPERequester_FlashbangEffects
	};

	static const vector TINT_NEUTRAL = "1 1 1";

	// Last applied colour; colour setters always resend, so skip unchanged values ourselves
	protected vector m_LastTint = TINT_NEUTRAL;

	// ev: extra exposure in EV stops. saturation: 1 = vanilla, 0 = grey.
	// contrast: 1 = vanilla (range 0..2). tint: colour multiplier, TINT_NEUTRAL = vanilla.
	// Neutral values release the parameter, so the vanilla look is untouched.
	void Apply(float ev, float saturation, float contrast, vector tint)
	{
		if (PPEManagerStatic.GetPPEManager().IsAnyRequesterRunning(YIELD_TO))
		{
			ev = 0;
			saturation = 1;
			contrast = 1;
			tint = TINT_NEUTRAL;
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
	}

	// Vanilla Stop() releases all our values, so the cached tint must be forgotten too
	override protected void OnStop(Param par = null)
	{
		super.OnStop(par);
		m_LastTint = TINT_NEUTRAL;
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
