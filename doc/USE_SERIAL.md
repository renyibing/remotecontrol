# Try serial read/write via data channel

**This feature is currently only available in p2p and ayame modes**

Here, we'll try it using [socat](http://www.dest-unreach.org/socat/).

[Creating a virtual serial port with socat \- Qiita](https://qiita.com/uhey22e/items/dc41d7fa1075970e66a1)

socat is assumed to be installed.

Create two virtual serial ports.

```console
$ socat -d -d pty,raw,echo=0 pty,raw,echo=0
2020/01/18 18:47:21 socat[71342] N PTY ​​is /dev/ttys003
2020/01/18 18:47:21 socat[71342] N PTY ​​is /dev/ttys004
2020/01/18 18:47:21 socat[71342] N starting data transfer loop with FDs [5,5] and [7,7]
```

Two internally connected virtual serial ports, /dev/ttys003 and /dev/ttys004, have been created.

Next, connect Momo to /dev/ttys003.

```bash
./momo --serial /dev/ttys003,9600 p2p
```

Display the data written to /dev/ttys004.

```bash
cat < /dev/ttys004
```

Access <http://127.0.0.1:8080/html/p2p.html>.

Send some string in the send section and verify that it is successfully displayed via ttys004.

Next, write to /dev/ttys004 and

```bash
echo "Hello, world" > /dev/ttys004
```

Verify that it is displayed in the JavaScript console at <http://127.0.0.1:8080/html/p2p.html>.

## Reference video

[![Image from Gyazo](https://i.gyazo.com/c1fb6696963e044a44576b1ddeffd0cb.gif)](https://gyazo.com/c1fb6696963e044a44576b1ddeffd0cb)

[![Image from Gyazo](https://i.gyazo.com/269ccc2290b43809a0e67e35c03e8601.gif)](https://gyazo.com/269ccc2290b43809a0e67e35c03e8601)