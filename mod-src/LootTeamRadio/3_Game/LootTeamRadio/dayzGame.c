// Server station map RPCs with null target arrive at DayZGame.OnRPC, not MissionGameplay.OnEvent.
modded class DayZGame
{
	override void OnRPC(PlayerIdentity sender, Object target, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type == 777003 && !target)
		{
			Param1<string> help = new Param1<string>("");
			if (ctx.Read(help) && help.param1 == "help") LootRadioState.ShowHelp();
			return;
		}
		if (rpc_type == 777002 && !target)
		{
			Param1<string> volume = new Param1<string>("");
			if (ctx.Read(volume)) LootRadioState.SetUserVolume(volume.param1.ToInt());
			return;
		}
		if (rpc_type == 777001 && !target)
		{
			Param1<string> data = new Param1<string>("");
			if (ctx.Read(data))
			{
				Print("[LootRadio] stations RPC received");
				LootRadioState.SetStationsFromString(data.param1);
			}
			else
			{
				Print("[LootRadio] stations RPC read failed");
			}
			return;
		}
		super.OnRPC(sender, target, rpc_type, ctx);
	}
}
