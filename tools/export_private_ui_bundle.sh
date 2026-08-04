#!/bin/sh

set -eu

script_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH='' cd -- "$script_dir/.." && pwd)
game_dir=${EQ_LEGENDS_DIR:-}
resolution=
destination=
refresh=0
staging_dir=
backup_dir=
checksums_staging=
archive_staging=
archive_path=
backup_archive=
previous_directory_moved=0
previous_archive_moved=0
published_directory=0
published_archive=0

usage() {
  cat <<'EOF'
Usage: tools/export_private_ui_bundle.sh [OPTIONS]

Options:
  --game-dir DIR      EverQuest Legends installation directory
  --resolution WxH    Layout resolution, for example 2560x1440
  --destination DIR   Private bundle directory
  --refresh           Preserve an existing bundle as a timestamped backup
  -h, --help          Show this help
EOF
}

cleanup() {
  status=$?
  trap - EXIT HUP INT TERM

  if [ -n "$staging_dir" ] && [ -d "$staging_dir" ]; then
    rm -rf -- "$staging_dir"
  fi
  if [ -n "$checksums_staging" ] && [ -f "$checksums_staging" ]; then
    rm -f -- "$checksums_staging"
  fi
  if [ -n "$archive_staging" ] && [ -f "$archive_staging" ]; then
    rm -f -- "$archive_staging"
  fi

  if [ "$status" -ne 0 ]; then
    if [ "$published_archive" -eq 1 ] && [ -f "$archive_path" ]; then
      rm -f -- "$archive_path"
    fi
    if [ "$published_directory" -eq 1 ] && [ -d "$destination" ]; then
      rm -rf -- "$destination"
    fi
    if [ "$previous_directory_moved" -eq 1 ] && [ -d "$backup_dir" ]; then
      mv -- "$backup_dir" "$destination"
    fi
    if [ "$previous_archive_moved" -eq 1 ] && [ -f "$backup_archive" ]; then
      mv -- "$backup_archive" "$archive_path"
    fi
  fi

  exit "$status"
}

trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

while [ "$#" -gt 0 ]; do
  case "$1" in
    --game-dir)
      [ "$#" -ge 2 ] || {
        printf '%s\n' '--game-dir requires a value.' >&2
        exit 2
      }
      game_dir=$2
      shift 2
      ;;
    --resolution)
      [ "$#" -ge 2 ] || {
        printf '%s\n' '--resolution requires a value.' >&2
        exit 2
      }
      resolution=$2
      shift 2
      ;;
    --destination)
      [ "$#" -ge 2 ] || {
        printf '%s\n' '--destination requires a value.' >&2
        exit 2
      }
      destination=$2
      shift 2
      ;;
    --refresh)
      refresh=1
      shift
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown option: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ -z "$game_dir" ]; then
  printf '%s\n' 'Set EQ_LEGENDS_DIR or pass --game-dir.' >&2
  exit 1
fi
case "$resolution" in
  *[!0-9x]* | '' | x* | *x | *x*x*)
    printf 'Invalid resolution: %s\n' "$resolution" >&2
    exit 1
    ;;
esac
if [ -z "$destination" ]; then
  destination="$repo_dir/private-bundles/plazmic-ui-$resolution"
fi

skin_source="$game_dir/uifiles/plazmic-ui"
if [ ! -f "$game_dir/eqgame.exe" ] || [ ! -f "$skin_source/EQUI.xml" ]; then
  printf 'Installed Plazmic UI was not found in: %s\n' "$game_dir" >&2
  exit 1
fi
if [ ! -f "$game_dir/eqclient.ini" ]; then
  printf 'eqclient.ini was not found in: %s\n' "$game_dir" >&2
  exit 1
fi

for required_command in cp find grep python3 realpath sha256sum sort tar tr; do
  if ! command -v "$required_command" >/dev/null 2>&1; then
    printf 'Required command is not installed: %s\n' "$required_command" >&2
    exit 1
  fi
done

game_canonical=$(realpath -e -- "$game_dir")
destination_canonical=$(realpath -m -- "$destination")
case "$destination_canonical" in
  "$game_canonical" | "$game_canonical"/*)
    printf '%s\n' 'Private bundle destination must be outside the game directory.' >&2
    exit 1
    ;;
esac

destination_parent=$(dirname -- "$destination")
archive_path="${destination}.tar.gz"
umask 077
mkdir -p -- "$destination_parent"

if [ -e "$destination" ] || [ -e "$archive_path" ]; then
  if [ "$refresh" -ne 1 ]; then
    printf 'Destination already exists: %s\n' "$destination" >&2
    printf '%s\n' 'Use --refresh to preserve it as a timestamped backup.' >&2
    exit 1
  fi
  backup_dir="${destination}.backup-$(date +%Y%m%d-%H%M%S)"
  backup_archive="${backup_dir}.tar.gz"
  if [ -e "$backup_dir" ] || [ -e "$backup_archive" ]; then
    printf 'Backup path already exists: %s\n' "$backup_dir" >&2
    exit 1
  fi
  if [ -d "$destination" ]; then
    mv -- "$destination" "$backup_dir"
    previous_directory_moved=1
  fi
  if [ -f "$archive_path" ]; then
    mv -- "$archive_path" "$backup_archive"
    previous_archive_moved=1
  fi
  printf 'Preserved previous bundle: %s\n' "$backup_dir"
fi

staging_dir=$(mktemp -d "${destination}.staging.XXXXXX")
mkdir -p \
  "$staging_dir/uifiles/plazmic-ui" \
  "$staging_dir/ini/layouts" \
  "$staging_dir/ini/characters"

cp -R -- "$skin_source/." "$staging_dir/uifiles/plazmic-ui/"
find "$staging_dir/uifiles/plazmic-ui" \
  -type f \( -name '*.crc' -o -name '*.bak' \) -delete
cp -- "$game_dir/eqclient.ini" "$staging_dir/ini/eqclient.ini"

layout_count=0
newest_layout=
for source_path in "$game_dir"/UI_*.ini; do
  [ -f "$source_path" ] || continue
  cp -- "$source_path" "$staging_dir/ini/layouts/"
  layout_count=$((layout_count + 1))
  if [ -z "$newest_layout" ] || [ "$source_path" -nt "$newest_layout" ]; then
    newest_layout=$source_path
  fi
done
if [ "$layout_count" -eq 0 ]; then
  printf '%s\n' 'No UI_*.ini layout files were found.' >&2
  exit 1
fi
cohesive_layout="$staging_dir/ini/layouts/UI_plazmic_1440p.ini"
if [ ! -e "$cohesive_layout" ]; then
  layout_count=$((layout_count + 1))
fi
python3 "$script_dir/create_cohesive_ui_layout.py" \
  "$newest_layout" "$cohesive_layout"

character_count=0
for source_path in "$game_dir"/*_*.ini; do
  [ -f "$source_path" ] || continue
  case "$(basename -- "$source_path")" in
    UI_*) continue ;;
  esac
  if ! tr -d '\r' <"$source_path" |
    grep -Eq '^\[(HotButtons|ADDITIONALFILTERS)\]$'; then
    continue
  fi
  cp -- "$source_path" "$staging_dir/ini/characters/"
  character_count=$((character_count + 1))
done
if [ "$character_count" -eq 0 ]; then
  printf '%s\n' 'No character INIs with UI/filter state were found.' >&2
  exit 1
fi

cat >"$staging_dir/bundle.ini" <<EOF
[plazmic_ui_bundle]
format=1
resolution=$resolution
skin=plazmic-ui
EOF

checksums_staging=$(mktemp "$destination_parent/.plazmic-checksums.XXXXXX")
(
  cd -- "$staging_dir"
  find . -type f -print0 |
    sort -z |
    xargs -0 sha256sum
) >"$checksums_staging"
mv -- "$checksums_staging" "$staging_dir/SHA256SUMS"
checksums_staging=

chmod -R go-rwx "$staging_dir"
mv -- "$staging_dir" "$destination"
staging_dir=
published_directory=1

archive_staging="${archive_path}.staging"
rm -f -- "$archive_staging"
tar -czf "$archive_staging" -C "$destination_parent" "$(basename -- "$destination")"
chmod 0600 "$archive_staging"
mv -- "$archive_staging" "$archive_path"
archive_staging=
published_archive=1

printf 'Private bundle: %s\n' "$destination"
printf 'Portable archive: %s\n' "$archive_path"
printf 'Resolution: %s\n' "$resolution"
printf 'Layouts: %s\n' "$layout_count"
printf 'Character profiles: %s\n' "$character_count"
if [ -n "$backup_dir" ]; then
  printf 'Previous bundle backup: %s\n' "$backup_dir"
fi
