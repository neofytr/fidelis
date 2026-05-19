Name:           fidelis
Version:        1.0.0
Release:        1%{?dist}
Summary:        Bit-perfect Linux music player for external USB DACs

License:        GPL-3.0-or-later
URL:            https://github.com/neofytr/fidelis
Source0:        https://github.com/neofytr/fidelis/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  meson >= 1.1
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(flac)
BuildRequires:  pkgconfig(libmpg123)
BuildRequires:  pkgconfig(vorbisfile)
BuildRequires:  pkgconfig(opusfile)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(sdbus-c++)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  nodejs
BuildRequires:  npm
BuildRequires:  systemd-rpm-macros

Requires:       alsa-lib
Requires:       flac-libs
Requires:       mpg123-libs
Requires:       libvorbis
Requires:       opus
Requires:       sqlite-libs
Requires:       sdbus-cpp
Requires:       systemd-libs

%description
fidelis plays audio files from disk straight to an external USB DAC
through ALSA, with no sound server in the audio path, no resampling,
no DSP, and no software volume. The DAC is held exclusive while
playing; its sample rate is switched per track. Every stage of the
decode -> ring -> ALSA pipeline is exposed by a local web UI, with a
live three-state bit-perfect verdict.

%prep
%autosetup

%build
( cd web && npm ci && npm run build )
%meson
%meson_build

%install
%meson_install

install -Dm644 packaging/fidelis.service \
    %{buildroot}%{_userunitdir}/fidelis.service
install -Dm644 packaging/limits-fidelis.conf \
    %{buildroot}%{_sysconfdir}/security/limits.d/99-fidelis.conf
install -Dm644 packaging/fidelis.1 \
    %{buildroot}%{_mandir}/man1/fidelis.1

# Web bundle served by the daemon.
install -d %{buildroot}%{_datadir}/%{name}/web
cp -r web/dist %{buildroot}%{_datadir}/%{name}/web/

%check
%meson_test --no-suite needs-alsa --no-suite needs-loopback

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/fidelis
%{_userunitdir}/fidelis.service
%{_sysconfdir}/security/limits.d/99-fidelis.conf
%{_mandir}/man1/fidelis.1*
%{_datadir}/%{name}/

%changelog
* Tue May 19 2026 neofytr <imagination12344321.com@gmail.com> - 1.0.0-1
- Initial release.
