// 3_Game: logout/login persistence (CLAUDE.md §3.6, docs/engine/mission-network.md §2, §10.4).
// The client reports its adaptation periodically, the server keeps the latest value per player and
// saves it to $profile:EyeAdapt/players/<id>.json on disconnect, together with a real-world timestamp.
// On login the offline time counts as time in the dark, so a quick relog gives nothing extra
// and a long break (or a server restart overnight) means full night vision.

// One player's saved state (JSON file)
class EyeAdaptRecord
{
	float Adaptation;
	int SavedAtUnix;

	[NonSerialized()]
	int m_LastReportMs; // rate limit for client reports
}

// Server-side store, keyed by PlayerIdentity.GetId() (hashed Steam ID, the same id PlayerDisconnected gets as uid)
class EyeAdaptStore
{
	static const string DIR = "$profile:EyeAdapt/players";
	static const int REPORT_MIN_INTERVAL_MS = 2000;

	protected static ref map<string, ref EyeAdaptRecord> s_Records = new map<string, ref EyeAdaptRecord>();

	// Value to give a connecting player. fresh = new character (first spawn or after death)
	static float Restore(string id, bool fresh)
	{
		float start = Math.Clamp(EyeAdaptSettings.Get().StartAdaptation, 0, 1);
		if (fresh)
		{
			SetRecord(id, start);
			return start;
		}

		EyeAdaptRecord rec = GetRecord(id);
		if (!rec)
		{
			// No record yet (first time with the mod): treat like a fresh character
			SetRecord(id, start);
			return start;
		}

		// Offline time counts as darkness, at the slow stage's rate (same curve as EyeAdaptState)
		float offline = Math.Max(UnixNow() - rec.SavedAtUnix, 0);
		float seconds = EyeAdaptSettings.GetFullAdaptSeconds();
		float restored = 1;
		if (seconds > 0)
			restored = 1 - (1 - rec.Adaptation) * Math.Pow(Math.EULER, -3 * offline / seconds);

		restored = Math.Clamp(restored, 0, 1);
		SetRecord(id, restored);
		Print(string.Format("[EyeAdapt] restore %1: saved %2, offline %3 s -> %4", id, rec.Adaptation, offline, restored));
		return restored;
	}

	// Latest in-memory value (reconnect during the logout countdown), -1 if unknown
	static float GetCurrent(string id)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (!rec)
			return -1;
		return rec.Adaptation;
	}

	// Client report. Only from the sender's own identity, clamped and rate limited
	static void OnReport(PlayerIdentity sender, float value)
	{
		if (!sender)
			return;

		EyeAdaptRecord rec = s_Records.Get(sender.GetId());
		if (!rec)
			return; // not connected through our hooks, ignore

		int now = g_Game.GetTime();
		if (rec.m_LastReportMs > 0 && now - rec.m_LastReportMs < REPORT_MIN_INTERVAL_MS)
			return;

		// NaN check: NaN fails every comparison
		if (!(value >= 0 && value <= 1))
			return;

		rec.m_LastReportMs = now;
		rec.Adaptation = value;
		rec.SavedAtUnix = UnixNow();
	}

	// Disconnect (identity may already be gone, so it takes the id). Writes the file and forgets the record
	static void Save(string id)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (!rec)
			return;

		rec.SavedAtUnix = UnixNow();
		WriteFile(id, rec);
		s_Records.Remove(id);
	}

	// Server shutdown
	static void SaveAll()
	{
		int now = UnixNow();
		foreach (string id, EyeAdaptRecord rec : s_Records)
		{
			rec.SavedAtUnix = now;
			WriteFile(id, rec);
		}
	}

	protected static EyeAdaptRecord GetRecord(string id)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (rec)
			return rec;

		string path = GetPath(id);
		if (!FileExist(path))
			return null;

		rec = new EyeAdaptRecord();
		string error;
		if (!JsonFileLoader<EyeAdaptRecord>.LoadFile(path, rec, error))
		{
			ErrorEx("[EyeAdapt] could not read " + path + ": " + error);
			return null;
		}
		return rec;
	}

	protected static void SetRecord(string id, float adaptation)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (!rec)
		{
			rec = new EyeAdaptRecord();
			s_Records.Set(id, rec);
		}
		rec.Adaptation = adaptation;
		rec.SavedAtUnix = UnixNow();
	}

	protected static void WriteFile(string id, EyeAdaptRecord rec)
	{
		if (!FileExist("$profile:EyeAdapt"))
			MakeDirectory("$profile:EyeAdapt");
		if (!FileExist(DIR))
			MakeDirectory(DIR);

		string error;
		if (!JsonFileLoader<EyeAdaptRecord>.SaveFile(GetPath(id), rec, error))
			ErrorEx("[EyeAdapt] could not write player record: " + error);
	}

	// GetId() is base64-like and may contain / + =, which don't belong in file names
	protected static string GetPath(string id)
	{
		string safe = id;
		safe.Replace("/", "_");
		safe.Replace("+", "-");
		safe.Replace("=", "");
		return DIR + "/" + safe + ".json";
	}

	// Unix time in seconds (UTC). Vanilla has no such helper (days-from-civil algorithm, valid until 2038)
	static int UnixNow()
	{
		int y, mo, d, h, mi, s;
		GetYearMonthDayUTC(y, mo, d);
		GetHourMinuteSecondUTC(h, mi, s);
		if (mo <= 2)
			y -= 1;
		int era = y / 400;
		int yoe = y - era * 400;
		int mp = (mo + 9) % 12;
		int doy = (153 * mp + 2) / 5 + d - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		int days = era * 146097 + doe - 719468;
		return days * 86400 + h * 3600 + mi * 60 + s;
	}
}

// Client side: restored value from the server, picked up by MissionGameplay on its next tick
class EyeAdaptClient
{
	static float s_PendingRestore = -1;

	// Client -> server
	static void Report(float adaptation)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(adaptation);
		rpc.Send(null, EyeAdaptRPC.REPORT_ADAPTATION, true, null);
	}
}
