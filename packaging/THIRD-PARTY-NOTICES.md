# Third-party notices

Plazmic Legends uses or packages the following independently maintained
components. This notice does not replace the license texts shipped by their
respective Fedora packages or included in the AppImage.

## RPM runtime boundary

The Fedora RPM dynamically links to Fedora-provided Qt 6, X11, OpenGL dispatch,
the GNU C and C++ runtimes, and their dependency closure. The RPM does not copy
those system libraries. Fedora installs their authoritative notices under
`/usr/share/licenses` and `/usr/share/doc`.

Qt 6 is available under commercial terms or the GNU Lesser General Public
License version 3 and GNU General Public License versions 2 or 3, depending on
the module. Plazmic uses dynamically linked Qt Concurrent, Core, D-Bus, GUI,
and Widgets modules from `qt6-qtbase`.

X11 client libraries use permissive MIT/X11-family terms. The GNU C Library,
GNU Compiler Collection runtime, and libstdc++ use their upstream
GPL/LGPL/runtime-exception terms. Fedora's package metadata and installed
license files are authoritative for the exact versions selected by DNF.

## AppImage runtime boundary

The x86-64 AppImage bundles the official Qt 6.8.3 Linux shared libraries,
plugins, and ICU payload selected by the Qt installer. The applicable Qt
GPL/LGPL texts and exception, ICU license, and AppImage runtime license are
downloaded from immutable upstream revisions with verified checksums and
copied below `usr/share/doc/plazmic-legends/third-party` inside the AppImage.

linuxdeploy and linuxdeploy-plugin-qt are MIT-licensed packaging tools. The
type-2 AppImage runtime is MIT licensed. The build script pins and verifies the
downloaded packaging-tool and runtime digests; the packaging tools are not
installed by the RPM.

linuxdeploy copies the non-base shared-library closure from the Ubuntu 22.04
build environment and copies Debian copyright records alongside it. The build
adds explicit records for any dependency for which linuxdeploy cannot resolve
the record through its normal package query. Inspect the exact artifact under
`usr/share/doc` for the authoritative bundled-library notices.

The AppImage deliberately does not bundle the Linux kernel, glibc, graphics
driver, X11 server, or every base graphics/client ABI. Those remain supplied by
the host and are subject to the host distribution's package licenses.
