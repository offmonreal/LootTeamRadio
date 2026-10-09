# LootTeamRadio — live radio for DayZ

> **A live-streaming radio mod for DayZ. Tune a handheld radio and listen to real internet streams.**

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/I0U227MY23)

**GitHub About:** The first live-streaming radio mod for DayZ — tune in and listen live.

**Release status:** source preview. The signed Workshop release and the public
`https://radio.loot.team/radio` stream are **not verified yet**. Do not assume
this repository or URL is ready for production. [Publishing checklist](docs/RELEASE.md).

## English

Hold an in-game **Personal Radio** with a battery, turn it on, and tune to a
station. The server sends eight fixed FM slots, including empty ones; the mod
writes a playlist into your DayZ profile and controls MPC-HC on *your own PC*.
Audio is personal — other players nearby do not hear the external player.
Empty channels display “Channel available.”

### Player setup (Windows)

1. Install and run [MPC-HC (clsid2)](https://github.com/clsid2/mpc-hc/releases).
2. In **View → Options → Player → Web Interface**, check **Listen on port**
   and set **13579**. Keep MPC-HC open while playing DayZ.
3. Subscribe to the mod **once the Workshop release is available** and enable
   it when connecting to a server that uses the matching version. Hold and
   switch on a battery-powered Personal Radio, then tune a configured frequency.

Chat: `!radio 50` sets personal player volume (0–100); `!radio help` shows
in-game help. The first time you switch on a radio, a short hint appears.
If MPC-HC is unavailable, an in-game hint displays the setup path. Radio
playback stops when you put the radio away, switch it off, die or exit normally.
A forcibly killed game cannot send a shutdown request to MPC-HC.

Server admins: see [server integration](docs/SERVER.md),
[build/signing](docs/BUILD.md), and [release checklist](docs/RELEASE.md).
One sample slot points to `https://radio.loot.team/radio` (only after the
stream is publicly verified); the other seven slots have empty URLs. Streaming
content rights are the responsibility of each server operator.

**Optional test server:** `loot.team:2302` — availability is **not guaranteed**;
only advertise it after checking access from outside the owner's network.

## Русский

Радио-мод для DayZ: удерживайте рацию **Personal Radio** с батарейкой,
включите её и настройтесь на волну. Сервер раздаёт восемь фиксированных частот,
включая пустые. Звук воспроизводится через MPC-HC **только на вашем ПК**, а не
в мире DayZ: другие игроки рядом его не слышат.

1. Установите и запустите [MPC-HC (clsid2)](https://github.com/clsid2/mpc-hc/releases).
2. **View → Options → Player → Web Interface**: поставьте галочку
   **Listen on port** и укажите **13579**.
3. После выпуска мода в Workshop подпишитесь на него, включите при входе
   на сервер с совместимой версией и возьмите включённую рацию в руки.

`!radio 50` — личная громкость 0–100; `!radio help` — справка в игре.
Первое включение показывает краткую подсказку; при отсутствии связи с плеером
мод подскажет, как его включить. Пустые частоты не воспроизводят звук.

Администратору: [встраивание серверной части](docs/SERVER.md),
[сборка и подпись](docs/BUILD.md), [проверка перед публикацией](docs/RELEASE.md).
Тестовый сервер `loot.team:2302` может быть недоступен.

## Links

- [MPC-HC downloads](https://github.com/clsid2/mpc-hc/releases)
- [Support on Ko-fi](https://ko-fi.com/I0U227MY23)
- Workshop: **not published yet**; add item URL only after release.

MIT licensed mod source. DayZ and MPC-HC remain property of their respective owners.
