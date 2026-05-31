#!/usr/bin/env bash
set -euo pipefail

sanitize=0
allow_build_rpath=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --sanitize)
      sanitize=1
      shift
      ;;
    --allow-build-rpath)
      allow_build_rpath=1
      shift
      ;;
    *)
      break
      ;;
  esac
done

if [ "$#" -ne 1 ]; then
  echo "usage: scripts/verify-macos-plugin.sh [--sanitize] [--allow-build-rpath] PLUGIN_BINARY" >&2
  exit 2
fi

plugin_path="$1"

if [ ! -f "$plugin_path" ]; then
  echo "missing macOS plugin binary: $plugin_path" >&2
  exit 1
fi

file "$plugin_path" | tee plugin-file.txt
grep -F "Mach-O" plugin-file.txt
grep -F "bundle" plugin-file.txt

lipo -info "$plugin_path" | tee plugin-lipo.txt
grep -F "arm64" plugin-lipo.txt
grep -F "x86_64" plugin-lipo.txt

otool -L "$plugin_path" | tee plugin-otool-l.txt
grep -F "@rpath/libobs.framework/Versions/A/libobs" plugin-otool-l.txt

echo "Verifying LC_RPATH entries via otool -l"
otool -l "$plugin_path" > plugin-load-commands.txt
awk '
  $1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
  want_path && $1 == "path" { print $2; want_path = 0 }
' plugin-load-commands.txt | sort -u > plugin-rpaths.txt

if [ "$sanitize" -eq 1 ]; then
  while IFS= read -r rpath; do
    case "$rpath" in
      /*|*obs-universal*|*runner/work*|*RUNNER_TEMP*)
        install_name_tool -delete_rpath "$rpath" "$plugin_path"
        ;;
    esac
  done < plugin-rpaths.txt

  otool -l "$plugin_path" > plugin-load-commands.txt
  awk '
    $1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
    want_path && $1 == "path" { print $2; want_path = 0 }
  ' plugin-load-commands.txt | sort -u > plugin-rpaths.txt
fi

echo "LC_RPATH entries:"
if [ -s plugin-rpaths.txt ]; then
  cat plugin-rpaths.txt
else
  echo "<none>"
fi

if grep -E '(^/|obs-universal|runner/work|RUNNER_TEMP)' plugin-rpaths.txt; then
  if [ "$allow_build_rpath" -eq 1 ]; then
    echo "macOS build-tree plugin has LC_RPATH entries that are not allowed in release packages"
  else
    echo "macOS plugin contains release-invalid LC_RPATH entries" >&2
    exit 1
  fi
fi
