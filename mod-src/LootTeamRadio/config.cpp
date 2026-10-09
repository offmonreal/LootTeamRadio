class CfgMods
{
	class LootTeamRadio
	{
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"LootTeamRadio/3_Game"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"LootTeamRadio/5_Mission"};
			};
		};
	};
};
class CfgPatches
{
	class LootTeamRadio
	{
		units[] = {};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts"};
	};
};
