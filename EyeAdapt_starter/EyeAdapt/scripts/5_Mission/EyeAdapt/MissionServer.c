// 5_Mission: server side. Loads the config and sends it to each connecting client.
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();
		EyeAdaptSettings.LoadServer();
	}

	// Runs before the player entity exists, same place vanilla syncs cfggameplay.json
	override void OnClientPrepareEvent(PlayerIdentity identity, out bool useDB, out vector pos, out float yaw, out int preloadTimeout)
	{
		super.OnClientPrepareEvent(identity, useDB, pos, yaw, preloadTimeout);
		EyeAdaptSettings.SendTo(identity);
	}
}
