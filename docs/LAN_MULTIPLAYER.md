# LAN multiplayer

Hosting and joining on one local network. No master server, no port forward, no
account, no CD key. The wire protocol is retail's: 66, unchanged -- a retail
1.10 client is still the same conversation.

The same engine code ships in both lanes (`app/src/main/cpp/code`): the Android
APK (Vulkan and GLES3) and the Linux x86_64 AppImage. Nothing in the discovery
path is platform-conditional; the ports, the broadcasts and the handshake are
identical.

---

## Hosting

Hosting is the retail host screen (`startserver`, which issues `map
<ui_startmap>`), or the console. The console recipe, on the hosting device:

```
set sv_maxclients 8
set g_gametype 1
set mp_gametype 2
map aah_ctf_tartirus
```

- `g_gametype 1` is **required**. `0` is `GT_SINGLE_PLAYER`, and a single-player
  server answers neither `getinfo` nor `getstatus` (`server/sv_main.c:213,262`
  return early) -- it is invisible to the LAN list and its details pane is empty,
  by design. A campaign map hosted that way is not a bug; it is a campaign.
- `mp_gametype` picks the match mode: `0` deathmatch (Holomatch), `1` team
  deathmatch, `2` capture the flag, `3` bomb diffusion
  (`fgame/mp_manager.cpp:419`).
- `sv_maxclients` is 8 by default in both the engine (`server/sv_init.c`) and the
  port's defaults seed. The old default was 1, which silently capped a listen
  server at one player.
- Map names are the `.bsp` names inside the retail multiplayer paks:
  `aah_ctf_tartirus` (pak `ctf_tartirus.pk3`), `ctf_vor2`, `dm_ctf_voy1`,
  `hm_ctf_exigent`. `map tartirus` does not resolve -- the pak carries
  `maps/aah_ctf_tartirus.bsp`.

`g_gametype` and `sv_maxclients` are `CVAR_LATCH` rows, so a console `set` prints
"will be changed upon restarting". A map spawn is that restart: the server
applies a parked value at the start of every spawn, so the three lines above in
that order do take effect. Setting them *after* the map loads needs a fresh
`map`.

The server binds the first free port of `12203`-`12212` (`net_port`, default
12203). The scan and direct connect both cover the whole range, so losing 12203
to another socket does not make a host invisible.

## Joining

Two ways, both masterless:

1. **The LAN list.** The retail browser's Local source (or the console
   `localservers`, or `setpingsource local`) sends `getinfo xxx` to the limited
   broadcast `255.255.255.255` and to each interface's subnet-directed broadcast,
   over ports 12203-12212, three rounds about 250 ms apart. The server answers
   `infoResponse` **unicast** to the asking address, so the AP's
   broadcast/multicast filtering -- the usual Android discovery problem -- does
   not apply to the reply. No `MulticastLock` is taken and none is needed.
2. **Direct connect.** `connect <ip>` fills in port 12203. When the address is a
   LAN IPv4 address and no port was given, it also probes 12204-12212 with the
   first `getchallenge`; whichever port answers becomes the server address. That
   is what makes `connect 192.168.5.20` work when the host is not on 12203.

LAN addresses skip CD-key authorisation entirely (client and server:
`Sys_IsLANAddress`), so there is no auth-server timeout on a local connect.

## No master

The internet list is the only path that needs a master server, and it is not
part of LAN play. A LAN list can be full while the master address is dead,
unresolvable, or pointed at the 2015 placeholder; neither affects hosting,
joining, or the local browser.

## Android notes

- The manifest declares `INTERNET`; that is what lets the engine open the UDP
  socket. No other permission is involved and there is no runtime prompt.
- Single player never opens a socket: the client and the in-process server talk
  through the in-memory loopback queues. A multiplayer host opens one on the
  first frame it is running (a server sends nothing first, so it cannot wait for
  the send path), and the client opens one when a LAN scan or a connect needs it.
- A refused socket is recoverable in place: `net_restart`.
- **Access points with client isolation** (guest, hotel, and some
  default-configured home routers) drop device-to-device traffic. Discovery and
  direct connect both fail there and nothing in the app can change it. A phone
  hotspot with the host on the hotspot side, or a normal home router, is the
  expected setup.

## Diagnostics

| Command | What it answers |
|---|---|
| `net_broadcasts` | the local address table, each mask, and the subnet broadcast derived from it; then the port range a scan would use |
| `net_restart` | close and reopen the UDP socket, clearing a latched refusal; prints `net_restart: socket open on port N` or the errno |
| `net_port` | the port the socket actually bound |
| `localservers` | run the LAN scan by hand (the console prints the scan line and a `Com_DPrintf` row per round with `developer 1`) |
| `connect <ip>` | direct join; with `developer 1` the log shows `Probing ports 12203-12212 on <ip>` and `... answered on port N` when a probe port won |
| `status` (host console) | one row per connected player and the `maxplayers` header |
| `serverstatus <ip>` | `getstatus` against a server, printing its settings |
| `showpackets 1` | one line per out-of-band send/receive -- the packet-level view of a scan or a join |

`developer 1` is what makes `Com_DPrintf` rows (round counts, ping times, packet
names) appear in the log; without it they are suppressed.

## Two-device test matrix

Two devices on one network -- two phones on one AP, or one phone hosting a
hotspot and the other joined to it. No master, no port forward.

| # | Test | Setup | Expected |
|---|---|---|---|
| T1 | Direct IP join (must pass) | A hosts the recipe above; B runs `connect <A-LAN-IP>` | B's console: `<ip> resolved to port 12203`, then `Probing ports 12203-12212 on <ip>`; `connectResponse`; the map loads. A's `status` lists B with a sane ping and no 10 s stall (LAN skips auth). |
| T2 | LAN list discovery | B opens the browser on the Local source (or runs `localservers`) | B's console: `Scanning for servers on the local network...`; A appears as hostname / map / clients / ping. With `developer 1`, `localservers: round 1/3 ... 10 ports` and `ping time Nms from port 12203`. |
| T3 | Gametype trap (expected to *fail* visibly) | A hosts a campaign map (or `g_gametype 0`) and repeats T2 | A does **not** appear. That is the single-player guard, not a bug. The host must run `g_gametype 1`. |
| T4 | Same-device sanity | A's own LAN list | A does not list itself (`NET_IsIPAddressLocal` hides own addresses). Expected; do not "fix". |
| T5 | Retail interop (optional, highest value) | A retail Windows 1.10 client joins A, and A joins a retail dedicated server | The only test that proves wire compatibility end to end. Protocol 66 is the only version gate. |

If T2 fails everywhere but T1 passes, the split is send vs. receive: `developer
1` plus `showpackets 1` on both devices shows whether the `getinfo` left B and
whether the `infoResponse` came back. `net_broadcasts` on B shows whether the
subnet broadcast is the right address for the network B is on.

## Known limits

- The dedicated server's game clock follows the wall clock. It used to run at
  the speed of the engine's spin loop: `NET_Sleep` -- the wait that is the
  remainder of a server frame `SV_Frame` cannot fill (retail 0x001E1194) -- was
  a no-op, and `Com_Frame` excused a dedicated frame from waiting out its
  millisecond, so every pass the loop could make was handed a millisecond of
  server time (`Com_ModifyMsec`'s floor). A 20-minute `mp_timelimit` expired in
  about 1.3 s and the server respawned the same map forever. The wait now really
  sleeps and a dedicated frame waits its millisecond like every other frame.
  Verified on the Linux build: a 20 s run stays on one spawn, and `mp_timelimit
  1` ends the match at 60 s of server time (70 s to the next spawn: the 10 s
  end-of-match intermission).
- The match ends after `mp_timelimit` (20 minutes by default), plays the
  end-of-match intermission, and reloads the same map. That is the retail
  behaviour for an empty `nextmap`.
- Buildings with client isolation cannot be fixed from software.
- The internet (master) list, retail server heartbeats and replacement masters
  are a separate subject: `personal/handover/ef2-multiplayer-lan-plan.md`.

## What was verified, and how

In the development sandbox, on the Linux build of this same source: a dedicated
server bound 12203, a host on 12203 and a host on 12205 both answered real
`getinfo` / `getstatus` / `getchallenge` datagrams over UDP loopback; a client
process joined a dedicated server on 12205 by `connect 127.0.0.1`, was re-pointed
by the port probe (`answered on port 12205`), completed the challenge, received
`connectResponse` and the gamestate, and loaded the map; and a listen server
inside the client process answered `getinfo` with `clients\1` and
`sv_maxclients\8` while its own client was connected. `net_broadcasts` printed
`192.168.5.255` for a 192.168.5.15/24 host and skipped both the loopback and a
/32 interface. A dedicated host on 12203 answered `getinfo` (`protocol\66`,
`sv_maxclients\8`), `getstatus` (`maxplayers\8`) and `getchallenge` in under
50 ms each, with 12204 silent while 12203 was bound, and stayed on one spawn for
the whole run; the same on `ctf_vor2` and `dm_ctf_voy1`.

Not verifiable here: two devices on one access point, Wi-Fi broadcast delivery
on a given AP, and the Android runtime itself. That is the operator's T1-T5 run
above.
