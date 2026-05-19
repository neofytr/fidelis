# fidelis

Bit-perfect Linux music player for external USB DACs. ALSA-direct: decoder → `hw:` → DAC. No sound server in the audio path, no resampling, no DSP, no software volume.

The DAC is held exclusive while playing. Sample rate switches per track. The Pipeline page in the UI shows the live state of every stage and a three-state bit-perfect verdict.

PCM only in 1.0. DSD and CUE land in 1.1.

## Build

Arch:

```
sudo pacman -S meson ninja pkg-config gcc \
  alsa-lib flac mpg123 libvorbis opus \
  sqlite sdbus-cpp systemd-libs nodejs npm

git clone https://github.com/neofytr/fidelis
cd fidelis
( cd web && npm ci && npm run build )
meson setup build
ninja -C build
```

Debian and Fedora package lists are in [`BUILDING.md`](BUILDING.md).

## Run

```
./build/fidelis
```

UI at <http://localhost:7800>. First connected USB DAC is selected automatically; switch via the **DAC** button.

Config lives at `~/.config/fidelis/config.toml`:

```toml
[device]
preferred = "hw:CARD=SE,DEV=0"   # or a stable fingerprint id (DAC picker writes this)

[audio]
period_ms    = 12
period_count = 4
rt_policy    = "auto"            # auto | fifo | other

[library]
roots = ["/home/user/Music"]
```

## Keys

| Key       | Action           |
|-----------|------------------|
| Space     | play / pause     |
| ← / →     | seek ∓10s        |
| m         | mute toggle (hw) |
| − / =     | volume ∓3% (hw)  |

Volume keys drive the DAC's own mixer (e.g. `PCM,0` on an iFi) via ALSA. There is no software volume.

## CLI

```
fidelis ctl status | play | pause | toggle | next | prev | clear
fidelis ctl enqueue /path/to/track.flac
```

Bind these to media keys in your WM for global control.

## systemd

```
mkdir -p ~/.config/systemd/user
cp packaging/fidelis.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now fidelis
loginctl enable-linger "$USER"   # keep running across logout
```

## Realtime audio

```
sudo gpasswd -a "$USER" audio
sudo install -m 0644 packaging/limits-fidelis.conf \
  /etc/security/limits.d/99-fidelis.conf
```

Log out and back in. The Pipeline page shows whether `SCHED_FIFO` was granted.

## Exclusive DAC access

`fidelis` opens the DAC `O_EXCL`. PipeWire can't share. To stop PipeWire claiming a specific device (replace VID/PID):

```
# /etc/udev/rules.d/90-fidelis-exclusive.rules
SUBSYSTEM=="sound", ATTRS{idVendor}=="2fc6", ATTRS{idProduct}=="f082", \
    ENV{ACP_IGNORE}="1", ENV{PULSE_IGNORE}="1"
```

```
sudo udevadm control --reload && sudo udevadm trigger
```

## Tests

```
meson test -C build
```

`needs-alsa` and `needs-loopback` suites need real hardware and are skipped otherwise.

## License

GPL-3.0-or-later.
