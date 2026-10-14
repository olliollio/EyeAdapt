// 4_World: registry of fireplaces that still smoulder. Vanilla fades the FireplaceLight out as soon as the fuel is gone
// (END_FIRE), so embers are only a particle effect and invisible to the light registry (ScriptedLightBase.c).
// The dose scan counts them as a weak light source, see EyeAdaptLightDose.CollectEmber.
modded class FireplaceBase
{
	static ref array<FireplaceBase> s_EyeAdaptFireplaces = new array<FireplaceBase>();

	override void EEInit()
	{
		super.EEInit();

		// Light and dose are client business
		#ifndef SERVER
		if (s_EyeAdaptFireplaces.Find(this) < 0)
			s_EyeAdaptFireplaces.Insert(this);
		#endif
	}

	// Fuel is burnt out but the fireplace is still hot (m_FireState is network-synced)
	bool EyeAdaptHasEmbers()
	{
		return !IsBurning() && m_FireState == FireplaceFireState.END_FIRE;
	}
}
