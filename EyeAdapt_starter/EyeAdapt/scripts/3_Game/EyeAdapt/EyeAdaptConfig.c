// 3_Game: client-only constants that are not gameplay values.
// All gameplay numbers live in EyeAdaptSettings (server config.json, synced to clients).
class EyeAdaptConfig
{
	static const float LOG_INTERVAL = 5.0;              // debug print interval (seconds)
	static const float TEXT_INTERVAL = 1.0;             // on-screen adaptation text refresh (seconds)

	// Debug keys, only while no menu/chat is open and only if the server allows them (EyeAdaptSettings.DebugKeys).
	// Numpad keys because F6-F8 are gesture slots in DayZ.
	static const int   KEY_RESET = KeyCode.KC_NUMPAD1;  // adaptation -> 0 ("just left a lit room")
	static const int   KEY_FULL = KeyCode.KC_NUMPAD3;   // adaptation -> 1 ("fully dark-adapted")
	static const int   KEY_BYPASS = KeyCode.KC_NUMPAD7; // toggle the visual effect off/on for A/B comparison with vanilla
	static const int   KEY_LIGHTS = KeyCode.KC_NUMPAD9; // print every light that contributes to the dose to the script log
	static const int   KEY_GLARE_TEST = KeyCode.KC_MULTIPLY; // Numpad *: toggle full glare on/off to judge the glare look
	static const int   KEY_CAR_LIGHTS = KeyCode.KC_DIVIDE;   // Numpad /: toggle local test headlights on the nearest car (testing alone)
	static const float CAR_LIGHTS_SEARCH_RADIUS = 30.0;      // how far the nearest car may be
}
