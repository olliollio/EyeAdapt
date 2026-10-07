// 3_Game: our post-processing requester. Must live in 3_Game because the
// registration hook (PPERequesterRegistrations) lives here. See docs/engine/ppe.md.
class PPERequester_EyeAdapt extends PPERequester_GameplayBase
{
	// Own priority layer for exposure: above NVG (100/200) and flashbang (400),
	// below burlap sack (500) and death darkening (1000).
	static const int L_EV = 450;

	// Extra exposure in EV stops on top of the engine's own exposure. 0 releases it.
	void SetExposureBoost(float ev)
	{
		if (ev <= 0.001)
			SetTargetValueFloatDefault(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY);
		else
			SetTargetValueFloat(PPEExceptions.EXPOSURE, PPEExposureNative.PARAM_INTENSITY, false, ev, L_EV, PPOperators.SET);
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
