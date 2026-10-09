# Server integration / Встраивание на сервер

**Do not replace your entire `mpmissions/<mission>/init.c`.**
Back it up first. `server/mission-radio-snippet.c` is a **reference snippet**;
it is not a standalone mod script or ready-to-replace mission file. The
client mod alone cannot configure a server mission.

1. Inside your existing `CustomMission: MissionServer`, merge the fields and
   methods from `server/mission-radio-snippet.c`. Integrate the shown `OnEvent`
   and `OnInit` logic into existing overrides, calling `super` only once.
   Keep unrelated mission hooks (spawn equipment, character creation, etc.).
   Add the marked station RPC block to your existing `InvokeOnConnect` after
   `super.InvokeOnConnect`; merge into an existing override if one exists.
   Place `RTStation` and `RTStations` classes at file scope.
2. Replace `REPLACE_WITH_ADMIN_STEAM_ID` with the **server admin's own** Steam
   ID. Keep operator-specific IDs out of public source control. Chat commands
   resolve player identity from the server-side player list; duplicate names
   are rejected. Administrators must not grant admin commands to arbitrary users.
3. Put `server/radio-stations.json` into `$mission:radio-stations.json`.
   On the first successful start it is copied to
   `$profile:DayZRadio/radio-stations.json` — **later edit that profile copy**
   (the mission file is only an initial default). Server validates exactly
   eight fixed frequencies; only slots with both a name and URL play. The
   example HTTPS URL must work from each client's PC before activation.
4. Install the matching signed client mod on server and distribute its `.bikey`
   into the server `keys/` directory. With all required mods signed, use
   `verifySignatures=2`; do not disable verification for public servers.
5. Restart gracefully; check server RPT for `[RADIO] stations loaded: 8` and
   script errors. Test with a separate Windows client. Back up profile
   configuration and binaries before any release migration; rollback if
   script compilation fails. The example mod does **not** start eight Icecast
   processes: the seven empty slots remain intentionally silent.

### Admin chat commands

- `!radio set 2,https://example.org/live,Station name` — updates slot 2.
- `!radio clear 2` — clears slot 2.
- `!radio reload` — validates and broadcasts current profile configuration.

Each update broadcasts all eight slots; active clients rebuild local playlist
and reconnect the stream if tuned to the updated station. The server keeps a
backup at `$profile:DayZRadio/radio-stations.backup.json`.
Players use `!radio 50` / `!radio volume 50` and `!radio help`.

**Caveat:** standalone mission snippets cannot be compiled directly; test the
merged mission in DayZ on a non-production server before changing the live
mission. Obtain content rights and check stream capacity before advertising.
