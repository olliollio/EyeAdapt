// 4_World: light exposure ("dose") from all registered lights. See CLAUDE.md §3.3 and docs/engine/lights.md §10.
// One light that passed the cheap checks of the scan (pass 1), waiting for its occlusion test (pass 2)
class EyeAdaptLightHit
{
	ScriptedLightBase Light;  // null for smouldering embers
	string Name;
	vector Position;
	Object Parent;            // ignored by the occlusion ray (the fireplace / the item the light sits on)
	float Brightness;
	float Radius;
	EyeAdaptLightEntry Entry;
	float Dose;       // before occlusion
	float Glare;      // before occlusion
	float Distance;
	bool IsSpot;
	bool IsOwnSpot;
}

// dose per light = intensity × distance falloff × view factor (× beam check for spot lights)
class EyeAdaptLightDose
{
	// Saturated total dose of the last scan, 0..1
	protected float m_Dose;
	// Same, but only light close to the view centre counts (drives the glare), 0..1
	protected float m_GlareDose;
	// Strongest single foreign spot light (someone else's flashlight/headtorch/headlights) in the face, raw, for the optional dazzle
	protected float m_DazzleLight;

	// Debug info of the last scan
	protected string m_StrongestName;
	protected float m_StrongestDose;
	protected int m_LightCount;

	float Get()
	{
		return m_Dose;
	}

	float GetGlareDose()
	{
		return m_GlareDose;
	}

	float GetDazzleLight()
	{
		return m_DazzleLight;
	}

	string GetStrongestName()
	{
		return m_StrongestName;
	}

	float GetStrongestDose()
	{
		return m_StrongestDose;
	}

	int GetLightCount()
	{
		return m_LightCount;
	}

	// Raw dose (sum before saturation, times DoseSensitivity) from which the saturated dose is >= 98 %.
	// More light changes nothing visible, so the remaining occlusion raycasts are skipped
	const float SATURATION_STOP = 3.9;

	// verbose: print every light that contributes (debug key)
	//
	// Two passes: 1. cheap checks for all registered lights (on, table, distance, view, beam, threshold), strongest first.
	// 2. occlusion raycasts in that order, until the dose is saturated or MaxOcclusionChecks is used up.
	// So a base with dozens of lamps costs at most MaxOcclusionChecks raycasts per scan, and the strongest lights are checked.
	void Scan(bool verbose = false)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		vector eyePos = g_Game.GetCurrentCameraPosition();
		vector eyeDir = g_Game.GetCurrentCameraDirection();
		Object player = g_Game.GetPlayer();

		float total = 0;
		float glareTotal = 0;
		m_DazzleLight = 0;
		m_StrongestName = "";
		m_StrongestDose = 0;

		// The car we sit in: its lights don't shine at us, they light up the road ahead (handled after the loop)
		CarScript ownCar = GetOwnCar(player);

		// Pass 1: cheap checks
		array<ref EyeAdaptLightHit> hits = new array<ref EyeAdaptLightHit>();
		array<ScriptedLightBase> lights = ScriptedLightBase.s_EyeAdaptLights;
		for (int i = lights.Count() - 1; i >= 0; i--)
		{
			ScriptedLightBase light = lights[i];
			if (!light)
			{
				lights.Remove(i);
				continue;
			}

			if (ownCar && light.GetAttachmentParent() == ownCar)
				continue;

			EyeAdaptLightHit hit = CollectLight(light, eyePos, eyeDir, player);
			if (hit)
				InsertByDose(hits, hit);
		}
		m_LightCount = lights.Count();

		// Smouldering fireplaces: no light entity in vanilla, counted as weak light sources
		array<FireplaceBase> fireplaces = FireplaceBase.s_EyeAdaptFireplaces;
		for (int j = fireplaces.Count() - 1; j >= 0; j--)
		{
			FireplaceBase fireplace = fireplaces[j];
			if (!fireplace)
			{
				fireplaces.Remove(j);
				continue;
			}

			EyeAdaptLightHit emberHit = CollectEmber(fireplace, eyePos, eyeDir);
			if (emberHit)
				InsertByDose(hits, emberHit);
		}

		// Pass 2: occlusion, strongest first
		int raycasts = 0;
		int skipped = 0;
		foreach (EyeAdaptLightHit h : hits)
		{
			bool foreignCandidate = h.IsSpot && !h.IsOwnSpot && h.Glare > 0;
			bool doseFull = total * cfg.DoseSensitivity >= SATURATION_STOP;
			bool glareFull = glareTotal * cfg.DoseSensitivity >= SATURATION_STOP;

			// Saturated: this light can't add anything visible (a foreign spot light still sets the dazzle value)
			if (doseFull && (glareFull || h.Glare <= 0) && !foreignCandidate)
			{
				skipped++;
				continue;
			}

			float d = h.Dose;
			float glare = h.Glare;
			string blocker = "";
			if (cfg.UseOcclusion && !h.IsOwnSpot)
			{
				float occlusion;
				if (cfg.MaxOcclusionChecks <= 0 || raycasts < cfg.MaxOcclusionChecks)
				{
					occlusion = GetOcclusionFactor(h.Position, h.Parent, eyePos, player, blocker);
					raycasts++;
				}
				else
				{
					// Over budget: assume blocked rather than spend another raycast
					occlusion = cfg.OccludedFactor;
					blocker = "(over MaxOcclusionChecks)";
				}
				d *= occlusion;
				glare *= occlusion;
				if (d < cfg.DoseThreshold)
				{
					d = 0;
					glare = 0;
				}
			}

			if (verbose)
			{
				string tableEntry = "none";
				if (h.Entry)
					tableEntry = string.Format("%1 x%2 %3", h.Entry.ClassName, h.Entry.Factor, h.Entry.Type);
				Print(string.Format("[EyeAdapt]   %1 | dose %2 | brightness %3 | radius %4 | spot %5 | own %6 | blocked by '%7' | table %8 | %9 m", h.Name, d, h.Brightness, h.Radius, h.IsSpot, h.IsOwnSpot, blocker, tableEntry, h.Distance));
			}

			if (d <= 0)
				continue;

			total += d;
			glareTotal += glare;
			if (h.IsSpot && !h.IsOwnSpot && glare > 0)
				m_DazzleLight = Math.Max(m_DazzleLight, glare);
			if (d > m_StrongestDose)
			{
				m_StrongestDose = d;
				m_StrongestName = h.Name;
			}
		}

		// Perception is roughly logarithmic: one light = full effect, more lights add less and less, never above 1
		m_Dose = 1 - Math.Pow(Math.EULER, -total * cfg.DoseSensitivity);
		m_GlareDose = 1 - Math.Pow(Math.EULER, -glareTotal * cfg.DoseSensitivity);

		// Driving with headlights on: looking at the lit road keeps the eyes partly light-adapted
		if (ownCar && ownCar.m_Headlight && ownCar.m_Headlight.IsEnabled())
		{
			m_Dose = Math.Max(m_Dose, cfg.OwnVehicleLightDose);
			if (verbose)
				Print(string.Format("[EyeAdapt]   own car %1 headlights on | dose at least %2", ownCar.GetType(), cfg.OwnVehicleLightDose));
		}

		if (verbose)
			Print(string.Format("[EyeAdapt] scan: %1 lights registered, %2 passed the cheap checks, %3 raycasts, %4 skipped (saturated) | raw dose %5 | saturated %6 | glare dose %7", m_LightCount, hits.Count(), raycasts, skipped, total, m_Dose, m_GlareDose));
	}

	// Keeps hits sorted by dose, strongest first (insertion sort: only a few lights get this far)
	protected void InsertByDose(array<ref EyeAdaptLightHit> hits, EyeAdaptLightHit hit)
	{
		int pos = hits.Count();
		hits.Insert(hit);
		while (pos > 0 && hits.Get(pos - 1).Dose < hit.Dose)
		{
			hits.Set(pos, hits.Get(pos - 1));
			pos--;
		}
		hits.Set(pos, hit);
	}

	// Pass 1 for one light: everything except occlusion. Null if the light doesn't count.
	// Dose and Glare are the values before occlusion. Glare = the same light weighted by how centred it is in view
	// (0 for own spot lights)
	protected EyeAdaptLightHit CollectLight(ScriptedLightBase light, vector eyePos, vector eyeDir, Object player)
	{
		if (!light.IsEnabled())
			return null;

		// Camera-following ambient fill, never an exposure source (not in the table, admins can't switch it on)
		if (light.IsInherited(PersonalLight))
			return null;

		// Per-class factor, red handling and range from the light table (cached per class)
		EyeAdaptLightEntry entry = EyeAdaptSettings.Get().FindLight(light.Type());
		float factor = 1;
		float range = EyeAdaptSettings.Get().ScanRadius;
		if (entry)
		{
			factor = entry.Factor;
			if (entry.Range > 0)
				range = entry.Range;
		}
		if (factor <= 0)
			return null;

		// Distance first: cheapest cull, most registered lights are far away.
		// The light's own radius is a natural cut-off (campfire 25 m, chemlight 7.5 m, headlights up to 100 m)
		vector m[4];
		light.GetTransform(m);
		vector toLight = m[3] - eyePos;
		float dist = toLight.Length();
		if (dist > Math.Min(range, light.m_Radius))
			return null;

		// Nominal brightness: the field, not GetBrightness() (that one is ~100 × flicker, lights.md §2)
		float intensity = light.m_Brightness * factor * GetColorFactor(light, entry) * GetEnclosureFactor(FireplaceBase.Cast(light.GetAttachmentParent()));
		if (intensity <= 0)
			return null;

		// Own spot light (flashlight, headtorch, weapon light): you light up the ground ahead instead of
		// looking into it, but the lit surroundings still cost night vision. Own point lights (torch,
		// chemlight, gas lamp) shine into your eyes too, so they count normally by distance like any other light.
		bool isSpot = light.GetLightType() == LightSourceType.SpotLight;
		bool isOwnSpot = isSpot && IsOwnLight(light, player);

		float d;
		float glare = 0;
		if (isOwnSpot)
		{
			d = intensity * EyeAdaptSettings.Get().OwnLightFactor;
		}
		else
		{
			vector dirToLight = toLight.Normalized();

			// Looking at it = full, behind you = VIEW_MIN_FACTOR (a fire behind you still lights up the scene)
			float facing = Math.Max(vector.Dot(eyeDir, dirToLight), 0);
			float view = Math.Lerp(EyeAdaptSettings.Get().ViewMinFactor, 1, facing);

			// Spot lights (flashlights, headlights) only count when the beam points at us.
			// Forward axis m[2] = beam direction (in-game: flashlight on the ground counts when looking into the beam)
			if (isSpot)
			{
				float beam = vector.Dot(m[2], dirToLight * -1);
				if (beam < Math.Cos(light.GetSpotLightAngle() * 0.5 * Math.DEG2RAD))
					return null;
			}

			float falloff = Math.Max(Math.Pow(dist, EyeAdaptSettings.Get().FalloffExponent), 1);
			d = intensity * view / falloff;

			// Glare only from light close to the view centre (cos^8: half strength at ~30 degrees off centre)
			glare = intensity * Math.Pow(facing, EyeAdaptSettings.Get().GlareViewPower) / falloff;
		}

		// Weak contributions count as zero, not "a little" (campfire 100 m away)
		if (d < EyeAdaptSettings.Get().DoseThreshold)
			return null;

		EyeAdaptLightHit hit = new EyeAdaptLightHit();
		hit.Light = light;
		hit.Name = light.ClassName();
		hit.Position = light.GetWorldPosition();
		hit.Parent = light.GetAttachmentParent();
		hit.Brightness = light.m_Brightness;
		hit.Radius = light.m_Radius;
		hit.Entry = entry;
		hit.Dose = d;
		hit.Glare = glare;
		hit.IsSpot = isSpot;
		hit.IsOwnSpot = isOwnSpot;
		hit.Distance = dist;
		return hit;
	}

	// Share of the light that gets out of an enclosed fire (barrel with holes, oven, indoor fireplace), 1 = open.
	// Looked up in the table by the class the light is attached to; the stone oven is a Fireplace in oven state and has the
	// row "FireplaceOven". The plain "FireplaceBase" row belongs to the embers and is not an enclosure.
	// No row (older config.json) = the defaults below.
	const float ENCLOSURE_BARREL = 0.5;
	const float ENCLOSURE_OVEN = 0.3;

	protected float GetEnclosureFactor(FireplaceBase fireplace)
	{
		if (!fireplace)
			return 1;

		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		EyeAdaptLightEntry entry;
		float fallback = 1;
		if (fireplace.IsOven())
		{
			entry = cfg.FindLightByName("FireplaceOven");
			fallback = ENCLOSURE_OVEN;
		}
		else
		{
			entry = cfg.FindLight(fireplace.Type());
			if (entry && entry.ClassName == "FireplaceBase")
				entry = null;

			if (fireplace.IsBarrelWithHoles() || fireplace.IsInherited(FireplaceFireBarrel))
				fallback = ENCLOSURE_BARREL;
			else if (fireplace.IsFireplaceIndoor() || fireplace.IsIndoorOven())
				fallback = ENCLOSURE_OVEN;
		}

		if (entry)
			return entry.Factor;
		return fallback;
	}

	// Pass 1 for a smouldering fireplace (fuel gone, still hot): the campfire's brightness times the table row
	// "FireplaceBase" (Factor, Range). No row = EMBER_FACTOR / EMBER_RANGE. Factor 0 switches embers off.
	// No glare, a glow on the ground doesn't dazzle. Same distance falloff, view factor and occlusion as lights.
	const float EMBER_FACTOR = 0.1;
	const float EMBER_RANGE = 8;
	const float EMBER_HEIGHT = 0.3; // light position above the fireplace origin (occlusion ray end)

	protected EyeAdaptLightHit CollectEmber(FireplaceBase fireplace, vector eyePos, vector eyeDir)
	{
		if (!fireplace.EyeAdaptHasEmbers())
			return null;

		float factor = EMBER_FACTOR;
		float range = EMBER_RANGE;
		EyeAdaptLightEntry entry = EyeAdaptSettings.Get().FindLight(FireplaceBase);
		if (entry)
		{
			factor = entry.Factor;
			if (entry.Range > 0)
				range = entry.Range;
		}
		if (factor <= 0)
			return null;

		vector pos = fireplace.GetPosition();
		pos[1] = pos[1] + EMBER_HEIGHT;
		vector toLight = pos - eyePos;
		float dist = toLight.Length();
		if (dist > range)
			return null;

		float intensity = FireplaceLight.m_FireplaceBrightness * factor * GetEnclosureFactor(fireplace);
		float facing = Math.Max(vector.Dot(eyeDir, toLight.Normalized()), 0);
		float view = Math.Lerp(EyeAdaptSettings.Get().ViewMinFactor, 1, facing);
		float falloff = Math.Max(Math.Pow(dist, EyeAdaptSettings.Get().FalloffExponent), 1);
		float d = intensity * view / falloff;
		if (d < EyeAdaptSettings.Get().DoseThreshold)
			return null;

		EyeAdaptLightHit hit = new EyeAdaptLightHit();
		hit.Entry = entry;
		hit.Name = "Embers:" + fireplace.GetType();
		hit.Position = pos;
		hit.Parent = fireplace;
		hit.Brightness = intensity;
		hit.Radius = range;
		hit.Dose = d;
		hit.Distance = dist;
		return hit;
	}

	// 1 = free line of sight eye -> light, lower = blocked. blocker gets the class of what's in the way ("" if free).
	// Walls/terrain block most of it (a fire behind a wall still lights up the surroundings a bit),
	// trees and bushes only part of it (light gets through the crown, and walking past trunks shouldn't flicker).
	// The ray stops OCCLUSION_END_OFFSET before the light so the fireplace/torch/lamp model itself can't block it,
	// and ignores our own player and the object the light is attached to.
	protected float GetOcclusionFactor(vector lightPos, Object lightParent, vector eyePos, Object player, out string blocker)
	{
		blocker = "";
		vector toLight = lightPos - eyePos;
		float dist = toLight.Length();
		if (dist <= EyeAdaptSettings.Get().OcclusionEndOffset)
			return 1;

		vector endPos = eyePos + toLight.Normalized() * (dist - EyeAdaptSettings.Get().OcclusionEndOffset);

		vector contactPos, contactDir;
		int contactComponent;
		set<Object> hits = new set<Object>();

		if (!DayZPhysics.RaycastRV(eyePos, endPos, contactPos, contactDir, contactComponent, hits, player, lightParent, false, false, ObjIntersectView))
			return GetGrassOcclusionFactor(eyePos, lightPos, blocker);

		// Hit without an object = terrain (hill between us and the light)
		if (hits.Count() == 0)
		{
			blocker = "terrain";
			return EyeAdaptSettings.Get().OccludedFactor;
		}

		Object hit = hits.Get(0);
		blocker = hit.GetType();
		if (hit.IsTree() || hit.IsBush())
			return EyeAdaptSettings.Get().VegetationOccludedFactor;

		return EyeAdaptSettings.Get().OccludedFactor;
	}

	// Grass has no geometry, so the free line of sight is sampled: where it runs lower above the ground than the
	// grass there is tall (a small grassy crest between you and a car), the light is partly hidden.
	// SurfaceY is cheap, the surface name is only read where the line is low enough to matter.
	protected float GetGrassOcclusionFactor(vector eyePos, vector lightPos, out string blocker)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		if (!cfg.UseGrassOcclusion || cfg.GrassSampleStep <= 0)
			return 1;

		// A light well above the grass is seen over it (cheap early exit, one height lookup)
		if (lightPos[1] - g_Game.SurfaceY(lightPos[0], lightPos[2]) > cfg.GrassLightMaxHeight)
			return 1;

		vector toLight = lightPos - eyePos;
		float dist = toLight.Length();
		float maxHeight = Math.Max(cfg.GrassHeight, cfg.TallGrassHeight);
		for (float s = cfg.GrassSkipNearEye; s < dist - cfg.GrassSkipNearLight; s += cfg.GrassSampleStep)
		{
			vector p = eyePos + toLight * (s / dist);
			float clearance = p[1] - g_Game.SurfaceY(p[0], p[2]);
			if (clearance >= maxHeight)
				continue;

			string surface;
			g_Game.SurfaceGetType(p[0], p[2], surface);
			string name = surface;
			name.ToLower();
			if (!name.Contains("grass"))
				continue;

			float height = cfg.GrassHeight;
			if (name.Contains("tall"))
				height = cfg.TallGrassHeight;

			if (clearance < height)
			{
				blocker = string.Format("grass %1 (%2 m above ground at %3 m)", surface, clearance, s);
				return cfg.GrassOccludedFactor;
			}
		}
		return 1;
	}

	// Red light barely bleaches rhodopsin, so it barely costs night vision (red chemlight, red headtorch). Flares are set to "full" in the table: too bright to stay cheap.
	// Table type "red" = always, "full" = never (flames: 1.3/0.35/0.15 is "redder" than the red chemlight 1.0/0.3/0.2,
	// but broadband light that does bleach the rods), "auto" or no entry = by the light's colour
	protected float GetColorFactor(ScriptedLightBase light, EyeAdaptLightEntry entry)
	{
		if (entry && entry.Type == "full")
			return 1;
		if (entry && entry.Type == "red")
			return EyeAdaptSettings.Get().RedLightFactor;

		vector c = light.GetDiffuseColorV();
		if (c[0] > 0 && c[1] < c[0] * 0.35 && c[2] < c[0] * 0.35)
			return EyeAdaptSettings.Get().RedLightFactor;

		return 1;
	}

	// Car the local player sits in (driver or passenger), null otherwise
	protected CarScript GetOwnCar(Object player)
	{
		Human human = Human.Cast(player);
		if (!human)
			return null;

		HumanCommandVehicle vehicleCommand = human.GetCommand_Vehicle();
		if (vehicleCommand)
			return CarScript.Cast(vehicleCommand.GetTransport());

		return CarScript.Cast(human.GetParent());
	}

	// Headtorch on our head, flashlight in our hands, weapon light on our gun.
	// Not reliable for point lights (the torch light sits on its flame particle), but those don't need it.
	protected bool IsOwnLight(ScriptedLightBase light, Object player)
	{
		if (!player)
			return false;

		// Native parent chain (headtorch lights are attached to the player bone, m_Parent is null there)
		IEntity parent = light.GetParent();
		while (parent)
		{
			if (parent == player)
				return true;
			parent = parent.GetParent();
		}

		// Script parent (flashlight/weapon light attached to an item we carry)
		EntityAI item = EntityAI.Cast(light.GetAttachmentParent());
		return item && item.GetHierarchyRootPlayer() == player;
	}
}
