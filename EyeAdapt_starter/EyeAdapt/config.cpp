class CfgPatches
{
	class EyeAdapt
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = { "DZ_Data", "DZ_Scripts" };
	};
};

class CfgMods
{
	class EyeAdapt
	{
		dir = "EyeAdapt";
		name = "Eye Adaptation";
		author = "ollio";
		type = "mod";
		dependencies[] = { "Game", "World", "Mission" };

		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = { "EyeAdapt/scripts/3_Game" };
			};
			class worldScriptModule
			{
				value = "";
				files[] = { "EyeAdapt/scripts/4_World" };
			};
			class missionScriptModule
			{
				value = "";
				files[] = { "EyeAdapt/scripts/5_Mission" };
			};
		};
	};
};
