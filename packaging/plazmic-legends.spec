Name:           plazmic-legends
Version:        0.1.3
Release:        1%{?dist}
Summary:        Read-only EverQuest Legends companion for Linux
License:        GPL-3.0-only
URL:            https://github.com/ChrisTitusTech/plazmic-legends
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  appstream
BuildRequires:  cmake >= 3.28
BuildRequires:  desktop-file-utils
BuildRequires:  gcc-c++
BuildRequires:  libX11-devel
BuildRequires:  ninja-build
BuildRequires:  python3 >= 3.11
BuildRequires:  qt6-qtbase-devel >= 6.8
BuildRequires:  xorg-x11-server-Xvfb
BuildRequires:  xorg-x11-xauth
Requires:       qt6-qtbase%{?_isa} >= 6.8

%description
Plazmic Legends is an independent Qt 6 companion window for the 64-bit
EverQuest Legends client running under Wine on Linux. It displays installed
map geometry and a bounded read-only snapshot for one exact supported client
profile. It does not inject into, write to, or modify the game or Wine prefix.

%prep
%autosetup

%build
%cmake \
    -G Ninja \
    -DBUILD_TESTING=ON \
    -DPLAZMIC_ENABLE_REPOSITORY_CHECKS=OFF
%cmake_build

%install
%cmake_install

%check
desktop-file-validate \
    %{buildroot}%{_datadir}/applications/plazmic-legends.desktop
appstreamcli validate --no-net \
    %{buildroot}%{_metainfodir}/io.github.ChristitusTech.PlazmicLegends.metainfo.xml
ctest --test-dir %{__cmake_builddir} --output-on-failure

%files
%{_bindir}/plazmic-legends
%{_datadir}/applications/plazmic-legends.desktop
%{_datadir}/icons/hicolor/512x512/apps/plazmic-legends.png
%{_metainfodir}/io.github.ChristitusTech.PlazmicLegends.metainfo.xml
%license %{_licensedir}/%{name}/LICENSE
%doc %{_docdir}/%{name}/README.md
%doc %{_docdir}/%{name}/development.md
%doc %{_docdir}/%{name}/package-operations.md
%doc %{_docdir}/%{name}/phase5-product-boundary.md
%doc %{_docdir}/%{name}/THIRD-PARTY-NOTICES.md

%changelog
* Sat Aug 01 2026 Chris Titus Tech <contact@christitus.com> - 0.1.3-1
- Add spawn category filters and independent map label controls
- Highlight named NPC spawns and distinguish ground or other markers

* Thu Jul 30 2026 Chris Titus Tech <contact@christitus.com> - 0.1.2-2
- License the original project under GPL-3.0-only
- Ship the license and contributor documentation

* Thu Jul 30 2026 Chris Titus Tech <contact@christitus.com> - 0.1.2-1
- Build the source archive without requiring Git in the COPR source chroot

* Thu Jul 30 2026 Chris Titus Tech <contact@christitus.com> - 0.1.1-1
- Build the SRPM from the exact SCM checkout and include the custom icon

* Thu Jul 30 2026 Chris Titus Tech <contact@christitus.com> - 0.1.0-1
- Initial Fedora COPR artifact
