# Try running Momo in P2P mode

Try running Momo in P2P mode, where it functions as a signaling server.

## Try streaming with Momo

```bash
./momo --no-audio-device p2p
```

For Windows:

```powershell
.\momo.exe --no-audio-device p2p
```

If Momo's IP address is 192.0.2.100, try connecting by accessing <http://192.0.2.100:8080/html/p2p.html> in Chrome.

## Try bidirectional streaming between Momos on a local network

- Make sure the machines running Momo are on the same network.
- Execute the command on each machine running Momo.
- SDL functionality is required to check the video. Therefore, please note that it cannot be used in a CUI environment.
For more information about SDL, please read [USE_SDL.md](USE_SDL.md).

Momo 1:

```bash
./momo --use-sdl p2p
```

Momo 2:

```bash
./momo --use-sdl ayame --signaling-url ws://[Momo 1's IP address]:8080/ws --room-id p2p
```

If you do not want to use Google STUN, you can enable it by adding the `--no-google-stun` option.

Momo 1:

```bash
./momo --no-google-stun　--use-sdl p2p
```

Momo 2:

```bash
./momo --no-google-stun --use-sdl ayame --signaling-url ws://[Momo 1's IP address]:8080/ws --room-id p2p
```

If the broadcast is successful, each machine will output video and audio from the other machine.

## Checking in P2P mode

Once the connection is successful, try running it using Ayame.

If you are using Ayame, please refer to [USE_AYAME.md](USE_AYAME.md).