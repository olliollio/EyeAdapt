// 3_Game: shared settings/constants. Loaded first, no access to player/world classes.
// All gameplay numbers live here until the server JSON config exists (Phase 5).
class EyeAdaptConfig
{
	// ---- Adaptation curve (sum of a fast and a slow stage, see CLAUDE.md §3.1) ----
	// Each stage approaches its target exponentially and is ~95 % there after its time.
	static const float FULL_ADAPT_SECONDS = 240.0;  // slow stage (rods): ~95 % after this many seconds. 4 min for multiplayer tests, brief: 300-600
	static const float FAST_CURVE_SECONDS = 20.0;   // fast stage (pupil/cones): ~95 % after this many seconds
	static const float FAST_CURVE_WEIGHT = 0.3;     // share of the total gain that comes from the fast stage (0..1)
	static const float START_ADAPTATION = 0.0;      // value on login. Brief says 1 for fresh logins (Phase 5), 0 makes testing the curve easier

	// Under light (asymmetric, CLAUDE.md design rule 5): adaptation drops much faster than it builds up
	static const float LIGHT_FAST_SECONDS = 2.0;    // fast stage drops ~95 % within this time under light
	static const float LIGHT_SLOW_SECONDS = 30.0;   // slow stage drops ~95 % within this time, so long exposure costs more

	// ---- Light exposure (dose), CLAUDE.md §3.3 ----
	// Example with defaults: campfire (brightness 4.75) at 2 m -> dose ~0.9, at 5 m -> ~0.3, at 15 m -> ~0
	static const float SCAN_INTERVAL = 0.5;         // seconds between light scans (eyes are slow, no need for every frame)
	static const float SCAN_RADIUS = 40.0;          // lights further away are ignored (also limited by each light's own radius)
	static const float FALLOFF_EXPONENT = 2.0;      // 2 = physical inverse square. Lower = light reaches further
	static const float DOSE_THRESHOLD = 0.02;       // contributions below this count as zero
	static const float DOSE_SENSITIVITY = 2.0;      // saturation curve: dose = 1 - e^(-raw * sensitivity)
	static const float MAX_DOSE_EFFECT = 1.0;       // how far a full dose pushes the target down (1 = fully light-adapted)
	static const float VIEW_MIN_FACTOR = 0.25;      // lights behind you still count this much (they light up the scene)
	static const float OWN_LIGHT_FACTOR = 0.1;      // own SPOT light (flashlight/headtorch/weapon light): this share of its brightness. Not free.
	                                                // Own point lights (torch, chemlight, gas lamp) count fully by distance, like on the ground
	static const float RED_LIGHT_FACTOR = 0.15;     // red light barely affects night vision
	static const bool  USE_OCCLUSION = true;        // raycast eye -> light, only for lights that would count
	static const float OCCLUDED_FACTOR = 0.15;      // blocked light still counts this much (indirect light around a wall/corner)
	static const float OCCLUSION_END_OFFSET = 0.5;  // ray stops this far before the light so the fireplace/lamp model can't block it

	// ---- Night gate (design rule 1: night only, day stays vanilla) ----
	// Sunrise/sunset come from the map's WorldData. Vanilla Chernarus in July: sunrise ~4.0 h, sunset ~20.0 h,
	// vanilla itself counts sunset..sunset+2 h as dusk and sunrise..sunrise+2 h as dawn.
	static const float DUSK_OFFSET_HOURS = 0.0;     // fade-in starts this many hours after sunset
	static const float DUSK_HOURS = 2.0;            // fade-in duration
	static const float DAWN_OFFSET_HOURS = -1.5;    // fade-out starts this many hours after sunrise (negative = before).
	                                                // Morning twilight is already bright 0.5 h before sunrise (+4.5 EV looked grey/washed out in test)
	static const float DAWN_HOURS = 1.5;            // fade-out duration, ends at sunrise

	// ---- Ambient light (design rule 4: amplify existing light, don't create it) ----
	// The EV boost is scaled by the sky: clear full moon = full boost, overcast new moon = MIN_AMBIENT_GAIN of it
	static const float MOON_INTENSITY_FULL = 1.0;   // raw GetMoonIntensity() that counts as full moonlight. Range undocumented, calibrate from the log
	static const float STARLIGHT_SHARE = 0.4;       // clear moonless sky, relative to clear full moon
	static const float OVERCAST_CLEAR = 0.35;       // overcast up to this counts as clear (engine uses clear lighting below 0.35)
	static const float OVERCAST_FULL = 1.0;         // fully overcast (engine: no moonlight at all)
	static const float OVERCAST_MIN_SHARE = 0.25;   // fully overcast sky, relative to the same sky when clear
	static const float MIN_AMBIENT_GAIN = 0.2;      // share of MAX_EV_BOOST in the darkest sky (0 = no boost at all)
	static const float UNDERGROUND_GAIN = 0.0;      // share of the boost underground (caves, bunkers): nothing to amplify
	static const float AMBIENT_SMOOTH_SECONDS = 2.0; // smoothing time for changes of the ambient gain

	// ---- Visuals ----
	// All of these scale linearly with adaptation (0 = vanilla look, 1 = full value). EV and contrast also scale with the ambient gain
	static const float MAX_EV_BOOST = 5.0;          // exposure boost in EV stops at full adaptation under a clear full moon (scaled down by the ambient gain)
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
	static const int   KEY_LIGHTS = KeyCode.KC_NUMPAD9; // print every light that contributes to the dose to the script log
}
