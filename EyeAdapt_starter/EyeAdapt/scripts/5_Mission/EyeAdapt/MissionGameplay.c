// 5_Mission: hooks into the running client mission. Runs only on the client.
modded class MissionGameplay
{
	protected ref EyeAdaptState m_EyeAdapt;
	protected float m_EyeAdaptLogTimer;

	override void OnInit()
	{
		super.OnInit();
		m_EyeAdapt = new EyeAdaptState();
		Print("[EyeAdapt] loaded");
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!m_EyeAdapt)
			return;

		m_EyeAdapt.Update(timeslice);

		m_EyeAdaptLogTimer += timeslice;
		if (m_EyeAdaptLogTimer >= EyeAdaptConfig.LOG_INTERVAL)
		{
			m_EyeAdaptLogTimer = 0;
			Print("[EyeAdapt] adaptation = " + m_EyeAdapt.Get().ToString());
		}
	}
}
