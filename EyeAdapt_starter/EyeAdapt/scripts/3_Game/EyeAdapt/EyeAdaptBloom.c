// 3_Game: bloom boost for the glare, set directly on the glow material instead of through PPE.
// PPEGlow registers BloomIntensity/BloomThreshold with defaults that don't match glow.emat (1 vs 0.75, docs/engine/ppe.md §2.2),
// so releasing them through PPE would leave the wrong bloom behind. Material.ResetParam restores the material's own value
// instead, which also keeps lighting mods that edit glow.emat intact.
class EyeAdaptBloom
{

	protected static Material s_Material;
	protected static bool s_Active;

	// glare 0..1. 0 restores the material's own bloom
	static void Apply(float glare)
	{
		// Use the exact material object the PPE manager renders with (a path spelled differently may give another instance)
		// (Material is a native handle: vanilla checks it with == 0x0)
		if (s_Material == 0x0)
		{
			s_Material = PPEManagerStatic.GetPPEManager().EyeAdapt_GetMaterial(PostProcessEffectType.Glow);
			Print(string.Format("[EyeAdapt] bloom: glow material found %1", s_Material != 0x0));
		}
		if (s_Material == 0x0)
			return;

		if (glare <= 0.005)
		{
			Reset();
			return;
		}

		// No getter on Material, so the glare is added to configured base values (vanilla glow.emat: 0.75 / 0.5)
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		float intensity = Math.Clamp(cfg.BloomBaseIntensity + glare * cfg.GlareBloomIntensity, 0, 4);
		float threshold = Math.Clamp(cfg.BloomBaseThreshold - glare * cfg.GlareBloomThresholdDrop, 0, 1);

		// Written every frame: a one-time SetParam was accepted (returns true) but had no visible effect,
		// so the engine probably refreshes the glow pass from its own values each frame
		bool first = !s_Active;
		bool okIntensity = s_Material.SetParam("BloomIntensity", intensity);
		bool okThreshold = s_Material.SetParam("BloomThreshold", threshold);
		if (first)
			Print(string.Format("[EyeAdapt] bloom on: intensity %1 threshold %2 (SetParam ok %3 / %4)", intensity, threshold, okIntensity, okThreshold));
		s_Active = true;
	}

	static void Reset()
	{
		if (!s_Active)
			return;

		if (s_Material != 0x0)
		{
			s_Material.ResetParam("BloomIntensity");
			s_Material.ResetParam("BloomThreshold");
		}
		s_Active = false;
	}
}

// Accessor for the material objects the PPE manager writes to (m_PPEClassMap is protected)
modded class PPEManager
{
	Material EyeAdapt_GetMaterial(int materialId)
	{
		Material mat;
		PPEClassBase matClass = m_PPEClassMap.Get(materialId);
		if (matClass)
			mat = matClass.GetMaterial();
		return mat;
	}
}
