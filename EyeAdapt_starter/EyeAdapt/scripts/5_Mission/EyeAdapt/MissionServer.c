// 5_Mission: server side. Loads the config, sends it to each connecting client,
// restores adaptation on login and saves it on logout (docs/engine/mission-network.md §2.3).
modded class MissionServer
{
	// Set in OnClientNewEvent, read in InvokeOnConnect right after it (same engine event)
	protected bool m_EyeAdaptFreshCharacter;
	protected float m_EyeAdaptSaveTimer;

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

	// New character (first spawn or after death): full vision, like waking up
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
	// Save before super, which deletes the body
	override void PlayerDisconnected(PlayerBase player, PlayerIdentity identity, string uid)
	{
		EyeAdaptStore.Save(uid);
		super.PlayerDisconnected(player, identity, uid);
	}

	// Periodic save, so a crash or a killed server process loses at most SaveIntervalSeconds
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		float interval = EyeAdaptSettings.Get().SaveIntervalSeconds;
		if (interval <= 0)
			return;

		m_EyeAdaptSaveTimer += timeslice;
		if (m_EyeAdaptSaveTimer >= interval)
		{
			m_EyeAdaptSaveTimer = 0;
			EyeAdaptStore.SaveAll();
		}
	}

	override void OnMissionFinish()
	{
		EyeAdaptStore.SaveAll();
		super.OnMissionFinish();
	}

	protected void EyeAdaptSendRestore(PlayerIdentity identity, float value)
	{
		ScriptRPC rpc = new ScriptRPC();
		rpc.Write(value);
		rpc.Send(null, EyeAdaptRPC.RESTORE_ADAPTATION, true, identity);
	}
}
