// 3_Game: shared settings/constants. Loaded first, no access to player/world classes.
class EyeAdaptConfig
{
	static const float FULL_ADAPT_SECONDS = 300.0;  // 5 min to full night vision (brief: 300-600)
	static const float LOG_INTERVAL = 5.0;          // debug print interval (seconds)

	// Phase 1 test: cycle a fixed exposure boost 0..TEST_EV_MAX EV to see what the engine does with it
	static const bool  TEST_EV_CYCLE = true;
	static const int   TEST_EV_MAX = 3;             // highest boost step in EV
	static const float TEST_EV_STEP_SECONDS = 10.0; // time per step
}
