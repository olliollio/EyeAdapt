// 4_World: registry of all dynamic lights (client-only entities). See docs/engine/lights.md §9.
// The dose scan iterates this list instead of searching the world.
modded class ScriptedLightBase
{
	// Weak references (lights are Managed): deleted lights turn into null and get pruned by the scan
	static ref array<ScriptedLightBase> s_EyeAdaptLights = new array<ScriptedLightBase>();

	// EOnInit runs after all subclass constructors, so per-type values are final here
	override void EOnInit(IEntity other, int extra)
	{
		super.EOnInit(other, extra);
		s_EyeAdaptLights.Insert(this);
	}
}
