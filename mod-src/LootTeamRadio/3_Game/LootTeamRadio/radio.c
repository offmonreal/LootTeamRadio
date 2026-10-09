// Loot.Team Radio — client-side state + MPC-HC controller (Phase 3)
// Сервер раздаёт карту частот (RPC 777001), мод:
//  1) пишет $profile:DayZRadio/stations.m3u (плейлист = слоты частот);
//  2) находит абсолютный путь через API MPC-HC (browse.json);
//  3) открывает плейлист и управляет: index/play/volume/pause.
// Игрок только ставит MPC-HC и включает Web interface (порт 13579).

class LootRadioClientStation
{
	int slotId;
	float frequency;
	string name;
	string url;
}

class LootRadioClientConfig
{
	ref array<ref LootRadioClientStation> stations;

	void LootRadioClientConfig()
	{
		stations = new array<ref LootRadioClientStation>();
	}
}

// HTTP callbacks run off the game update thread; never use GET_now for MPC-HC.
class LootRadioHttpCallback : RestCallback
{
	string m_Query;
	bool m_StatusProbe;

	void LootRadioHttpCallback(string query, bool statusProbe)
	{
		m_Query = query;
		m_StatusProbe = statusProbe;
	}

	override void OnSuccess(string data, int dataSize)
	{
		LootRadioState.OnMpcResponse(this, m_Query, m_StatusProbe, data);
	}

	override void OnError(int errorCode)
	{
		LootRadioState.OnMpcError(this, m_Query, m_StatusProbe);
	}

	override void OnTimeout()
	{
		LootRadioState.OnMpcError(this, m_Query, m_StatusProbe);
	}
}

class LootRadioState
{
	// MPC-HC on the same PC; Linux test host forwards this port to the Windows VM.
	static const string MPC_BASE = "http://127.0.0.1:13579/";
	static const int MUSIC_RPC_ID = 777001;
	static const int DEFAULT_VOLUME = 100;
	static int m_UserVolume = 100;
	static bool m_VolumeLoaded = false;
	static bool m_FirstUseChecked = false;
	static bool m_SessionInitialized = false;
	static bool m_ProbePending = false;
	static int m_ProbeStartedAt = 0;
	static ref LootRadioHttpCallback m_StatusCallback;
	static ref LootRadioHttpCallback m_BrowseCallback;
	static ref array<ref LootRadioHttpCallback> m_CommandCallbacks;
	static string m_DiscoveryUser = "";
	static int m_DiscoveryIndex = 0;
	static bool m_DiscoveryPending = false;
	static int m_DiscoveryStartedAt = 0;
	static ref array<string> m_DiscoveryUsers;
	static bool m_PlayerReachable = false;
	static bool m_ForceReload = false;
	static int m_PlaylistRevision = 0;
	static int m_OpenedRevision = -1;
	static string m_LastSnapshot = "";
	static int m_LastAttemptMillis = 0;
	static bool m_HasAttempted = false;
	static bool m_PausedForNoRadio = false;
	static ref array<int> s_AllFrequencies;           // 8 fixed frequency slots (including empty)
	static ref map<int, int> s_SlotIds;               // freqKey -> server slot id

	static bool IsRussianUI()
	{
		GameOptions options = new GameOptions();
		ListOptionsAccess language = ListOptionsAccess.Cast(options.GetOptionByType(OptionAccessType.AT_OPTIONS_LANGUAGE));
		if (!language) return false;
		string langName;
		language.GetItemText(language.GetIndex(), langName);
		langName.ToLower();
		return langName.Contains("russian") || langName.Contains("русск");
	}

	static void ShowFirstUseHelp()
	{
		if (m_FirstUseChecked) return;
		m_FirstUseChecked = true;
		string marker = "$profile:DayZRadio/intro-shown.txt";
		if (FileExist(marker)) return;
		MakeDirectory("$profile:DayZRadio");
		FileHandle f = OpenFile(marker, FileMode.WRITE);
		if (f != 0)
		{
			FPrintln(f, "1");
			CloseFile(f);
		}
		if (IsRussianUI())
			NotificationSystem.AddNotificationExtended(8.0, "Радио: громкость !radio 50", "От 0 до 100. Справка: !radio help");
		else
			NotificationSystem.AddNotificationExtended(8.0, "Radio volume: !radio 50", "Range 0–100. Help: !radio help");
	}

	static void ShowHelp()
	{
		int newlineCode = 10;
		string br = newlineCode.AsciiToString();
		if (IsRussianUI())
			NotificationSystem.AddNotificationExtended(18.0, "Радио: справка", "Рация с батарейкой: в руки, включить, выбрать FM" + br + "Громкость: !radio 50 (0–100)" + br + "Плеер: github.com/clsid2/mpc-hc/releases" + br + "Options > Player > Web Interface" + br + "Listen on port [x]: 13579");
		else
			NotificationSystem.AddNotificationExtended(18.0, "Radio: help", "Battery radio: hold, turn on, tune FM" + br + "Volume: !radio 50 (0–100)" + br + "Player: github.com/clsid2/mpc-hc/releases" + br + "Options > Player > Web Interface" + br + "Listen on port [x]: 13579");
	}

	static void SetUserVolume(int volume)
	{
		if (volume < 0 || volume > 100) return;
		m_UserVolume = volume;
		if (m_CurrentSlot >= 0) SendVolume(volume);
		MakeDirectory("$profile:DayZRadio");
		FileHandle f = OpenFile("$profile:DayZRadio/volume.txt", FileMode.WRITE);
		if (f != 0)
		{
			FPrintln(f, volume.ToString());
			CloseFile(f);
		}
	}

	static void LoadUserVolume()
	{
		if (m_VolumeLoaded) return;
		m_VolumeLoaded = true;
		string value;
		FileHandle f = OpenFile("$profile:DayZRadio/volume.txt", FileMode.READ);
		if (f == 0) return;
		FGets(f, value);
		CloseFile(f);
		if (value.Length() < 1 || value.Length() > 3) return;
		for (int digitIdx = 0; digitIdx < value.Length(); digitIdx++)
		{
			string digit = value.Substring(digitIdx, 1);
			if (digit.ToInt().ToString() != digit) return;
		}
		int loaded = value.ToInt();
		if (loaded >= 0 && loaded <= 100) m_UserVolume = loaded;
	}

	static void MarkPlayerOffline()
	{
		m_PlayerReachable = false;
		m_OpenedRevision = -1;
		m_CurrentSlot = -1;
		m_LastVolumeSent = -1;
		m_PausedForNoRadio = true;
		m_DiscoveryPending = false;
		m_DiscoveryUsers = null;
		m_BrowseCallback = null;
	}

	static void OnMpcError(LootRadioHttpCallback cb, string query, bool statusProbe)
	{
		if (statusProbe)
		{
			if (m_StatusCallback != cb) return;
			m_ProbePending = false;
			m_StatusCallback = null;
			MarkPlayerOffline();
		}
		else if (query.IndexOf("browse.json?") == 0)
		{
			if (m_BrowseCallback != cb) return;
			m_DiscoveryPending = false;
			m_DiscoveryUsers = null;
			m_BrowseCallback = null;
		}
		else if (m_CommandCallbacks)
		{
			m_CommandCallbacks.RemoveItem(cb);
			if (query.IndexOf("browser.html?") == 0) m_OpenedRevision = -1;
		}
	}

	static void OnMpcResponse(LootRadioHttpCallback cb, string query, bool statusProbe, string data)
	{
		if (!statusProbe && query.IndexOf("browse.json?") != 0 && m_CommandCallbacks)
			m_CommandCallbacks.RemoveItem(cb);
		if (statusProbe)
		{
			if (m_StatusCallback != cb) return;
			m_ProbePending = false;
			m_StatusCallback = null;
			if (!data.Contains("stateString"))
			{
				MarkPlayerOffline();
				return;
			}
			if (!m_PlayerReachable)
			{
				m_PlayerReachable = true;
				m_OpenedRevision = -1;
				m_CurrentSlot = -1;
				m_HasAttempted = false;
				MpcGet("command.html?wm_command=888");
			}
			return;
		}
		if (query.IndexOf("browse.json?") != 0) return;
		if (m_BrowseCallback != cb) return;
		m_DiscoveryPending = false;
		m_BrowseCallback = null;
		if (query == "browse.json?path=C:/Users/")
		{
			m_DiscoveryUsers = new array<string>();
			int brace = data.IndexOf("{");
			if (brace < 0 || brace + 1 >= data.Length()) return;
			string quoteChar = data.Substring(brace + 1, 1);
			string rest = data;
			while (true)
			{
				string name = ExtractJsonStringField(rest, quoteChar, "name");
				if (name == "") break;
				int cut = rest.IndexOf(name);
				if (cut < 0) break;
				rest = rest.Substring(cut + name.Length(), rest.Length() - cut - name.Length());
				string lower = name;
				lower.ToLower();
				if (!lower.Contains("public") && !lower.Contains("default") && !lower.Contains("all users"))
					m_DiscoveryUsers.Insert(name);
			}
			m_DiscoveryIndex = 0;
			ProbeNextDirectory();
			return;
		}
		if (data.Contains("stations.m3u"))
		{
			int marker = query.IndexOf("path=");
			if (marker >= 0 && query.Length() > marker + 6)
			{
				string dirPath = query.Substring(marker + 5, query.Length() - marker - 6);
				m_PlaylistPath = dirPath + "/stations.m3u";
				m_DiscoveryUsers = null;
				Print("[LootRadio] playlist discovered: " + m_PlaylistPath);
				// A delayed browse reply must not start playback after the radio was switched off.
			}
			return;
		}
		ProbeNextDirectory();
	}

	static void ProbeNextDirectory()
	{
		if (!m_DiscoveryUsers || m_DiscoveryIndex >= m_DiscoveryUsers.Count() * 2)
		{
			m_DiscoveryUsers = null;
			return;
		}
		int userIndex = m_DiscoveryIndex / 2;
		string user = m_DiscoveryUsers[userIndex];
		string path = "C:/Users/" + user + "/Documents/DayZ/DayZRadio/";
		if (m_DiscoveryIndex % 2 == 1)
			path = "C:/Users/" + user + "/AppData/Local/DayZ/DayZRadio/";
		m_DiscoveryIndex++;
		MpcGet("browse.json?path=" + path);
		m_DiscoveryPending = true;
		m_DiscoveryStartedAt = GetGame().GetTime();
	}

	static void CheckPlayerConnection()
	{
		if (m_ProbePending)
		{
			if (GetGame().GetTime() - m_ProbeStartedAt < 15000) return;
			m_ProbePending = false;
			m_StatusCallback = null;
			MarkPlayerOffline();
		}
		RestContext ctx = GetRestApi().GetRestContext(MPC_BASE);
		if (!ctx)
		{
			MarkPlayerOffline();
			return;
		}
		m_StatusCallback = new LootRadioHttpCallback("status.json", true);
		m_ProbePending = true;
		m_ProbeStartedAt = GetGame().GetTime();
		ctx.GET(m_StatusCallback, "status.json");
	}

	static void BeginSession()
	{
		if (m_SessionInitialized) return;
		m_SessionInitialized = true;
		LoadUserVolume();
		m_PausedForNoRadio = true;
		CheckPlayerConnection();
	}

	static void StopSession()
	{
		if (m_PlayerReachable) MpcGet("command.html?wm_command=888");
		m_CurrentSlot = -1;
		m_LastVolumeSent = -1;
		m_OpenedRevision = -1;
		m_PausedForNoRadio = true;
		m_DiscoveryPending = false;
		m_DiscoveryUsers = null;
	}

	static ref map<int, string> s_Stations;          // active freqKey -> url
	static ref map<int, string> s_Names;             // freqKey -> name
	static ref array<int> s_FreqOrder;               // отсортированные freqKey = индексы плейлиста
	static string m_PlaylistPath = "";               // абсолютный путь stations.m3u (найден через API)
	static string m_PlaylistSignature = "";          // чтобы не перезаписывать без изменений
	static int m_CurrentSlot = -1;
	static int m_LastVolumeSent = -1;
	static bool m_DbgHands = false;
	static int m_LastFrequencyIndex = -1;

	// ---------- карта станций (RPC от сервера) ----------

	static void SetStationsFromString(string data)
	{
		BeginSession();
		if (data == m_LastSnapshot) return;
		array<string> incoming = new array<string>();
		data.Split(";", incoming);
		if (incoming.Count() != 8)
		{
			Print("[LootRadio] invalid station snapshot, keeping current stations");
			return;
		}
		if (m_CurrentSlot >= 0 || m_LastVolumeSent != -1) StopSession(); // stop old stream before replacing playlist
		m_CurrentSlot = -1;
		m_OpenedRevision = -1;
		m_ForceReload = true;
		m_HasAttempted = false;
		m_PausedForNoRadio = true;
		s_AllFrequencies = new array<int>();
		s_SlotIds = new map<int, int>();
		s_Stations = new map<int, string>();
		s_Names = new map<int, string>();
		s_FreqOrder = new array<int>();

		foreach (string st : incoming)
		{
			array<string> parts = new array<string>();
			st.Split("|", parts);
			if (parts.Count() == 4)
			{
				int slotId = parts[0].ToInt();
				int freqKey = parts[1].ToInt();
				if (slotId < 1 || slotId > 8 || freqKey <= 0 || s_SlotIds.Contains(freqKey)) continue;
				s_AllFrequencies.Insert(freqKey);
				s_SlotIds.Insert(freqKey, slotId);
				if (parts[3] != "-")
				{
					s_Stations.Insert(freqKey, parts[3]);
					s_Names.Insert(freqKey, parts[2]);
				}
			}
		}

		// стабильный порядок слотов = по возрастанию freqKey
		array<int> keys = new array<int>();
		for (int i = 0; i < s_Stations.Count(); i++)
			keys.Insert(s_Stations.GetKey(i));
		while (keys.Count() > 0)
		{
			int best = 0;
			for (int j = 1; j < keys.Count(); j++)
				if (keys[j] < keys[best]) best = j;
			s_FreqOrder.Insert(keys[best]);
			keys.Remove(best);
		}

		if (s_AllFrequencies.Count() != 8)
		{
			Print("[LootRadio] incomplete station snapshot; ignoring");
			m_LastSnapshot = data;
			return;
		}
		m_LastSnapshot = data;
		Print("[LootRadio] active stations: " + s_FreqOrder.Count().ToString());
		WriteStationCopy();
		WritePlaylistFile();
	}

	static int GetSlotByFreqKey(int freqKey)
	{
		if (!s_FreqOrder)
			return -1;
		return s_FreqOrder.Find(freqKey);
	}

	// Client-side copy of the complete 8-slot server snapshot.
	static void WriteStationCopy()
	{
		MakeDirectory("$profile:DayZRadio");
		LootRadioClientConfig copy = new LootRadioClientConfig();
		foreach (int freq : s_AllFrequencies)
		{
			LootRadioClientStation station = new LootRadioClientStation();
			station.slotId = s_SlotIds.Get(freq);
			station.frequency = freq * 0.1;
			station.name = "";
			station.url = "";
			if (s_Stations.Contains(freq))
			{
				station.name = s_Names.Get(freq);
				station.url = s_Stations.Get(freq);
			}
			copy.stations.Insert(station);
		}
		string error;
		if (!JsonFileLoader<LootRadioClientConfig>.SaveFile("$profile:DayZRadio/radio-stations.json", copy, error))
			Print("[LootRadio] cannot write local config: " + error);
	}

	// ---------- запись плейлиста в $profile ----------

	static void WritePlaylistFile()
	{
		if (!s_FreqOrder) return;

		string signature = "";
		foreach (int fk : s_FreqOrder)
			signature = signature + fk.ToString() + "=" + s_Stations.Get(fk) + ";";
		if (signature == m_PlaylistSignature && !m_ForceReload)
			return;

		MakeDirectory("$profile:DayZRadio");
		FileHandle f = OpenFile("$profile:DayZRadio/stations.m3u", FileMode.WRITE);
		if (f == 0)
		{
			Print("[LootRadio] cannot write stations.m3u");
			return;
		}
		FPrintln(f, "#EXTM3U");
		foreach (int fkey : s_FreqOrder)
		{
			float freq = fkey * 0.1;
			FPrintln(f, "#EXTINF:-1," + s_Names.Get(fkey) + " (" + freq.ToString() + " MHz)");
			FPrintln(f, s_Stations.Get(fkey));
		}
		CloseFile(f);
		m_PlaylistSignature = signature;
		m_ForceReload = false;
		m_PlaylistRevision++;
		m_CurrentSlot = -1;
		// Absolute path stays valid; MPC must reopen the file to pick up new content.
		Print("[LootRadio] stations.m3u written, slots=" + s_FreqOrder.Count().ToString());
	}

	// ---------- MPC-HC HTTP ----------

	static void MpcGet(string query)
	{
		RestContext ctx = GetRestApi().GetRestContext(MPC_BASE);
		if (!ctx) return;
		LootRadioHttpCallback cb = new LootRadioHttpCallback(query, false);
		if (query.IndexOf("browse.json?") == 0) m_BrowseCallback = cb;
		else
		{
			if (!m_CommandCallbacks) m_CommandCallbacks = new array<ref LootRadioHttpCallback>();
			m_CommandCallbacks.Insert(cb);
		}
		ctx.GET(cb, query); // asynchronous: never freeze the DayZ update thread
	}

	// ищем абсолютный путь stations.m3u через browse.json самого плеера
	static string ExtractJsonStringField(string json, string q, string fieldName)
	{
		string needle = q + fieldName + q + ":" + q;
		int pos = json.IndexOf(needle);
		if (pos < 0)
			return "";
		int start = pos + needle.Length();
		int end = json.IndexOfFrom(start, q);
		if (end < 0)
			return "";
		return json.Substring(start, end - start);
	}

	static void OpenPlaylist()
	{
		if (m_PlaylistPath == "" || !m_PlayerReachable) return;
		MpcGet("browser.html?path=" + m_PlaylistPath);
		m_OpenedRevision = m_PlaylistRevision;
		m_CurrentSlot = -1;
		m_LastVolumeSent = -1;
		m_PausedForNoRadio = false;
	}

	// ---------- публичное API (вызывается из MissionGameplay) ----------

	static void PlayStation(int freqKey)
	{
		BeginSession();
		if (!m_PlayerReachable) return;
		int slot = GetSlotByFreqKey(freqKey);
		if (slot < 0)
		{
			SetVolume0();
			return;
		}
		if (m_DiscoveryPending && GetGame().GetTime() - m_DiscoveryStartedAt >= 15000)
		{
			m_DiscoveryPending = false;
			m_DiscoveryUsers = null;
			m_BrowseCallback = null;
		}
		if (m_PlaylistPath == "" && !m_DiscoveryPending)
		{
			int now = GetGame().GetTime();
			if (m_HasAttempted && now - m_LastAttemptMillis < 5000) return;
			m_HasAttempted = true;
			m_LastAttemptMillis = now;
			m_DiscoveryPending = true;
			m_DiscoveryStartedAt = now;
			MpcGet("browse.json?path=C:/Users/");
			return;
		}
		if (m_PlaylistPath == "" || m_DiscoveryPending) return;
		if (m_OpenedRevision != m_PlaylistRevision) OpenPlaylist();
		if (m_CurrentSlot != slot)
		{
			MpcGet("command.html?wm_command=-3&index=" + slot.ToString());
			MpcGet("command.html?wm_command=887");
			m_CurrentSlot = slot;
			m_PausedForNoRadio = false;
		}
		SendVolume(m_UserVolume);
	}

	static void SendVolume(int vol100)
	{
		if (m_LastVolumeSent == vol100)
			return;
		m_LastVolumeSent = vol100;
		MpcGet("command.html?wm_command=-2&volume=" + vol100.ToString());
	}

	static void SetVolume0()
	{
		m_CurrentSlot = -1;   // next active tuning must select and play again
		if (m_PausedForNoRadio) return;
		m_PausedForNoRadio = true;
		m_LastVolumeSent = -1;
		MpcGet("command.html?wm_command=888");   // pause
	}
}
