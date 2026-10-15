// 3_Game: bloom boost for the glare, set directly on the glow material instead of through PPE.
// PPEGlow registers BloomIntensity/BloomThreshold with defaults that don't match glow.emat (1 vs 0.75, docs/engine/ppe.md §2.2),
// so going through PPE would leave the wrong bloom behind.
// In game: the values are written every frame while glaring. Material.ResetParam must NOT be used: after a reset the
// engine ignored further SetParam calls for that parameter (glow worked once, then never again). So when the glare ends
// the configured base values are written back instead (vanilla glow.emat 0.75 / 0.5, admins with a lighting mod can
// set BloomBaseIntensity/BloomBaseThreshold to that mod's values).
class EyeAdaptBloom
{

	protected static Material s_Material;
	protected static bool s_Active;

	// Soft look (NightSoftness 1): bloom raised by this much, threshold lowered by this much
	static const float SOFT_BLOOM_INTENSITY = 1.0;
	static const float SOFT_BLOOM_THRESHOLD_DROP = 0.2;

	// glare 0..1, soft 0..1 (optional night softness). Both 0 writes the base bloom back
	static void Apply(float glare, float soft = 0)
	{
		// Use the exact material object the PPE manager renders with (a path spelled differently may give another instance)
		// (Material is a native handle: vanilla checks it with == 0x0)
		if (s_Material == 0x0)
		{
			s_Material = PPEManagerStatic.GetPPEManager().EyeAdapt_GetMaterial(PostProcessEffectType.Glow);
			if (EyeAdaptSettings.Get().DebugLogging)
				Print(string.Format("[EyeAdapt] bloom: glow material found %1", s_Material != 0x0));
		}
		if (s_Material == 0x0)
			return;

		if (glare <= 0.005 && soft <= 0.005)
		{
			Reset();
			return;
		}

		// No getter on Material, so the glare and the soft look are added to configured base values (vanilla glow.emat: 0.75 / 0.5)
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		float intensity = Math.Clamp(cfg.BloomBaseIntensity + glare * cfg.GlareBloomIntensity + soft * SOFT_BLOOM_INTENSITY, 0, 4);
		float threshold = Math.Clamp(cfg.BloomBaseThreshold - glare * cfg.GlareBloomThresholdDrop - soft * SOFT_BLOOM_THRESHOLD_DROP, 0, 1);

		// Written every frame while glaring (cheap, and safe if the engine refreshes the glow pass itself)
		bool first = !s_Active;
		bool okIntensity = s_Material.SetParam("BloomIntensity", intensity);
		bool okThreshold = s_Material.SetParam("BloomThreshold", threshold);
		if (first && EyeAdaptSettings.Get().DebugLogging)
			Print(string.Format("[EyeAdapt] bloom on: intensity %1 threshold %2 (SetParam ok %3 / %4)", intensity, threshold, okIntensity, okThreshold));
		s_Active = true;
	}

	// Glare over: write the base values back (no ResetParam, see top)
	static void Reset()
	{
		if (!s_Active)
			return;

		if (s_Material != 0x0)
		{
			EyeAdaptSettings cfg = EyeAdaptSettings.Get();
			s_Material.SetParam("BloomIntensity", cfg.BloomBaseIntensity);
			s_Material.SetParam("BloomThreshold", cfg.BloomBaseThreshold);
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
