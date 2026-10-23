// 5_Mission: server side. Loads the config, sends it to each connecting client,
// restores adaptation on login, remembers it on logout and saves it to profiles/EyeAdapt/adaptation.json (docs/engine/mission-network.md §2.3).
modded class MissionServer
{
	// Set in OnClientNewEvent, read in InvokeOnConnect right after it (same engine event)
	protected bool m_EyeAdaptFreshCharacter;

	override void OnInit()
	{
		super.OnInit();
		EyeAdaptSettings.LoadServer();
		EyeAdaptStore.Load();
	}

	// Runs before the player entity exists, same place vanilla syncs cfggameplay.json
	override void OnClientPrepareEvent(PlayerIdentity identity, out bool useDB, out vector pos, out float yaw, out int preloadTimeout)
	{
		super.OnClientPrepareEvent(identity, useDB, pos, yaw, preloadTimeout);
		EyeAdaptSettings.SendTo(identity);
	}

	// New character (first spawn or after death): starts at StartAdaptation
	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		PlayerBase player = super.OnClientNewEvent(identity, pos, ctx);
		// Only flag it if InvokeOnConnect will follow (vanilla skips it without a player)
		m_EyeAdaptFreshCharacter = player != null;
		return player;
	}

	// Called for new and for loaded characters: restore and send the adaptation
	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity)
	{
		super.InvokeOnConnect(player, identity);

		bool fresh = m_EyeAdaptFreshCharacter;
		m_EyeAdaptFreshCharacter = false;
		if (!identity)
			return;

		float value = EyeAdaptStore.Restore(identity.GetId(), fresh);
		EyeAdaptSendRestore(identity, value);
	}

	// Reconnect to a character still in the world (during the logout countdown). Skips InvokeOnConnect
	override void OnClientReconnectEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReconnectEvent(identity, player);
		if (!identity)
			return;

		float value = EyeAdaptStore.GetCurrent(identity.GetId());
		if (value < 0)
			value = EyeAdaptStore.Restore(identity.GetId(), false);
		EyeAdaptSendRestore(identity, value);
	}

	// Final disconnect after the logout countdown. identity can already be deleted, uid is GetId() captured earlier.
	// Offline time starts here
	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		EyeAdaptStore.OnDisconnect(uid);
		super.PlayerDisconnected(player, identity, uid);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		// Night length for AdaptationMode "relativeToNight" (the offline curve in the persistence uses it too)
		EyeAdaptNightClock.Update(timeslice);
		EyeAdaptStore.Update(timeslice);
	}

	// Shutdown: save everyone, online players with their current value (time freeze until they return)
	override void OnMissionFinish()
	{
		EyeAdaptStore.Save();
		super.OnMissionFinish();
	}

	protected void EyeAdaptSendRestore(PlayerIdentity identity, float value)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(value);
		rpc.Send(null, EyeAdaptRPC.RESTORE_ADAPTATION, true, identity);
	}
}
