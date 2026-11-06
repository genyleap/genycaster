#!/bin/sh
# Supervised AppImage replacement. Arguments are passed as argv, never interpolated.
set -eu
[ "$#" -eq 4 ] || exit 2
pid=$1
package=$2
target=$3
expected=$4

case "$pid" in *[!0-9]*|"") exit 2 ;; esac
case "$target" in /*.AppImage) ;; *) exit 3 ;; esac
case "$package" in /*.AppImage) ;; *) exit 3 ;; esac
case "$expected" in *[!0-9a-f]*|"") exit 3 ;; esac
[ "${#expected}" -eq 64 ] || exit 3
[ -f "$target" ] && [ ! -L "$target" ] && [ -w "$target" ] || exit 4
[ -f "$package" ] && [ ! -L "$package" ] || exit 4
parent=$(dirname "$target")
[ -w "$parent" ] || exit 4

hashbin=/usr/bin/sha256sum
[ -x "$hashbin" ] || hashbin=/bin/sha256sum
[ -x "$hashbin" ] || exit 5

digest() {
    "$hashbin" "$1" | cut -d ' ' -f 1
}
[ "$(digest "$package")" = "$expected" ] || exit 6

# Wait for the old process to release FUSE mounts and its signer/database handles.
tries=0
while kill -0 "$pid" 2>/dev/null; do
    tries=$((tries + 1))
    [ "$tries" -lt 240 ] || exit 7
    sleep 0.25
done

stage="$parent/.GenyCaster-update-$.AppImage"
backup="$parent/.GenyCaster-backup-$.AppImage"
statusFile="$(dirname "$package")/genycaster-native-update.status"
installed=0
cleanup() {
    [ -f "$stage" ] && rm -f -- "$stage"
    if [ "$installed" -eq 0 ] && [ -f "$backup" ]; then
        [ -f "$target" ] && rm -f -- "$target"
        mv -- "$backup" "$target" || true
    fi
}
finish() {
    result=$?
    trap - EXIT HUP INT TERM
    cleanup
    if [ "$result" -eq 0 ]; then
        printf 'success\n' > "$statusFile"
    else
        printf 'failed:%s\n' "$result" > "$statusFile"
    fi
    exit "$result"
}
trap finish EXIT
trap 'exit 130' HUP INT TERM

cp -- "$package" "$stage"
chmod 755 "$stage"
[ "$(digest "$stage")" = "$expected" ] || exit 8
mv -- "$target" "$backup"
mv -- "$stage" "$target"
[ "$(digest "$target")" = "$expected" ] || exit 9

# Relaunch the original AppImage. If it cannot even start, restore its predecessor.
"$target" >/dev/null 2>&1 &
newpid=$!
sleep 2
if ! kill -0 "$newpid" 2>/dev/null; then
    exit 10
fi
installed=1
rm -f -- "$backup"
exit 0
