// 3_Game: server-side config file, synced to every client on connect (CLAUDE.md §6, docs/engine/mission-network.md §3-4).
// File: $profile:EyeAdapt/config.json on the server (= <server>/profiles/EyeAdapt/config.json).
// Field initializers are the defaults. JSON keys = field names, so keep fields public and don't rename them lightly.
// Missing keys keep their defaults. The file is only (re)written when it's missing or older than CURRENT_VERSION,
// so admins' hand-edited values stay as typed (DayZ writes floats as 0.30000001192092898 and bools as 1/0).
// Bump CURRENT_VERSION (and the Version default) whenever fields are added, so existing files get the new keys.
class EyeAdaptSettings
{
	static const int CURRENT_VERSION = 23;
	int Version = 23;
	bool Enabled = true;

	// Adaptation curve: fast stage (pupil/cones) + slow stage (rods), each ~95 % after its time.
	// How long full adaptation takes (slow stage):
	//   AdaptationMode "fixed":           FullAdaptSeconds
	//   AdaptationMode "relativeToNight": FullAdaptNightFraction of the night's real length, measured by the mod
	//                                     (covers serverTimeAcceleration and serverNightTimeAcceleration),
	//                                     clamped to AdaptMinSeconds..AdaptMaxSeconds. FullAdaptSeconds until measured
	string AdaptationMode = "fixed";
	float FullAdaptSeconds = 180.0;        // 3 min (decided 2026-10-10 for the first test release)
	float FullAdaptNightFraction = 0.2;    // share of the WHOLE night length (sunset to sunrise), not of the time left. 0.2 = 20 %
	float AdaptMinSeconds = 60.0;
	float AdaptMaxSeconds = 900.0;
	float FastCurveSeconds = 20.0;
	float FastCurveWeight = 0.3;           // share of the gain from the fast stage (0..1)
	float AdaptCurvePower = 1.5;           // visual strength = adaptation ^ power (1 = linear). >1: early minutes subtle, the late phase counts more
	float StartAdaptation = 0.2;           // fresh characters and players without a record since the last restart (dark start, like leaving a lit room)
	float LightFastSeconds = 2.0;          // fast stage drops this fast under light
	float LightSlowSeconds = 8.0;          // slow stage drops this fast under light (30 s left the scene overexposed at the fire for too long)
	float ReportIntervalSeconds = 15.0;    // client reports its adaptation to the server this often (kept until the next login)

	// Light exposure (dose)
	float ScanInterval = 0.25;             // seconds between light scans (0.5 made the glare start noticeably late)
	float ScanRadius = 40.0;               // also limited by each light's own radius
	float FalloffExponent = 1.3;           // 2 = physical inverse square, felt too short in game (campfire had no effect at 15 m).
	                                       // With 1.3, looking at a campfire: 2 m -> dose ~0.98, 5 m ~0.7, 10 m ~0.4, 15 m ~0.25, 25 m ~0.13
	float DoseThreshold = 0.02;            // contributions below this count as zero
	float DoseSensitivity = 2.0;           // saturation: dose = 1 - e^(-raw * sensitivity)
	float MaxDoseEffect = 1.0;             // how far a full dose pushes the target down
	float ViewMinFactor = 0.25;            // lights behind you still count this much
	float OwnLightFactor = 0.1;            // own spot light (flashlight/headtorch/weapon light). Own point lights (torch, chemlight) count by distance
	float RedLightFactor = 0.15;           // red light barely affects night vision (lights with Type "red", or "auto" and red colour)
	float NightVisionDose = 0.5;           // powered NV goggles (both eyes) count as this dose, 0 = free
	float NightVisionScopeDose = 0.25;     // powered NV scope (only the aiming eye), 0 = free
	float FlashbangAdaptationLoss = 1.0;   // share of the adaptation lost when a flash grenade hits you (1 = back to zero, 0 = off)
	float OwnVehicleLightDose = 0.5;      // sitting in a car with its headlights on (you look at the lit road): at least this dose, 0 = free

	// Light table, per light class (also matches subclasses, the most specific entry wins). Lights without an entry
	// count with factor 1, type "auto", range ScanRadius. Add modded lights here (Numpad 9 prints the class names).
	//   ClassName: script class of the light (ScriptedLightBase subclass)
	//   Factor:    brightness multiplier, 0 = ignore the light
	//   Type:      "auto" = red discount if the light colour is red, "red" = always, "full" = never (flames are
	//              orange-red in RGB but broadband light, they do bleach the rods)
	//   Range:     metres the light counts up to, 0 = ScanRadius. Always also limited by the light's own radius
	ref array<ref EyeAdaptLightEntry> Lights;
	bool UseOcclusion = true;
	int MaxOcclusionChecks = 8;            // raycasts per scan, strongest lights first (0 = no limit). Lights over the budget count as blocked
	float OccludedFactor = 0.15;           // light behind walls/terrain
	float VegetationOccludedFactor = 0.5;  // light behind trees/bushes
	float OcclusionEndOffset = 0.5;        // ray stops this far before the light
	// Grass is GPU clutter without geometry, raycasts pass through it. Instead the line eye -> light is sampled:
	// where it runs through grass (lower above the ground than the grass is tall), the light counts as partly hidden.
	// Surfaces count as grass when their name contains "grass" (all vanilla maps), "tall" in the name = tall grass
	bool UseGrassOcclusion = true;
	float GrassOccludedFactor = 0.3;       // light seen through grass
	float GrassHeight = 0.5;               // metres
	float TallGrassHeight = 1.0;
	float GrassSampleStep = 2.0;           // metres between samples along the line
	float GrassSkipNearEye = 1.0;          // grass at your own feet doesn't count
	float GrassSkipNearLight = 3.0;        // grass right at the light doesn't count (headlights always sit in it)

	// Night gate (hours relative to the map's sunrise/sunset)
	float DuskOffsetHours = 0.0;
	float DuskHours = 2.0;
	float DawnOffsetHours = -1.5;          // morning twilight is already bright 0.5 h before sunrise (full boost looked washed out in test)
	float DawnHours = 1.5;

	// Ambient light: the EV boost is scaled by moon/overcast/underground
	float MoonIntensityFull = 1.0;         // raw GetMoonIntensity() that counts as full moonlight (vanilla: 0..1)
	float StarlightShare = 0.6;            // clear moonless sky relative to clear full moon (0.4 looked nearly black in test)
	float OvercastClear = 0.35;
	float OvercastFull = 1.0;
	float OvercastMinShare = 0.25;         // fully overcast sky relative to clear
	float MinAmbientGain = 0.2;            // share of MaxEvBoost in the darkest sky
	float UndergroundGain = 0.0;
	float AmbientSmoothSeconds = 2.0;

	// Visuals at full adaptation
	float MaxEvBoost = 5.0;                // EV stops under a clear full moon
	float MaxDesaturation = 0.6;           // in the dark. Near light colour returns (cones take over), scaled down by the light dose
	float ColorReturnSmoothSeconds = 0.5;  // smoothing of the colour return (the light scan runs every ScanInterval)
	float MaxContrastBoost = 0.125;        // counters the grey/foggy look of a strong EV boost (0.15 left the dark sky too bright in test)
	float NightBlackLevel = -0.015;        // black point at full adaptation (colour offset, after the tonemap, 0 = off, 0..-0.1). The EV boost lifts the empty sky to grey, this pulls it back to black; stars and Milky Way are bright points and stay. Scaled like the contrast boost
	float BlueShift = 0.1;                 // Purkinje effect, 0 = off
	// Optional extras, off by default. Both scale with the rod vision (adaptation, less near light) and step aside for night vision
	float NightGrain = 0.0;                // film grain at full adaptation, 0..1 (0 = off). Dark-adapted vision is noisy; also hurts gamma/brightness filter abuse
	float NightGrainSize = 2.75;           // 1..9 (vanilla default 2.75)
	float NightGrainFrequency = 20.0;      // 1..1000, how fast the grain changes ("visual snow")
	float NightSoftness = 0.0;             // soft look at full adaptation, 0..1 (0 = off): rods see little detail. Done with bloom, not blur

	// Glare: dark-adapted eyes facing sudden bright light (CLAUDE.md §3.4), in two phases:
	// 1. blowout = fully blown out for GlareHoldSeconds, fades over GlareFadeSeconds: GlareEvBoost, GlareOverlay, GlareContrastLoss
	// 2. glow = bloom halo that fades over GlowFadeSeconds while the eyes adapt: GlareBloom*
	// Strength scales with how adapted you were. Values below are at full glare, scaled by GlareStrength
	float GlareStrength = 0.0;             // overall scale (0 = off). Off by default for the first test release (was 1.0)
	float GlareViewPower = 8.0;            // how centred the light must be: cos^power, 8 = half at ~30 degrees off centre
	float GlareBloomIntensity = 2.0;       // added to the bloom intensity (halo around bright light, engine max 4)
	float GlareBloomThresholdDrop = 0.3;   // bloom threshold lowered by this (more of the image blooms)
	float BloomBaseIntensity = 0.75;       // vanilla glow.emat values the boost is added to (no getter in the engine)
	float BloomBaseThreshold = 0.5;
	float GlareTriggerLevel = 0.3;         // blowout starts when the adaptation lags this far behind the light in view (0..1)
	float GlareHoldSeconds = 0.6;          // blowout: fully blown out ("hurts", you can't see much)
	float GlareFadeSeconds = 1.0;          // blowout: fade into the glow phase
	float GlareRecoverySeconds = 30.0;     // after a blowout or dazzle, the next one is weaker: 0 right after it ended,
	                                       // full again after this long (eyes just blinded are less sensitive). 0 = off
	float GlowFadeSeconds = 6.0;           // glow phase: halo ~95 % gone after this
	float GlareEvBoost = 6.0;              // blowout: extra EV stops
	float GlareContrastLoss = 0.3;         // blowout: contrast reduction (detail loss)
	float GlareOverlay = 1.0;              // blowout: white-out, linear (flashbang: 1.0)
	float GlareSmoothSeconds = 0.15;       // fade-in/out smoothing of the glare

	// Optional flashlight dazzle: someone else's flashlight/headtorch/headlights in your face at night acts like a
	// short flashbang white-out (no sound). On by default for the first test release (2026-10-10), server admins decide
	bool FlashlightDazzle = true;
	float DazzleThreshold = 0.3;           // how strong the beam in the face must be (flashlight centred: ~7 m with defaults)
	float DazzleHoldSeconds = 1.0;         // full white-out (vanilla flashbang: ~5.5 s)
	float DazzleFadeSeconds = 2.0;         // fade back to normal (vanilla flashbang: ~2.5 s)
	float DazzleRearmSeconds = 2.0;        // after a dazzle the beam must be out of your face this long before the next one
	                                       // (a lamp held on you dazzles once, your eyes have adjusted to it)
	float DazzleMinAdaptation = 0.3;       // no dazzle below this strength. Strength = adaptation × glare recovery
	                                       // (fully dark-adapted = full white-out, light-adapted eyes barely notice)
	float DazzleOverlay = 0.9;             // white-out strength (vanilla flashbang: 1.0)
	float DazzleEvBoost = 5.0;             // extra EV stops at full dazzle

	// Numpad debug keys (set adaptation, toggle effect, print lights). Lets players cheat, switch off on live servers
	bool DebugKeys = true;
	bool DebugLogging = false;             // client script log: the two [EyeAdapt] status lines every 5 s and event lines (dazzle). Off = quiet log
	bool ShowAdaptation = false;           // on-screen text with the current adaptation (NA), updated every second. For testing, players could use it to read their NA

	// Vanilla defaults for the light table (written to config.json when the key is missing)
	static array<ref EyeAdaptLightEntry> DefaultLights()
	{
		array<ref EyeAdaptLightEntry> lights = new array<ref EyeAdaptLightEntry>();
		// Muzzle flashes last 10-80 ms, not an exposure source (_1/_2 don't inherit from MuzzleFlashLight)
		lights.Insert(new EyeAdaptLightEntry("MuzzleFlashLight", 0));
		lights.Insert(new EyeAdaptLightEntry("MuzzleFlashLight_1", 0));
		lights.Insert(new EyeAdaptLightEntry("MuzzleFlashLight_2", 0));
		// Flames: never the red discount
		lights.Insert(new EyeAdaptLightEntry("FireplaceLight", 1, "full"));
		lights.Insert(new EyeAdaptLightEntry("BonfireLight", 1, "full"));
		lights.Insert(new EyeAdaptLightEntry("TirePileLight", 1, "full"));
		lights.Insert(new EyeAdaptLightEntry("TorchLight", 1, "full"));
		lights.Insert(new EyeAdaptLightEntry("PortableGasLampLight", 1, "full"));
		// Headlights: bright focused beam, glare dark-adapted eyes from ~50-70 m. Rear lights normal (red by colour)
		lights.Insert(new EyeAdaptLightEntry("CarLightBase", 5, "auto", 70));
		lights.Insert(new EyeAdaptLightEntry("CarRearLightBase", 1));
		// Flare gun flares light up a whole area from the sky
		lights.Insert(new EyeAdaptLightEntry("FlareLight", 10, "full", 100));
		// Road flare is red but bright and broadband, it still bleaches the rods
		lights.Insert(new EyeAdaptLightEntry("RoadflareLight", 1, "full"));
		// Fireworks: nominal brightness is only 0.05, but the colours are set as 0..255 (not 0..1), so the real light is
		// ~250 times stronger (x ~12 effective, like a flare) and flickers up to x3. Lives 2.1 s, so the scan sees it
		lights.Insert(new EyeAdaptLightEntry("ExplosionLight", 250, "full", 60));
		// Livonia bunker entrance and stair lights are very dim (brightness 0.5): x1 hardly moved the adaptation
		lights.Insert(new EyeAdaptLightEntry("EntranceLight", 4));
		// Generator spotlight (radius 60 m)
		lights.Insert(new EyeAdaptLightEntry("SpotlightLight", 1, "auto", 60));
		// Smouldering fireplaces (fuel gone, still hot): vanilla has no light entity for the embers, the dose scan counts
		// them with the campfire brightness times this Factor. Factor 0 = embers do not count
		lights.Insert(new EyeAdaptLightEntry("FireplaceBase", 0.1, "full", 8));
		// Enclosed fires: the table row is looked up by the class the light is attached to (not the light class), Factor = share
		// of the light that gets out. "FireplaceOven" is not a class: it stands for the stone oven (a Fireplace in oven state)
		lights.Insert(new EyeAdaptLightEntry("BarrelHoles_ColorBase", 0.5, "full"));
		lights.Insert(new EyeAdaptLightEntry("FireplaceFireBarrel", 0.5, "full"));
		lights.Insert(new EyeAdaptLightEntry("FireplaceIndoor", 0.3, "full"));
		lights.Insert(new EyeAdaptLightEntry("OvenIndoor", 0.3, "full"));
		lights.Insert(new EyeAdaptLightEntry("FireplaceOven", 0.3, "full"));
		return lights;
	}

	[NonSerialized()]
	protected ref map<string, int> m_LightIndex; // light class -> index in Lights, -1 = no entry

	// Table row with exactly this ClassName (for rows that are not a class, like "FireplaceOven"), null if none
	EyeAdaptLightEntry FindLightByName(string className)
	{
		if (!Lights)
			Lights = DefaultLights();

		foreach (EyeAdaptLightEntry entry : Lights)
		{
			if (entry && entry.ClassName == className)
				return entry;
		}
		return null;
	}

	// Most specific table entry for a light class, null if none. Cached per class
	EyeAdaptLightEntry FindLight(typename lightType)
	{
		if (!Lights)
			Lights = DefaultLights();
		if (!m_LightIndex)
			m_LightIndex = new map<string, int>();

		string key = lightType.ToString();
		int index;
		if (!m_LightIndex.Find(key, index))
		{
			index = -1;
			typename best;
			for (int i = 0; i < Lights.Count(); i++)
			{
				EyeAdaptLightEntry entry = Lights[i];
				if (!entry)
					continue;

				typename entryType = entry.ClassName.ToType();
				if (!entryType || !lightType.IsInherited(entryType))
					continue;

				// Later or more specific entries win (an admin's own entry for a subclass beats the base class)
				if (index < 0 || entryType.IsInherited(best))
				{
					index = i;
					best = entryType;
				}
			}
			m_LightIndex.Insert(key, index);
		}

		if (index < 0)
			return null;
		return Lights[index];
	}

	// ---- Static access ----

	// Effective full adaptation time in seconds, depending on AdaptationMode
	static float GetFullAdaptSeconds()
	{
		EyeAdaptSettings cfg = Get();
		if (cfg.AdaptationMode != "relativeToNight")
			return cfg.FullAdaptSeconds;

		float night = EyeAdaptNightClock.GetNightRealSeconds();
		if (night <= 0)
			return cfg.FullAdaptSeconds; // not measured yet

		return Math.Clamp(night * cfg.FullAdaptNightFraction, cfg.AdaptMinSeconds, cfg.AdaptMaxSeconds);
	}

	protected static ref EyeAdaptSettings s_Instance;

	// Current settings: synced from the server on a client, loaded from file on the server, defaults otherwise
	static EyeAdaptSettings Get()
	{
		if (!s_Instance)
			s_Instance = new EyeAdaptSettings();
		return s_Instance;
	}

	static void Set(EyeAdaptSettings settings)
	{
		if (settings)
			s_Instance = settings;
	}

	static const string DIR = "$profile:EyeAdapt";
	static const string PATH = "$profile:EyeAdapt/config.json";

	// Server: load the config file. Create it with defaults if missing, upgrade it if it's from an older version
	static void LoadServer()
	{
		if (!FileExist(DIR))
			MakeDirectory(DIR);

		EyeAdaptSettings settings = new EyeAdaptSettings();
		string error;
		bool write;
		if (FileExist(PATH))
		{
			if (!JsonFileLoader<EyeAdaptSettings>.LoadFile(PATH, settings, error))
			{
				// Keep the broken file for the admin to fix, run with defaults
				ErrorEx("[EyeAdapt] config.json could not be read, using defaults: " + error);
				s_Instance = new EyeAdaptSettings();
				return;
			}
			Print("[EyeAdapt] config loaded from " + PATH);

			if (settings.Version < CURRENT_VERSION)
			{
				Print(string.Format("[EyeAdapt] config version %1 -> %2, adding new keys", settings.Version, CURRENT_VERSION));
				settings.Version = CURRENT_VERSION;
				write = true;
			}

			if (!settings.Lights)
			{
				settings.Lights = DefaultLights();
				write = true;
			}
		}
		else
		{
			Print("[EyeAdapt] no config found, writing defaults to " + PATH);
			settings.Lights = DefaultLights();
			write = true;
		}

		s_Instance = settings;
		if (write && !JsonFileLoader<EyeAdaptSettings>.SaveFile(PATH, settings, error))
			ErrorEx("[EyeAdapt] config.json could not be written: " + error);
	}

	// Server -> one client. Sent in ClientPrepareEvent, before the player entity exists
	static void SendTo(PlayerIdentity identity)
	{
		g_Game.RPCSingleParam(null, EyeAdaptRPC.SYNC_SETTINGS, new Param1<EyeAdaptSettings>(Get()), true, identity);
	}
}

// One row of the light table (EyeAdaptSettings.Lights). Fields = JSON keys
class EyeAdaptLightEntry
{
	string ClassName;
	float Factor = 1.0;
	string Type = "auto";
	float Range = 0.0;

	void EyeAdaptLightEntry(string className = "", float factor = 1.0, string type = "auto", float range = 0.0)
	{
		ClassName = className;
		Factor = factor;
		Type = type;
		Range = range;
	}
}

// Own RPC ids, far above vanilla's ERPCs range (docs/engine/mission-network.md §3.5)
class EyeAdaptRPC
{
	static const int SYNC_SETTINGS = 714200101;     // server -> client: settings
	static const int RESTORE_ADAPTATION = 714200102; // server -> client: adaptation after login
	static const int REPORT_ADAPTATION = 714200103;  // client -> server: current adaptation
}

// Our RPCs (target == null RPCs land in DayZGame.OnRPC)
modded class DayZGame
{
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (!target && rpc_type == EyeAdaptRPC.REPORT_ADAPTATION)
		{
			#ifdef SERVER
			float reported;
			if (ctx.Read(reported))
				EyeAdaptStore.OnReport(sender, reported);
			#endif
			return;
		}

		if (!target && rpc_type == EyeAdaptRPC.RESTORE_ADAPTATION)
		{
			float restored;
			if (ctx.Read(restored))
				EyeAdaptClient.s_PendingRestore = restored;
			return;
		}

		if (!target && rpc_type == EyeAdaptRPC.SYNC_SETTINGS)
		{
			Param1<EyeAdaptSettings> data = new Param1<EyeAdaptSettings>(null);
			if (ctx.Read(data) && data.param1)
			{
				EyeAdaptSettings.Set(data.param1);
				Print("[EyeAdapt] settings received from server");
			}
			else
			{
				ErrorEx("[EyeAdapt] failed to read settings from server");
			}
			return;
		}

		super.OnRPC(sender, target, rpc_type, ctx);
	}
}
