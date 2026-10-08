// 4_World: light exposure ("dose") from all registered lights. See CLAUDE.md §3.3 and docs/engine/lights.md §10.
// dose per light = intensity × distance falloff × view factor (× beam check for spot lights)
class EyeAdaptLightDose
{
	// Saturated total dose of the last scan, 0..1
	protected float m_Dose;

	// Debug info of the last scan
	protected string m_StrongestName;
	protected float m_StrongestDose;
	protected int m_LightCount;

	float Get()
	{
		return m_Dose;
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

	// verbose: print every light that contributes (debug key)
	void Scan(bool verbose = false)
	{
		vector eyePos = g_Game.GetCurrentCameraPosition();
		vector eyeDir = g_Game.GetCurrentCameraDirection();
		Object player = g_Game.GetPlayer();

		float total = 0;
		m_StrongestName = "";
		m_StrongestDose = 0;

		array<ScriptedLightBase> lights = ScriptedLightBase.s_EyeAdaptLights;
		for (int i = lights.Count() - 1; i >= 0; i--)
		{
			ScriptedLightBase light = lights[i];
			if (!light)
			{
				lights.Remove(i);
				continue;
			}

			float d = GetLightDose(light, eyePos, eyeDir, player, verbose);
			if (d <= 0)
				continue;

			total += d;
			if (d > m_StrongestDose)
			{
				m_StrongestDose = d;
				m_StrongestName = light.ClassName();
			}
		}
		m_LightCount = lights.Count();

		// Perception is roughly logarithmic: one light = full effect, more lights add less and less, never above 1
		m_Dose = 1 - Math.Pow(Math.EULER, -total * EyeAdaptConfig.DOSE_SENSITIVITY);

		if (verbose)
			Print(string.Format("[EyeAdapt] scan: %1 lights registered | raw dose %2 | saturated %3", m_LightCount, total, m_Dose));
	}

	protected float GetLightDose(ScriptedLightBase light, vector eyePos, vector eyeDir, Object player, bool verbose)
	{
		if (!light.IsEnabled())
			return 0;

		// Distance first: cheapest cull, most registered lights are far away.
		// The light's own radius is a natural cut-off (campfire 25 m, chemlight 7.5 m, headlights up to 100 m)
		vector m[4];
		light.GetTransform(m);
		vector toLight = m[3] - eyePos;
		float dist = toLight.Length();
		if (dist > Math.Min(EyeAdaptConfig.SCAN_RADIUS, light.m_Radius))
			return 0;

		// Camera-following ambient fill and 10-80 ms muzzle flashes are not exposure sources
		if (light.IsInherited(PersonalLight) || light.IsInherited(MuzzleFlashLight))
			return 0;

		// Nominal brightness: the field, not GetBrightness() (that one is ~100 × flicker, lights.md §2)
		float intensity = light.m_Brightness * GetColorFactor(light);
		if (intensity <= 0)
			return 0;

		// Own spot light (flashlight, headtorch, weapon light): you light up the ground ahead instead of
		// looking into it, but the lit surroundings still cost night vision. Own point lights (torch,
		// chemlight, gas lamp) shine into your eyes too, so they count normally by distance like any other light.
		bool isSpot = light.GetLightType() == LightSourceType.SpotLight;
		bool isOwnSpot = isSpot && IsOwnLight(light, player);

		float d;
		if (isOwnSpot)
		{
			d = intensity * EyeAdaptConfig.OWN_LIGHT_FACTOR;
		}
		else
		{
			vector dirToLight = toLight.Normalized();

			// Looking at it = full, behind you = VIEW_MIN_FACTOR (a fire behind you still lights up the scene)
			float view = Math.Max(vector.Dot(eyeDir, dirToLight), 0);
			view = Math.Lerp(EyeAdaptConfig.VIEW_MIN_FACTOR, 1, view);

			// Spot lights (flashlights, headlights) only count when the beam points at us.
			// Forward axis m[2] = beam direction (in-game: flashlight on the ground counts when looking into the beam)
			if (isSpot)
			{
				float beam = vector.Dot(m[2], dirToLight * -1);
				if (beam < Math.Cos(light.GetSpotLightAngle() * 0.5 * Math.DEG2RAD))
					return 0;
			}

			d = intensity * view / Math.Max(Math.Pow(dist, EyeAdaptConfig.FALLOFF_EXPONENT), 1);
		}

		// Weak contributions count as zero, not "a little" (campfire 100 m away)
		if (d < EyeAdaptConfig.DOSE_THRESHOLD)
			return 0;

		// Occlusion last: raycasts are the expensive part, so only for lights that would count.
		// Own spot lights are never occluded (they light up what's right in front of us).
		string blocker = "";
		if (EyeAdaptConfig.USE_OCCLUSION && !isOwnSpot)
		{
			d *= GetOcclusionFactor(light, eyePos, player, blocker);
			if (d < EyeAdaptConfig.DOSE_THRESHOLD)
				d = 0;
		}

		if (verbose)
			Print(string.Format("[EyeAdapt]   %1 | dose %2 | brightness %3 | radius %4 | spot %5 | own %6 | blocked by '%7'", light.ClassName(), d, light.m_Brightness, light.m_Radius, isSpot, IsOwnLight(light, player), blocker));

		return d;
	}

	// 1 = free line of sight eye -> light, lower = blocked. blocker gets the class of what's in the way ("" if free).
	// Walls/terrain block most of it (a fire behind a wall still lights up the surroundings a bit),
	// trees and bushes only part of it (light gets through the crown, and walking past trunks shouldn't flicker).
	// The ray stops OCCLUSION_END_OFFSET before the light so the fireplace/torch/lamp model itself can't block it,
	// and ignores our own player and the object the light is attached to.
	protected float GetOcclusionFactor(ScriptedLightBase light, vector eyePos, Object player, out string blocker)
	{
		blocker = "";
		vector lightPos = light.GetWorldPosition();
		vector toLight = lightPos - eyePos;
		float dist = toLight.Length();
		if (dist <= EyeAdaptConfig.OCCLUSION_END_OFFSET)
			return 1;

		vector endPos = eyePos + toLight.Normalized() * (dist - EyeAdaptConfig.OCCLUSION_END_OFFSET);

		vector contactPos, contactDir;
		int contactComponent;
		set<Object> hits = new set<Object>();
		Object lightParent = light.GetAttachmentParent();

		if (!DayZPhysics.RaycastRV(eyePos, endPos, contactPos, contactDir, contactComponent, hits, player, lightParent, false, false, ObjIntersectView))
			return 1;

		// Hit without an object = terrain (hill between us and the light)
		if (hits.Count() == 0)
		{
			blocker = "terrain";
			return EyeAdaptConfig.OCCLUDED_FACTOR;
		}

		Object hit = hits.Get(0);
		blocker = hit.GetType();
		if (hit.IsTree() || hit.IsBush())
			return EyeAdaptConfig.VEGETATION_OCCLUDED_FACTOR;

		return EyeAdaptConfig.OCCLUDED_FACTOR;
	}

	// Flames look orange-red in RGB (fireplace 1.3/0.35/0.15 is "redder" than the red chemlight 1.0/0.3/0.2),
	// but they are broadband light and do bleach the rods, so they never get the red discount
	protected const ref array<typename> FLAME_LIGHTS = {
		FireplaceLight,
		BonfireLight,
		TirePileLight,
		TorchLight,
		PortableGasLampLight
	};

	// Red light barely bleaches rhodopsin, so it barely costs night vision (red chemlight, red headtorch, road flare)
	protected float GetColorFactor(ScriptedLightBase light)
	{
		foreach (typename flame : FLAME_LIGHTS)
		{
			if (light.IsInherited(flame))
				return 1;
		}

		vector c = light.GetDiffuseColorV();
		if (c[0] > 0 && c[1] < c[0] * 0.35 && c[2] < c[0] * 0.35)
			return EyeAdaptConfig.RED_LIGHT_FACTOR;

		return 1;
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
