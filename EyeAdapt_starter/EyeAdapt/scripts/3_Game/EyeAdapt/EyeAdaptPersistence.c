// 3_Game: logout/login persistence (CLAUDE.md §3.6, docs/engine/mission-network.md §2, §10.4).
// The client reports its adaptation periodically, the server keeps the latest value per player in memory
// and stamps the time on disconnect. On login the offline time counts as time in the dark, so a quick relog
// gives nothing extra and a long break means full night vision.
// Memory only, no files: a server restart (or crash) forgets everyone, so they start at StartAdaptation,
// which by default means full vision, same as an overnight break ("you were sleeping").

// One player's state
class EyeAdaptRecord
{
	float Adaptation;
	int SavedAtMs;      // server uptime (g_Game.GetTime) of the last update
	int m_LastReportMs; // rate limit for client reports
}

// Server-side store, keyed by PlayerIdentity.GetId() (hashed Steam ID, the same id PlayerDisconnected gets as uid)
class EyeAdaptStore
{
	static const int REPORT_MIN_INTERVAL_MS = 2000;

	protected static ref map<string, ref EyeAdaptRecord> s_Records = new map<string, ref EyeAdaptRecord>();

	// Value to give a connecting player. fresh = new character (first spawn or after death)
	static float Restore(string id, bool fresh)
	{
		float start = Math.Clamp(EyeAdaptSettings.Get().StartAdaptation, 0, 1);
		EyeAdaptRecord rec = s_Records.Get(id);
		if (fresh || !rec)
		{
			// New character, or no record since the last restart: treat like a fresh start
			SetRecord(id, start);
			return start;
		}

		// Offline time counts as darkness, at the slow stage's rate (same curve as EyeAdaptState)
		float offline = Math.Max(g_Game.GetTime() - rec.SavedAtMs, 0) / 1000.0;
		float seconds = EyeAdaptSettings.GetFullAdaptSeconds();
		float restored = 1;
		if (seconds > 0)
			restored = 1 - (1 - rec.Adaptation) * Math.Pow(Math.EULER, -3 * offline / seconds);

		restored = Math.Clamp(restored, 0, 1);
		SetRecord(id, restored);
		Print(string.Format("[EyeAdapt] restore %1: saved %2, offline %3 s -> %4", id, rec.Adaptation, offline, restored));
		return restored;
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

	// Disconnect (identity may already be gone, so it takes the id). Offline time starts now.
	// The record stays, it's a few bytes per player until the next restart
	static void OnDisconnect(string id)
	{
		EyeAdaptRecord rec = s_Records.Get(id);
		if (rec)
			rec.SavedAtMs = g_Game.GetTime();
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
