# Building fidelis from source

fidelis is C++23 + a Svelte web frontend. Both build via system libraries and a vendored single-header subset; no fetching at configure time.

## Required system packages

### Arch Linux

```
sudo pacman -S meson ninja pkg-config gcc \
  alsa-lib flac mpg123 libvorbis opus \
  sqlite sdbus-cpp \
  systemd-libs \
  nodejs npm
```

### Debian / Ubuntu (24.04 or newer)

```
sudo apt install meson ninja-build pkg-config g++ \
  libasound2-dev libflac-dev libmpg123-dev libvorbis-dev libopus-dev \
  libsqlite3-dev libsdbus-c++-dev libudev-dev \
  nodejs npm
```

### Fedora

```
sudo dnf install meson ninja-build pkgconf gcc-c++ \
  alsa-lib-devel flac-devel mpg123-devel libvorbis-devel opus-devel \
  sqlite-devel sdbus-cpp-devel systemd-devel \
  nodejs npm
```

Vendored sources under `third_party/` (Apple ALAC, doctest, cpp-httplib, nlohmann/json, toml++) are GPL-3-compatible and not external requirements.

## Build steps

```
git clone https://github.com/neofytr/fidelis.git
cd fidelis

# Web bundle (committed under web/dist/; rebuild if you edit the frontend)
( cd web && npm ci && npm run build )

# Daemon
meson setup build
ninja -C build
```

Resulting binary: `build/fidelis`.

## Install

```
sudo ninja -C build install
```

Installs the binary to `/usr/local/bin/fidelis` (`prefix=/usr` and `DESTDIR=` override the usual way). The web bundle is read at runtime from the path in `[web].static_dir` — the daemon assumes `./web/dist` by default. Point `static_dir` at where you placed the bundle for system-wide installs.

## Realtime audio

The audio thread requests `SCHED_FIFO` priority 80 with `mlockall`. Grant the capability without root:

```
sudo gpasswd -a "$USER" audio
sudo install -m 0644 packaging/limits-fidelis.conf /etc/security/limits.d/99-fidelis.conf
```

Log out and back in. The Pipeline page reports `RT: SCHED_FIFO` once granted, `SCHED_OTHER` otherwise.

## systemd user unit

```
mkdir -p ~/.config/systemd/user
cp packaging/fidelis.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now fidelis
loginctl enable-linger "$USER"      # survive logout
```

## Tests

```
meson test -C build
```

Suites:

- default — pure / mock-engine tests; no hardware. CI runs this.
- `needs-alsa` — exercises a real ALSA device. Skip with `--no-suite needs-alsa`.
- `needs-loopback` — bit-perfect loopback verification on a real DAC. Skip with `--no-suite needs-loopback`.

## Sandbox / hard-exclusive DAC access

fidelis opens the DAC with `O_EXCL` semantics. PipeWire / PulseAudio cannot share. A udev rule keeps PipeWire from claiming a specific device — adapt VID/PID for yours:

```
# /etc/udev/rules.d/90-fidelis-exclusive.rules
SUBSYSTEM=="sound", ATTRS{idVendor}=="2fc6", ATTRS{idProduct}=="f082", \
    ENV{ACP_IGNORE}="1", ENV{PULSE_IGNORE}="1"
```

```
sudo udevadm control --reload && sudo udevadm trigger
```
