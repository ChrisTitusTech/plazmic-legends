#!/usr/bin/env bash

set -Eeuo pipefail
shopt -s inherit_errexit

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly SCRIPT_DIR
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd -P)"
readonly REPO_ROOT
readonly VERSION="0.1.0"
readonly QT_VERSION="6.8.3"
readonly UBUNTU_IMAGE="docker.io/library/ubuntu:22.04"
readonly LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
readonly LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
readonly QT_PLUGIN_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
readonly QT_PLUGIN_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
readonly APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
readonly APPIMAGETOOL_SHA256="a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0"
readonly APPIMAGE_RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64"
readonly APPIMAGE_RUNTIME_SHA256="1cc49bcf1e2ccd593c379adb17c9f85a36d619088296504de95b1d06215aebbf"
readonly QT_LICENSE_BASE_URL="https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES"
readonly ICU_LICENSE_URL="https://raw.githubusercontent.com/unicode-org/icu/release-73-2/icu4c/LICENSE"
readonly ICU_LICENSE_SHA256="f3005e195ff74d8812cc1f182a1c446fab678d70a10e3dada497585befee5416"
readonly APPIMAGE_RUNTIME_LICENSE_URL="https://raw.githubusercontent.com/AppImage/type2-runtime/75849dce7cc37e4319b633df1f116ca895c71a12/LICENSE"
readonly APPIMAGE_RUNTIME_LICENSE_SHA256="aa154fc9070614bbe7921f89db11efd1dba7a1f3a41685958110e2230f9c0ca1"

usage() {
	printf 'Usage: %s [output-directory]\n' "${0##*/}"
}

case "${1:-}" in
-h | --help)
	usage
	exit 0
	;;
esac

if (($# > 1)); then
	usage >&2
	exit 2
fi

if command -v podman >/dev/null 2>&1; then
	readonly CONTAINER_ENGINE="podman"
elif command -v docker >/dev/null 2>&1; then
	readonly CONTAINER_ENGINE="docker"
else
	printf '%s\n' 'podman or docker is required to build the AppImage' >&2
	exit 1
fi

OUTPUT_DIR_INPUT="${1:-"$REPO_ROOT/build/artifacts"}"
mkdir -p -- "$OUTPUT_DIR_INPUT"
OUTPUT_DIR="$(cd -- "$OUTPUT_DIR_INPUT" && pwd -P)"
readonly OUTPUT_DIR
WORK_DIR="$(mktemp -d -t plazmic-appimage.XXXXXXXX)"
readonly WORK_DIR
trap 'rm -rf -- "$WORK_DIR"' EXIT

SOURCE_DATE_EPOCH="$(
	git -C "$REPO_ROOT" log -1 --format=%ct
)"
readonly SOURCE_DATE_EPOCH

container_args=(
	run
	--rm
	--env "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"
	--env "PLAZMIC_VERSION=$VERSION"
	--env "QT_VERSION=$QT_VERSION"
	--env "LINUXDEPLOY_URL=$LINUXDEPLOY_URL"
	--env "LINUXDEPLOY_SHA256=$LINUXDEPLOY_SHA256"
	--env "QT_PLUGIN_URL=$QT_PLUGIN_URL"
	--env "QT_PLUGIN_SHA256=$QT_PLUGIN_SHA256"
	--env "APPIMAGETOOL_URL=$APPIMAGETOOL_URL"
	--env "APPIMAGETOOL_SHA256=$APPIMAGETOOL_SHA256"
	--env "APPIMAGE_RUNTIME_URL=$APPIMAGE_RUNTIME_URL"
	--env "APPIMAGE_RUNTIME_SHA256=$APPIMAGE_RUNTIME_SHA256"
	--env "QT_LICENSE_BASE_URL=$QT_LICENSE_BASE_URL"
	--env "ICU_LICENSE_URL=$ICU_LICENSE_URL"
	--env "ICU_LICENSE_SHA256=$ICU_LICENSE_SHA256"
	--env "APPIMAGE_RUNTIME_LICENSE_URL=$APPIMAGE_RUNTIME_LICENSE_URL"
	--env "APPIMAGE_RUNTIME_LICENSE_SHA256=$APPIMAGE_RUNTIME_LICENSE_SHA256"
	--volume "$REPO_ROOT:/src:ro"
	--volume "$WORK_DIR:/work"
	--volume "$OUTPUT_DIR:/output"
	--workdir /work
)

# The single-quoted program expands only inside the container.
# shellcheck disable=SC2016
"$CONTAINER_ENGINE" "${container_args[@]}" "$UBUNTU_IMAGE" bash -Eeuo pipefail -c '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        appstream \
		curl \
		file \
		libdbus-1-3 \
		libegl-dev \
		libfontconfig1-dev \
		libfreetype-dev \
		libgl-dev \
		libglib2.0-0 \
		libx11-dev \
        libxcb-cursor0 \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-keysyms1 \
        libxcb-render-util0 \
        libxcb-shape0 \
        libxcb-xinerama0 \
        libxcb-xkb1 \
        libxkbcommon-x11-0 \
        patchelf \
        python3-pip \
        squashfs-tools

    python3 -m pip install --no-cache-dir \
        "aqtinstall==3.3.0" \
        "cmake==3.31.6" \
        "ninja==1.11.1.3"
    python3 -m aqt install-qt \
        linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir /opt/Qt

    qt_root="/opt/Qt/$QT_VERSION/gcc_64"
    cmake \
        -S /src \
        -B /work/build \
        -G Ninja \
        -DBUILD_TESTING=OFF \
        -DPLAZMIC_ENABLE_REPOSITORY_CHECKS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_PREFIX_PATH="$qt_root"
    cmake --build /work/build --parallel

    appdir="/work/PlazmicLegends.AppDir"
    DESTDIR="$appdir" cmake --install /work/build
    rm "$appdir/usr/share/metainfo/io.github.ChristitusTech.PlazmicLegends.metainfo.xml"
    install -m 0644 \
        /src/packaging/io.github.ChristitusTech.PlazmicLegends.appdata.xml \
        "$appdir/usr/share/metainfo/io.github.ChristitusTech.PlazmicLegends.appdata.xml"

    notices="$appdir/usr/share/doc/plazmic-legends/third-party"
    mkdir -p "$notices/qt" "$notices/icu" "$notices/appimage-runtime"

    download_checked() {
        local url="$1"
        local expected_sha256="$2"
        local destination="$3"
        curl --fail --location --silent --show-error \
            --output "$destination" "$url"
        printf "%s  %s\n" "$expected_sha256" "$destination" \
            | sha256sum --check
    }

    download_checked \
        "$QT_LICENSE_BASE_URL/LGPL-3.0-only.txt" \
        da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768 \
        "$notices/qt/LGPL-3.0-only.txt"
    download_checked \
        "$QT_LICENSE_BASE_URL/GPL-2.0-only.txt" \
        8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643 \
        "$notices/qt/GPL-2.0-only.txt"
    download_checked \
        "$QT_LICENSE_BASE_URL/GPL-3.0-only.txt" \
        8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903 \
        "$notices/qt/GPL-3.0-only.txt"
    download_checked \
        "$QT_LICENSE_BASE_URL/Qt-GPL-exception-1.0.txt" \
        40678d338ce53cd93f8b22b281a2ecbcaa3ee65ce60b25ffb0c462b0530846b2 \
        "$notices/qt/Qt-GPL-exception-1.0.txt"
    download_checked \
        "$ICU_LICENSE_URL" \
        "$ICU_LICENSE_SHA256" \
        "$notices/icu/LICENSE"
    download_checked \
        "$APPIMAGE_RUNTIME_LICENSE_URL" \
        "$APPIMAGE_RUNTIME_LICENSE_SHA256" \
        "$notices/appimage-runtime/LICENSE"

    for package in libcap2 libdbus-1-3 liblzma5 libpcre3; do
        copyright="/usr/share/doc/$package/copyright"
        if [[ ! -f "$copyright" ]]; then
            printf "required copyright file is unavailable: %s\n" \
                "$copyright" >&2
            exit 1
        fi
        install -Dpm0644 \
            "$copyright" \
            "$appdir/usr/share/doc/$package/copyright"
    done

    tools_dir="/work/tools"
    mkdir -p "$tools_dir"
    curl --fail --location --silent --show-error \
        --output "$tools_dir/linuxdeploy-x86_64.AppImage" \
        "$LINUXDEPLOY_URL"
    curl --fail --location --silent --show-error \
        --output "$tools_dir/linuxdeploy-plugin-qt" \
        "$QT_PLUGIN_URL"
    curl --fail --location --silent --show-error \
        --output "$tools_dir/appimagetool-x86_64.AppImage" \
        "$APPIMAGETOOL_URL"
    curl --fail --location --silent --show-error \
        --output "$tools_dir/runtime-x86_64" \
        "$APPIMAGE_RUNTIME_URL"
    printf "%s  %s\n" \
        "$LINUXDEPLOY_SHA256" \
        "$tools_dir/linuxdeploy-x86_64.AppImage" \
        | sha256sum --check
    printf "%s  %s\n" \
        "$QT_PLUGIN_SHA256" \
        "$tools_dir/linuxdeploy-plugin-qt" \
        | sha256sum --check
    printf "%s  %s\n" \
        "$APPIMAGETOOL_SHA256" \
        "$tools_dir/appimagetool-x86_64.AppImage" \
        | sha256sum --check
    printf "%s  %s\n" \
        "$APPIMAGE_RUNTIME_SHA256" \
        "$tools_dir/runtime-x86_64" \
        | sha256sum --check
    chmod 0755 \
        "$tools_dir/appimagetool-x86_64.AppImage" \
        "$tools_dir/linuxdeploy-x86_64.AppImage" \
        "$tools_dir/linuxdeploy-plugin-qt" \
        "$tools_dir/runtime-x86_64"

    export APPIMAGE_EXTRACT_AND_RUN=1
    export ARCH=x86_64
    export LD_LIBRARY_PATH="$qt_root/lib"
    export PATH="$tools_dir:$qt_root/bin:$PATH"
    export QMAKE="$qt_root/bin/qmake"

    "$tools_dir/linuxdeploy-x86_64.AppImage" \
        --appimage-extract-and-run \
        --appdir "$appdir" \
        --desktop-file \
        "$appdir/usr/share/applications/plazmic-legends.desktop" \
        --icon-file \
        "$appdir/usr/share/icons/hicolor/512x512/apps/plazmic-legends.png" \
        --plugin qt

    export VERSION="$PLAZMIC_VERSION"
    "$tools_dir/appimagetool-x86_64.AppImage" \
        --appimage-extract-and-run \
        --runtime-file "$tools_dir/runtime-x86_64" \
        "$appdir"

    mapfile -t artifacts < <(
        find /work -maxdepth 1 -type f -name "*.AppImage" -print
    )
    if ((${#artifacts[@]} != 1)); then
        printf "expected one AppImage, found %d\n" "${#artifacts[@]}" >&2
        exit 1
    fi

    artifact="/output/Plazmic-Legends-$PLAZMIC_VERSION-x86_64.AppImage"
    install -m 0755 "${artifacts[0]}" "$artifact"
    (
        cd /output
        sha256sum "${artifact##*/}" > "${artifact##*/}.sha256"
    )
'

printf 'AppImage: %s\n' \
	"$OUTPUT_DIR/Plazmic-Legends-$VERSION-x86_64.AppImage"
printf 'Checksum: %s\n' \
	"$OUTPUT_DIR/Plazmic-Legends-$VERSION-x86_64.AppImage.sha256"
