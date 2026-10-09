// 4_World: flash grenade hit. The grenade's light lives only 0.35 s, so the light scan would often miss it.
// Vanilla already decides who is hit (range and line of sight to the head) and calls this on the local player,
// so the hit itself is the trigger. The mission tick applies it (5_Mission isn't visible from here).
modded class PlayerBase
{
	override void OnPlayerReceiveFlashbangHitStart(bool visual)
	{
		super.OnPlayerReceiveFlashbangHitStart(visual);

		if (EyeAdaptSettings.Get().FlashbangAdaptationLoss > 0 && this == g_Game.GetPlayer())
			EyeAdaptClient.s_FlashbangHit = true;
	}
}
