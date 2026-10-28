// 5_Mission: performance test log (setting PerfLog). EyeAdapt's own client cost, one line every WINDOW seconds:
// frame rate and worst frame (whole game), time of EyeAdapt's per-frame update (incl. scans) and of the light scans,
// lights registered / past the cheap checks / occlusion raycasts per scan. TickCount is the CPU performance counter,
// its rate is calibrated against the game clock every window.
class EyeAdaptPerf
{
	static const float WINDOW = 10.0;

	protected int m_Frames;
	protected float m_Elapsed;
	protected float m_WorstSlice;
	protected int m_UpdateTicks;
	protected int m_UpdateMaxTicks;
	protected int m_Scans;
	protected int m_ScanTicks;
	protected int m_ScanMaxTicks;
	protected int m_Lights;
	protected int m_Passed;
	protected int m_PassedMax;
	protected int m_Raycasts;
	protected int m_RaycastsMax;
	protected int m_TickStart;
	protected int m_MsStart;
	protected float m_TicksPerMs = 10000;

	void AddScan(int ticks, int lights, int passed, int raycasts)
	{
		m_Scans++;
		m_ScanTicks += ticks;
		m_ScanMaxTicks = Math.Max(m_ScanMaxTicks, ticks);
		m_Lights = lights;
		m_Passed += passed;
		m_PassedMax = Math.Max(m_PassedMax, passed);
		m_Raycasts += raycasts;
		m_RaycastsMax = Math.Max(m_RaycastsMax, raycasts);
	}

	void AddFrame(int ticks, float timeslice)
	{
		if (m_MsStart == 0)
		{
			Reset();
			return;
		}
		m_Frames++;
		m_Elapsed += timeslice;
		m_WorstSlice = Math.Max(m_WorstSlice, timeslice);
		m_UpdateTicks += ticks;
		m_UpdateMaxTicks = Math.Max(m_UpdateMaxTicks, ticks);
		if (m_Elapsed < WINDOW)
			return;

		int ms = g_Game.GetTime() - m_MsStart;
		if (ms > 0)
			m_TicksPerMs = TickCount(m_TickStart) / ms;

		float frameMs = m_Elapsed * 1000 / m_Frames;
		float updateAvg = ToMs(m_UpdateTicks) / m_Frames;
		string line = "[EyeAdapt] perf | fps " + Math.Round(m_Frames / m_Elapsed) + " (worst frame " + Math.Round(m_WorstSlice * 1000) + " ms)";
		line += " | update avg " + Fmt(updateAvg) + " ms max " + Fmt(ToMs(m_UpdateMaxTicks)) + " ms (" + Fmt(updateAvg / frameMs * 100) + " % of frame)";
		if (m_Scans > 0)
		{
			float scans = m_Scans;
			line += " | scans " + m_Scans + " avg " + Fmt(ToMs(m_ScanTicks) / m_Scans) + " ms max " + Fmt(ToMs(m_ScanMaxTicks)) + " ms";
			line += " | lights " + m_Lights + ", passed avg " + Fmt(m_Passed / scans) + " max " + m_PassedMax;
			line += ", raycasts avg " + Fmt(m_Raycasts / scans) + " max " + m_RaycastsMax;
		}
		// Print(variable) logs "string line = '...'", so pass an expression instead
		Print(string.Format("%1", line));
		Reset();
	}

	protected void Reset()
	{
		m_Frames = 0;
		m_Elapsed = 0;
		m_WorstSlice = 0;
		m_UpdateTicks = 0;
		m_UpdateMaxTicks = 0;
		m_Scans = 0;
		m_ScanTicks = 0;
		m_ScanMaxTicks = 0;
		m_Passed = 0;
		m_PassedMax = 0;
		m_Raycasts = 0;
		m_RaycastsMax = 0;
		m_MsStart = g_Game.GetTime();
		m_TickStart = TickCount(0);
	}

	protected float ToMs(int ticks)
	{
		return ticks / m_TicksPerMs;
	}

	// 3 decimals
	protected string Fmt(float v)
	{
		return (Math.Round(v * 1000) / 1000).ToString();
	}
}
