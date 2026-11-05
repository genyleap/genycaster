#!/usr/bin/env bash
# Reproducible native-server WASM build; optionally deploy a verified package.
# Usage: QT_ROOT_DIR=... QT_HOST_PATH=... bash ci/build_deploy_wasm_server.sh [--web-root /absolute/path]
set -euo pipefail

web_root=""
build_dir="${GENYCASTER_WASM_BUILD_DIR:-build/web}"
while (($#)); do
  case "$1" in
    --web-root) if (($# < 2)); then echo "Missing --web-root value" >&2; exit 2; fi
                web_root="$2"; shift 2 ;;
    --build-dir) if (($# < 2)); then echo "Missing --build-dir value" >&2; exit 2; fi
                 build_dir="$2"; shift 2 ;;
    --help|-h) echo "Usage: bash ci/build_deploy_wasm_server.sh [--build-dir /absolute/path] [--web-root /absolute/path]"; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

cd "$(dirname "${BASH_SOURCE[0]}")/.."
repo_root="$PWD"
# Reuse a reviewed, previously configured Qt 6.12 WASM build tree instead
# of allocating a second full build on small production hosts.
if [[ "$build_dir" != /* ]]; then build_dir="$repo_root/$build_dir"; fi
if [[ -n "${EMSDK:-}" && -r "$EMSDK/emsdk_env.sh" ]]; then
  # Qt's wasm toolchain requires EMSDK/.emscripten, not just an emcc binary.
  source "$EMSDK/emsdk_env.sh" >/dev/null
fi
: "${QT_ROOT_DIR:?Set QT_ROOT_DIR to the matching Qt 6.12 WASM SDK}"
: "${QT_HOST_PATH:?Set QT_HOST_PATH to the matching native Qt host SDK}"
test -r "$QT_ROOT_DIR/lib/cmake/Qt6/Qt6Config.cmake" || {
  echo "Qt WebAssembly SDK is missing: $QT_ROOT_DIR" >&2; exit 1;
}
test -d "$QT_HOST_PATH" || { echo "Qt host tools are missing" >&2; exit 1; }
for exe in cmake ninja python3 emcc sha256sum tar; do
  command -v "$exe" >/dev/null || { echo "Missing build tool: $exe" >&2; exit 1; }
done
emsdk_version="$(emcc --version)"
if [[ "$emsdk_version" != *"5.0.5"* ]]; then
  echo "Expected Emscripten 5.0.5 for the pinned Qt 6.12 WASM SDK" >&2
  exit 1
fi

# Qt 6.12 WASM multimedia is a tech preview; build from verified sources when needed.
if [[ ! -s "$QT_ROOT_DIR/lib/cmake/Qt6ShaderTools/Qt6ShaderToolsConfig.cmake" ||
      ! -s "$QT_ROOT_DIR/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]]; then
  bash ci/build_qt_wasm_multimedia.sh
fi

cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  "-DCMAKE_TOOLCHAIN_FILE=$QT_ROOT_DIR/lib/cmake/Qt6/qt.toolchain.cmake" \
  "-DCMAKE_PREFIX_PATH=$QT_ROOT_DIR" \
  "-DQT_HOST_PATH=$QT_HOST_PATH" \
  -DFARCASTER_BUILD_TESTS=OFF
cmake --build "$build_dir" --target genycaster --parallel 2
cmake --build "$build_dir" --target genycaster_wasm_asset_refresh --parallel 2

version="$(python3 - <<'PY'
from pathlib import Path
from ci.release_gate import project_version
print(project_version(Path.cwd()))
PY
)"
mkdir -p dist
archive="$repo_root/dist/GenyCaster-v${version}-web-wasm.tar.gz"
python3 ci/package_web.py --source "$build_dir/bin" --output "$archive"
python3 ci/verify_platform_package.py web "$archive"
sha256sum "$archive" > "${archive}.sha256"
echo "Validated WASM archive: $archive"

if [[ -z "$web_root" ]]; then
  echo "Build-only mode: no site files changed."
  exit 0
fi
if [[ "$web_root" != /* ]]; then
  echo "--web-root must be an existing absolute path" >&2; exit 2
fi
web_root="$(realpath -e "$web_root")"
if [[ ! -d "$web_root" || "$web_root" == "/" ||
      "$web_root" == "$repo_root" || "$web_root" == "$repo_root/"* ]]; then
  echo "Refusing unsafe web root: $web_root" >&2; exit 2
fi
parent="$(dirname "$web_root")"
stage="$(mktemp -d "$parent/.genycaster-stage.XXXXXXXX")"
backup="$(mktemp -d "$parent/.genycaster-backup.XXXXXXXX")"
updated=()
finished=0

cleanup() {
  local code=$?
  trap - EXIT
  if ((code != 0 && finished == 0)); then
    echo "Deployment failed; restoring overwritten static assets" >&2
    local rel
    for rel in "${updated[@]}"; do
      if [[ -f "$backup/$rel" ]]; then
        cp -p "$backup/$rel" "$web_root/$rel" || true
      else
        rm -f "$web_root/$rel" || true
      fi
    done
  fi
  rm -rf -- "$stage"
  if ((finished == 1)); then
    echo "Previous mutable files backed up at: $backup"
  else
    rm -rf -- "$backup"
  fi
  exit "$code"
}
trap cleanup EXIT

tar -xzf "$archive" -C "$stage"
for item in genycaster.html genycaster.wasm genycaster-assets.json genycaster-assets.js genycaster-sw.js; do
  test -s "$stage/$item" || { echo "Missing deploy asset: $item" >&2; exit 1; }
done

# Preserve all old hashed WASM binaries: running tabs can still request them.
while IFS= read -r -d '' file; do
  rel="${file#"$stage/"}"
  if [[ "$rel" =~ ^(genycaster|farcaster_client)\.[0-9a-f]{20}\.wasm$ ]]; then
    dest="$web_root/$rel"
    if [[ -f "$dest" ]]; then
      cmp -s "$file" "$dest" || {
        echo "Immutable wasm hash collision: $rel" >&2; exit 1;
      }
    else
      temp="$(mktemp "$web_root/.genycaster-immutable.XXXXXXXX")"
      cp "$file" "$temp"
      chmod 0644 "$temp"
      mv -n "$temp" "$dest"
      rm -f "$temp"
    fi
  fi
done < <(find "$stage" -type f -print0)

# Promote dependencies with same-filesystem rename; HTML entrypoint goes LAST.
promote() {
  local rel="$1" src="$stage/$1" dest="$web_root/$1" temp
  mkdir -p "$(dirname "$dest")" "$(dirname "$backup/$rel")"
  if [[ -f "$dest" ]] && cmp -s "$src" "$dest"; then return; fi
  if [[ -f "$dest" ]]; then cp -p "$dest" "$backup/$rel"; fi
  temp="$(mktemp "$(dirname "$dest")/.genycaster-file.XXXXXXXX")"
  cp "$src" "$temp"
  chmod 0644 "$temp"
  mv -f "$temp" "$dest"
  updated+=("$rel")
}
while IFS= read -r -d '' file; do
  rel="${file#"$stage/"}"
  if [[ "$rel" == genycaster.html ||
        "$rel" =~ ^(genycaster|farcaster_client)\.[0-9a-f]{20}\.wasm$ ]]; then continue; fi
  promote "$rel"
done < <(find "$stage" -type f -print0)
promote genycaster.html
finished=1
echo "Published verified WASM assets at: $web_root"

if [[ -n "${GENYCASTER_SITE_URL:-}" ]] && command -v curl >/dev/null; then
  echo "Checking public HTTP entrypoint:"
  curl --fail --silent --show-error --location --head --retry 2 \
    "${GENYCASTER_SITE_URL%/}/genycaster.html"
fi
