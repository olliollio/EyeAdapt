// 3_Game: shared settings/constants. Loaded first, no access to player/world classes.
// All gameplay numbers live here until the server JSON config exists (Phase 5).
class EyeAdaptConfig
{
	// ---- Adaptation curve (sum of a fast and a slow stage, see CLAUDE.md §3.1) ----
	// Each stage approaches its target exponentially and is ~95 % there after its time.
	static const float FULL_ADAPT_SECONDS = 60.0;   // slow stage (rods): ~95 % after this many seconds. TESTING value, brief: 300-600
	static const float FAST_CURVE_SECONDS = 20.0;   // fast stage (pupil/cones): ~95 % after this many seconds
	static const float FAST_CURVE_WEIGHT = 0.3;     // share of the total gain that comes from the fast stage (0..1)
	static const float START_ADAPTATION = 0.0;      // value on login. Brief says 1 for fresh logins (Phase 5), 0 makes testing the curve easier

	// ---- Night gate (design rule 1: night only, day stays vanilla) ----
	// Sunrise/sunset come from the map's WorldData. Vanilla Chernarus in July: sunrise ~4.0 h, sunset ~20.0 h,
	// vanilla itself counts sunset..sunset+2 h as dusk and sunrise..sunrise+2 h as dawn.
	static const float DUSK_OFFSET_HOURS = 0.0;     // fade-in starts this many hours after sunset
	static const float DUSK_HOURS = 2.0;            // fade-in duration
	static const float DAWN_OFFSET_HOURS = -1.5;    // fade-out starts this many hours after sunrise (negative = before).
	                                                // Morning twilight is already bright 0.5 h before sunrise (+4.5 EV looked grey/washed out in test)
	static const float DAWN_HOURS = 1.5;            // fade-out duration, ends at sunrise

	// ---- Visuals ----
	// All of these scale linearly with adaptation (0 = vanilla look, 1 = full value)
	static const float MAX_EV_BOOST = 4.5;          // exposure boost in EV stops at full adaptation (in-game sweet spot 4-5 on a dark, moonless night)
	static const float MAX_DESATURATION = 0.6;      // 0 = full colour, 1 = grey. Rods don't see colour
	static const float MAX_CONTRAST_BOOST = 0.15;   // added to contrast 1.0 (engine range 0..2), counters the grey/foggy look of a strong EV boost
	static const float BLUE_SHIFT = 0.1;            // Purkinje effect: red/green multiplied down by up to this much. 0 = off

	// ---- Debug ----
	static const float LOG_INTERVAL = 5.0;          // debug print interval (seconds)
	// Keyboard shortcuts, only while no menu/chat is open. Numpad keys because F6-F8 are gesture slots in DayZ.
	static const bool  DEBUG_KEYS = true;
	static const int   KEY_RESET = KeyCode.KC_NUMPAD1;  // adaptation -> 0 ("just left a lit room")
	static const int   KEY_FULL = KeyCode.KC_NUMPAD3;   // adaptation -> 1 ("fully dark-adapted")
	static const int   KEY_BYPASS = KeyCode.KC_NUMPAD7; // toggle the visual effect off/on for A/B comparison with vanilla
}
