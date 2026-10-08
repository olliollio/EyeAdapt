// 3_Game: server-side config file, synced to every client on connect (CLAUDE.md §6, docs/engine/mission-network.md §3-4).
// File: $profile:EyeAdapt/config.json on the server (= <server>/profiles/EyeAdapt/config.json).
// Field initializers are the defaults. JSON keys = field names, so keep fields public and don't rename them lightly.
// Missing keys keep their defaults. The file is only (re)written when it's missing or older than CURRENT_VERSION,
// so admins' hand-edited values stay as typed (DayZ writes floats as 0.30000001192092898 and bools as 1/0).
// Bump CURRENT_VERSION (and the Version default) whenever fields are added, so existing files get the new keys.
class EyeAdaptSettings
{
	static const int CURRENT_VERSION = 1;
	int Version = 1;
	bool Enabled = true;

	// Adaptation curve: fast stage (pupil/cones) + slow stage (rods), each ~95 % after its time
	float FullAdaptSeconds = 240.0;        // slow stage. Brief: 300-600 for release
	float FastCurveSeconds = 20.0;
	float FastCurveWeight = 0.3;           // share of the gain from the fast stage (0..1)
	float StartAdaptation = 0.0;           // value on login (until persistence exists)
	float LightFastSeconds = 2.0;          // fast stage drops this fast under light
	float LightSlowSeconds = 30.0;         // slow stage drops this fast under light

	// Light exposure (dose)
	float ScanInterval = 0.5;              // seconds between light scans
	float ScanRadius = 40.0;               // also limited by each light's own radius
	float FalloffExponent = 1.3;           // 2 = physical inverse square, felt too short in game (campfire had no effect at 15 m).
	                                       // With 1.3, looking at a campfire: 2 m -> dose ~0.98, 5 m ~0.7, 10 m ~0.4, 15 m ~0.25, 25 m ~0.13
	float DoseThreshold = 0.02;            // contributions below this count as zero
	float DoseSensitivity = 2.0;           // saturation: dose = 1 - e^(-raw * sensitivity)
	float MaxDoseEffect = 1.0;             // how far a full dose pushes the target down
	float ViewMinFactor = 0.25;            // lights behind you still count this much
	float OwnLightFactor = 0.1;            // own spot light (flashlight/headtorch/weapon light). Own point lights (torch, chemlight) count by distance
	float RedLightFactor = 0.15;           // red light barely affects night vision
	float NightVisionDose = 0.5;           // powered NVG/NV scope counts as this dose, 0 = free
	bool UseOcclusion = true;
	float OccludedFactor = 0.15;           // light behind walls/terrain
	float VegetationOccludedFactor = 0.5;  // light behind trees/bushes
	float OcclusionEndOffset = 0.5;        // ray stops this far before the light

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
	float MaxDesaturation = 0.6;
	float MaxContrastBoost = 0.15;         // counters the grey/foggy look of a strong EV boost
	float BlueShift = 0.1;                 // Purkinje effect, 0 = off

	// Numpad debug keys (set adaptation, toggle effect, print lights). Lets players cheat, switch off on live servers
	bool DebugKeys = true;

	// ---- Static access ----

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
		}
		else
		{
			Print("[EyeAdapt] no config found, writing defaults to " + PATH);
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

// Own RPC ids, far above vanilla's ERPCs range (docs/engine/mission-network.md §3.5)
class EyeAdaptRPC
{
	static const int SYNC_SETTINGS = 714200101; // server -> client: settings
}

// Client receives the settings. target == null RPCs land in DayZGame.OnRPC
modded class DayZGame
{
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
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
