// Server-side radio integration reference. Paste the class members/methods
// below into CustomMission: MissionServer. Merge OnEvent and OnInit into
// existing overrides (super called once), not replace the complete mission.
// In InvokeOnConnect send the snapshot block shown below.
// For DayZ script compilation, do NOT install this snippet as a standalone
// file: class members/overrides must live inside the existing CustomMission.

	private ref RTStations m_RadioStations;
	private string m_RadioAdminSteamId = "REPLACE_WITH_ADMIN_STEAM_ID";
	private const string RADIO_CONFIG = "$profile:DayZRadio/radio-stations.json";

	override void OnEvent(EventType eventTypeId, Param params)
	{
		super.OnEvent(eventTypeId, params);
		if (eventTypeId == ChatMessageEventTypeID)
		{
			ChatMessageEventParams chat = ChatMessageEventParams.Cast(params);
			if (chat) this.HandleRadioChatCommand(chat.param3, chat.param2);
		}
	}

	override void OnInit()
	{
		super.OnInit();
		RadioLoadStations();
	}

	void HandleRadioChatCommand(string text, string from)
	{
		if (text.IndexOf("!radio") != 0) return;
		if (text == "!radio help")
		{
			RadioSendHelp(from);
			return;
		}
		if (text.IndexOf("!radio volume ") == 0)
		{
			RadioSetPlayerVolume(from, text.Substring(14, text.Length() - 14));
			return;
		}
		// Personal volume shorthand: !radio 0 .. !radio 100.
		if (text.IndexOf("!radio ") == 0)
		{
			string shorthand = text.Substring(7, text.Length() - 7);
			if (shorthand.Length() >= 1 && shorthand.Length() <= 3 && shorthand.ToInt().ToString() == shorthand)
			{
				RadioSetPlayerVolume(from, shorthand);
				return;
			}
		}
		if (this.RadioGetPlayerSteamId(from) != m_RadioAdminSteamId)
		{
			Print("[RADIO] chat cmd DENIED for " + from);
			return;
		}
		Print("[RADIO] cmd from admin: " + text);
		if (text == "!radio reload")
		{
			RadioLoadStations();
			return;
		}
		if (text.IndexOf("!radio clear ") == 0)
		{
			string slotText = text.Substring(13, text.Length() - 13);
			if (slotText.Length() != 1 || slotText.ToInt() < 1 || slotText.ToInt() > 8 || slotText.ToInt().ToString() != slotText) return;
			RadioSetStation(slotText.ToInt(), "", "");
			return;
		}
		if (text.IndexOf("!radio set ") != 0) return;
		string args = text.Substring(11, text.Length() - 11);
		int comma1 = args.IndexOf(",");
		if (comma1 < 1) return;
		int comma2 = args.IndexOfFrom(comma1 + 1, ",");
		if (comma2 <= comma1 + 1) return;
		string slotTextSet = args.Substring(0, comma1);
		if (slotTextSet.Length() != 1 || slotTextSet.ToInt() < 1 || slotTextSet.ToInt() > 8 || slotTextSet.ToInt().ToString() != slotTextSet) return;
		int slot = slotTextSet.ToInt();
		string url = args.Substring(comma1 + 1, comma2 - comma1 - 1);
		string name = args.Substring(comma2 + 1, args.Length() - comma2 - 1);
		RadioSetStation(slot, url, name);
	}

	void RadioSendHelp(string playerName)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		PlayerIdentity target;
		for (int p = 0; p < players.Count(); p++)
		{
			PlayerIdentity identity = players[p].GetIdentity();
			if (identity && identity.GetName() == playerName)
			{
				if (target) return; // duplicate names: deny rather than target wrong player
				target = identity;
			}
		}
		if (target)
			GetGame().RPCSingleParam(null, 777003, new Param1<string>("help"), true, target);
	}

	void RadioSetPlayerVolume(string playerName, string value)
	{
		if (value.Length() < 1 || value.Length() > 3) return;
		for (int i = 0; i < value.Length(); i++)
		{
			string digit = value.Substring(i, 1);
			if (digit.ToInt().ToString() != digit) return;
		}
		int volume = value.ToInt();
		if (volume < 0 || volume > 100) return;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		PlayerIdentity target;
		for (int p = 0; p < players.Count(); p++)
		{
			PlayerIdentity identity = players[p].GetIdentity();
			if (identity && identity.GetName() == playerName)
			{
				if (target) return; // duplicate names: no reliable chat sender identity
				target = identity;
			}
		}
		if (target)
			GetGame().RPCSingleParam(null, 777002, new Param1<string>(value), true, target);
	}

	string RadioGetPlayerSteamId(string playerName)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		string found = "";
		for (int i = 0; i < players.Count(); i++)
		{
			PlayerIdentity id = players[i].GetIdentity();
			if (id && id.GetName() == playerName)
			{
				if (found != "") return ""; // identical names are ambiguous; deny
				found = id.GetPlainId();
			}
		}
		return found;
	}

	void RadioLoadStations()
	{
		RTStations cfg = new RTStations();
		string error;
		string path = RADIO_CONFIG;
		if (!FileExist(path)) path = "$mission:radio-stations.json";
		if (!JsonFileLoader<RTStations>.LoadFile(path, cfg, error))
		{
			Print("[RADIO] config load FAIL: " + error);
			return;
		}
		if (!RadioValidConfig(cfg))
		{
			Print("[RADIO] invalid config; keeping last good stations");
			return;
		}
		if (path != RADIO_CONFIG)
		{
			MakeDirectory("$profile:DayZRadio");
			if (!JsonFileLoader<RTStations>.SaveFile(RADIO_CONFIG, cfg, error))
				Print("[RADIO] cannot persist default config: " + error);
		}
		m_RadioStations = cfg;
		Print("[RADIO] stations loaded: " + m_RadioStations.stations.Count().ToString());
		this.RadioBroadcastStations();
	}

	bool RadioValidConfig(RTStations cfg)
	{
		if (!cfg || !cfg.stations || cfg.stations.Count() != 8) return false;
		int freqs[] = {878, 895, 913, 919, 946, 966, 997, 1025};
		for (int i = 0; i < 8; i++)
		{
			RTStation s = cfg.stations[i];
			if (!s || s.slotId != i + 1 || Math.Round(s.frequency * 10.0) != freqs[i]) return false;
			if (s.url != "" && s.url.IndexOf("http://") != 0 && s.url.IndexOf("https://") != 0) return false;
			if (s.url.Contains("|") || s.url.Contains(";") || s.url.Contains(" ") || s.url.Contains(",") || s.name.Contains("|") || s.name.Contains(";")) return false;
			if (s.url.Length() > 512 || s.name.Length() > 80 || s.name == "-" || s.url == "-") return false;
			if (s.name.Contains("#")) return false;
			if (s.url != "" && s.name == "") return false;
			if (s.url == "" && s.name != "") return false;
		}
		return true;
	}

	string RadioSerializeStations()
	{
		string data = "";
		if (!m_RadioStations) return data;
		for (int i = 0; i < 8; i++)
		{
			RTStation s = m_RadioStations.stations[i];
			if (i > 0) data = data + ";";
			string name = s.name;
			string url = s.url;
			if (name == "") name = "-";
			if (url == "") url = "-";
			data = data + s.slotId.ToString() + "|" + Math.Round(s.frequency * 10.0).ToString() + "|" + name + "|" + url;
		}
		return data;
	}

	void RadioSetStation(int slot, string url, string name)
	{
		if (!m_RadioStations || slot < 1 || slot > 8) return;
		RTStations updated = new RTStations();
		for (int i = 0; i < 8; i++)
		{
			RTStation old = m_RadioStations.stations[i];
			RTStation copy = new RTStation();
			copy.slotId = old.slotId;
			copy.frequency = old.frequency;
			copy.url = old.url;
			copy.name = old.name;
			if (copy.slotId == slot)
			{
				copy.url = url;
				copy.name = name;
			}
			updated.stations.Insert(copy);
		}
		if (!RadioValidConfig(updated))
		{
			Print("[RADIO] admin update rejected: invalid slot, URL or name");
			return;
		}
		string error;
		// Validate a temporary JSON first. Only then touch the active config.
		if (!JsonFileLoader<RTStations>.SaveFile("$profile:DayZRadio/radio-stations.pending.json", updated, error))
		{
			Print("[RADIO] pending save failed: " + error);
			return;
		}
		RTStations check = new RTStations();
		if (!JsonFileLoader<RTStations>.LoadFile("$profile:DayZRadio/radio-stations.pending.json", check, error) || !RadioValidConfig(check))
		{
			Print("[RADIO] pending config invalid: " + error);
			return;
		}
		if (!CopyFile(RADIO_CONFIG, "$profile:DayZRadio/radio-stations.backup.json"))
		{
			Print("[RADIO] backup failed; update cancelled");
			return;
		}
		if (!CopyFile("$profile:DayZRadio/radio-stations.pending.json", RADIO_CONFIG))
		{
			Print("[RADIO] activating config failed");
			CopyFile("$profile:DayZRadio/radio-stations.backup.json", RADIO_CONFIG);
			return;
		}
		RTStations verified = new RTStations();
		if (!JsonFileLoader<RTStations>.LoadFile(RADIO_CONFIG, verified, error) || !RadioValidConfig(verified))
		{
			Print("[RADIO] config verification failed; restoring backup: " + error);
			CopyFile("$profile:DayZRadio/radio-stations.backup.json", RADIO_CONFIG);
			return;
		}
		m_RadioStations = verified;
		RadioBroadcastStations();
		Print("[RADIO] slot updated: " + slot.ToString());
	}

	void RadioBroadcastStations()
	{
		if (!m_RadioStations) return;
		string data = RadioSerializeStations();
		Param1<string> rpcData = new Param1<string>(data);
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		for (int j = 0; j < players.Count(); j++)
		{
			PlayerIdentity playerIdentity = players[j].GetIdentity();
			if (playerIdentity)
				GetGame().RPCSingleParam(null, 777001, rpcData, true, playerIdentity);
		}
		Print("[RADIO] stations sent to " + players.Count().ToString() + " players");
	}
// END: CustomMission members. Add the following inside your existing
// InvokeOnConnect AFTER super.InvokeOnConnect(player, identity).
/*
		if (m_RadioStations && identity)
		{
			string data = RadioSerializeStations();
			Param1<string> radioParam = new Param1<string>(data);
			GetGame().RPCSingleParam(null, 777001, radioParam, true, identity);
			Print("[RADIO] sent stations on connect to " + identity.GetName());
		}
*/

// File-scope models: paste OUTSIDE CustomMission at the end of init.c.
// ==== Loot.Team Radio: stations model ====
class RTStation
{
	string name;
	string url;
	int slotId;
	float frequency;
}
class RTStations
{
	ref array<ref RTStation> stations;

	void RTStations()
	{
		stations = new array<ref RTStation>;
	}
}
