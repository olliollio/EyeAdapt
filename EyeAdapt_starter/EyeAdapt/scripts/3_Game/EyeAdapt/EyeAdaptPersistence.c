// 3_Game: logout/login persistence (CLAUDE.md §3.6, docs/engine/mission-network.md §2, §10.4).
// The client reports its adaptation periodically, the server keeps the latest value per player in memory
// and stamps the time on disconnect. What a returning player gets depends on LogoutMode:
//   "keep"  = the value they logged off with (default)
//   "fixed" = offline time counts as darkness, capped at LogoutAdaptation
//   "sleep" = offline time counts as darkness, a long break means full night vision
// No mode lets a quick relog gain more than waiting in the dark would.
// All records are saved to one file (SAVE_PATH) every SAVE_INTERVAL and on shutdown. Offline time is stored,
// not a timestamp, so a server restart is a time freeze: the downtime doesn't count.

// One player's state
class EyeAdaptRecord
{
	float Adaptation;
	int SavedAtMs;      // server uptime (g_Game.GetTime) of the last update; offline time starts here
	bool Online;
	int m_LastReportMs; // rate limit for client reports
}

// One row of the save file. Fields = JSON keys
class EyeAdaptSavedRecord
{
	string Id;
	float Adaptation;
	float OfflineSeconds;
}

class EyeAdaptSaveFile
{
	int Version = 1;
	ref array<ref EyeAdaptSavedRecord> Players = new array<ref EyeAdaptSavedRecord>();
}

// Server-side store, keyed by PlayerIdentity.GetId() (hashed Steam ID, the same id PlayerDisconnected gets as uid)
class EyeAdaptStore
{
	static const int REPORT_MIN_INTERVAL_MS = 2000;
	static const float SAVE_INTERVAL = 60.0; // seconds; a crash loses at most this much
	static const string SAVE_PATH = "$profile:EyeAdapt/adaptation.json";

	protected static ref map<string, ref EyeAdaptRecord> s_Records = new map<string, ref EyeAdaptRecord>();
	protected static float s_SaveTimer;

	// Value to give a connecting player. fresh = new character (first spawn or after death)
	static float Restore(string id, bool fresh)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		float value;
		if (fresh)
			value = GetStartValue();
		else if (!rec)
			value = GetUnknownValue();
		else
			value = GetReturnValue(rec.Adaptation, GetOfflineSeconds(rec));

		value = Math.Clamp(value, 0, 1);
		if (EyeAdaptSettings.Get().DebugLogging)
			Print(string.Format("[EyeAdapt] restore %1 (%2, fresh %3, record %4) -> %5", id, EyeAdaptSettings.Get().LogoutMode, fresh, rec != null, value));
		SetRecord(id, value);
		return value;
	}

	// Latest value (reconnect during the logout countdown), -1 if unknown
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
		rec.SavedAtMs = now;
	}

	// Disconnect (identity may already be gone, so it takes the id). Offline time starts now
	static void OnDisconnect(string id)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (!rec)
			return;
		rec.SavedAtMs = g_Game.GetTime();
		rec.Online = false;
	}

	// ---- Return value per LogoutMode ----

	static float GetStartValue()
	{
		return Math.Clamp(EyeAdaptSettings.Get().StartAdaptation, 0, 1);
	}

	// Known character without a record: never seen since the mod was added, or pruned after a long break
	static float GetUnknownValue()
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		if (cfg.LogoutMode == "sleep")
			return 1;
		if (cfg.LogoutMode == "fixed")
			return Math.Clamp(cfg.LogoutAdaptation, 0, 1);
		return GetStartValue();
	}

	static float GetReturnValue(float saved, float offlineSeconds)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		if (cfg.LogoutMode == "sleep")
			return GetDarkValue(saved, offlineSeconds);
		if (cfg.LogoutMode == "fixed")
			return Math.Min(GetDarkValue(saved, offlineSeconds), Math.Clamp(cfg.LogoutAdaptation, 0, 1));
		return saved;
	}

	// Offline time as time in the dark, at the slow stage's rate (same curve as EyeAdaptState)
	static float GetDarkValue(float saved, float offlineSeconds)
	{
		float seconds = EyeAdaptSettings.GetFullAdaptSeconds();
		if (seconds <= 0)
			return 1;
		return 1 - (1 - saved) * Math.Pow(Math.EULER, -3 * offlineSeconds / seconds);
	}

	// A record can go once returning would give the same as having no record
	static bool CanForget(EyeAdaptRecord rec, float offlineSeconds)
	{
		EyeAdaptSettings cfg = EyeAdaptSettings.Get();
		if (cfg.LogoutMode == "sleep" || cfg.LogoutMode == "fixed")
			return GetReturnValue(rec.Adaptation, offlineSeconds) >= GetUnknownValue() - 0.001;
		return cfg.RecordKeepDays > 0 && offlineSeconds > cfg.RecordKeepDays * 86400;
	}

	static float GetOfflineSeconds(EyeAdaptRecord rec)
	{
		if (rec.Online)
			return 0;
		return Math.Max(g_Game.GetTime() - rec.SavedAtMs, 0) / 1000.0;
	}

	// ---- Save file ----

	// Server start: offline time continues where it stopped, the downtime doesn't count
	static void Load()
	{
		if (!FileExist(SAVE_PATH))
			return;

		EyeAdaptSaveFile file = new EyeAdaptSaveFile();
		string error;
		if (!JsonFileLoader<EyeAdaptSaveFile>.LoadFile(SAVE_PATH, file, error))
		{
			ErrorEx("[EyeAdapt] adaptation.json could not be read, starting without records: " + error);
			return;
		}

		int now = g_Game.GetTime();
		foreach (EyeAdaptSavedRecord saved : file.Players)
		{
			if (!saved || saved.Id == "")
				continue;
			EyeAdaptRecord rec = new EyeAdaptRecord();
			rec.Adaptation = Math.Clamp(saved.Adaptation, 0, 1);
			rec.SavedAtMs = now - (int)(Math.Max(saved.OfflineSeconds, 0) * 1000);
			s_Records.Set(saved.Id, rec);
		}
		Print(string.Format("[EyeAdapt] %1 adaptation records loaded", s_Records.Count()));
	}

	// Writes every record, online players with 0 offline seconds. Drops records that no longer matter
	static void Save()
	{
		EyeAdaptSaveFile file = new EyeAdaptSaveFile();
		array<string> forget = new array<string>();
		foreach (string id, EyeAdaptRecord rec : s_Records)
		{
			float offline = GetOfflineSeconds(rec);
			if (!rec.Online && CanForget(rec, offline))
			{
				forget.Insert(id);
				continue;
			}
			EyeAdaptSavedRecord saved = new EyeAdaptSavedRecord();
			saved.Id = id;
			saved.Adaptation = rec.Adaptation;
			saved.OfflineSeconds = offline;
			file.Players.Insert(saved);
		}
		foreach (string gone : forget)
			s_Records.Remove(gone);

		string error;
		if (!JsonFileLoader<EyeAdaptSaveFile>.SaveFile(SAVE_PATH, file, error))
			ErrorEx("[EyeAdapt] adaptation.json could not be written: " + error);
	}

	// Server tick
	static void Update(float timeslice)
	{
		s_SaveTimer += timeslice;
		if (s_SaveTimer < SAVE_INTERVAL)
			return;
		s_SaveTimer = 0;
		Save();
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
		rec.SavedAtMs = g_Game.GetTime();
		rec.Online = true;
	}
}

// Client side: restored value from the server, picked up by MissionGameplay on its next tick
class EyeAdaptClient
{
	static float s_PendingRestore = -1;
	// Set when vanilla decided the local player is hit by a flash grenade (range + line of sight), consumed by the mission tick
	static bool s_FlashbangHit;

	// Client -> server
	static void Report(float adaptation)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(adaptation);
		rpc.Send(null, EyeAdaptRPC.REPORT_ADAPTATION, true, null);
	}
}
