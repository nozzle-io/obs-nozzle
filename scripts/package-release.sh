#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 4 ]; then
  echo "usage: scripts/package-release.sh PLATFORM PACKAGE_NAME PACKAGE_ROOT BUILD_DIR" >&2
  exit 2
fi

platform="$1"
package_name="$2"
package_root="$3"
build_dir="$4"

case "$platform" in
  macos)
    extension=".so"
    binary_destination="lib/obs-plugins/obs-nozzle/obs-nozzle.so"
    ;;
  *)
    echo "unsupported platform for shell packager: $platform" >&2
    exit 2
    ;;
esac

plugin_path="$(find "$build_dir" -type f -name "obs-nozzle${extension}" | sort | head -n 1)"
if [ -z "$plugin_path" ]; then
  echo "missing plugin binary: obs-nozzle${extension} under $build_dir" >&2
  exit 1
fi

data_path="data/locale/en-US.ini"
if [ ! -f "$data_path" ]; then
  echo "missing data file: $data_path" >&2
  exit 1
fi

rm -rf package "$package_name" package-contents.txt plugin-file.txt
mkdir -p "package/$package_root/$(dirname "$binary_destination")"
mkdir -p "package/$package_root/share/obs-plugins/obs-nozzle/locale"

cp "$plugin_path" "package/$package_root/$binary_destination"
cp "$data_path" "package/$package_root/share/obs-plugins/obs-nozzle/locale/en-US.ini"
cp README.md "package/$package_root/README.md"
cp LICENSE "package/$package_root/LICENSE"

file "package/$package_root/$binary_destination" | tee plugin-file.txt
grep -F "Mach-O" plugin-file.txt
grep -F "bundle" plugin-file.txt

(
  cd package
  zip -r "../$package_name" "$package_root"
)

test -f "$package_name"
unzip -l "$package_name" > package-contents.txt
grep -F "$package_root/$binary_destination" package-contents.txt
grep -F "$package_root/share/obs-plugins/obs-nozzle/locale/en-US.ini" package-contents.txt
grep -F "$package_root/README.md" package-contents.txt
grep -F "$package_root/LICENSE" package-contents.txt
