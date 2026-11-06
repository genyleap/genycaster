#!/bin/sh
# Supervised user-writable macOS app update from a verified signed DMG.
# An unsigned or differently signed build is intentionally NOT auto-installed.
set -eu
[ "$#" -eq 4 ] || exit 2
pid=$1
package=$2
target=$3
expected=$4

case "$pid" in *[!0-9]*|"") exit 2 ;; esac
case "$target" in /*/Genycaster.app) ;; *) exit 3 ;; esac
case "$package" in /*.dmg) ;; *) exit 3 ;; esac
case "$expected" in *[!0-9a-f]*|"") exit 3 ;; esac
[ "${#expected}" -eq 64 ] || exit 3
[ -d "$target" ] && [ ! -L "$target" ] || exit 4
[ -f "$package" ] && [ ! -L "$package" ] || exit 4
parent=$(dirname "$target")
[ -w "$parent" ] || exit 4
[ "$(/usr/bin/shasum -a 256 "$package" | /usr/bin/awk '{print $1}')" = "$expected" ] || exit 5

tries=0
while kill -0 "$pid" 2>/dev/null; do
    tries=$((tries + 1))
    [ "$tries" -lt 240 ] || exit 6
    sleep 0.25
done

mount=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/genycaster-mount.XXXXXX")
stage="$parent/.Genycaster-update-$.app"
backup="$parent/.Genycaster-backup-$.app"
statusFile="$(dirname "$package")/genycaster-native-update.status"
mounted=0
installed=0
cleanup() {
    if [ "$installed" -eq 0 ] && [ -d "$backup" ]; then
        [ -d "$target" ] && /bin/rm -rf "$target"
        /bin/mv "$backup" "$target" || true
    fi
    [ -d "$stage" ] && /bin/rm -rf "$stage"
    if [ "$mounted" -eq 1 ]; then
        /usr/bin/hdiutil detach -quiet "$mount" || true
    fi
    /bin/rmdir "$mount" 2>/dev/null || true
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

/usr/bin/hdiutil attach -quiet -readonly -nobrowse -mountpoint "$mount" "$package"
mounted=1
candidate="$mount/Genycaster.app"
[ -d "$candidate" ] && [ ! -L "$candidate" ] || exit 7

# Require a real trusted signing team and the same bundle identifier/team as
# the existing installation. Do not bypass Gatekeeper or remove quarantine.
newid=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$candidate/Contents/Info.plist")
oldid=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$target/Contents/Info.plist")
[ -n "$newid" ] && [ "$newid" = "$oldid" ] || exit 8
/usr/bin/codesign --verify --deep --strict "$candidate" || exit 9
/usr/sbin/spctl --assess --type execute "$candidate" || exit 10
oldteam=$(/usr/bin/codesign -dv --verbose=4 "$target" 2>&1 | /usr/bin/sed -n 's/^TeamIdentifier=//p')
newteam=$(/usr/bin/codesign -dv --verbose=4 "$candidate" 2>&1 | /usr/bin/sed -n 's/^TeamIdentifier=//p')
[ -n "$oldteam" ] && [ "$oldteam" = "$newteam" ] || exit 11

/usr/bin/ditto "$candidate" "$stage"
/usr/bin/codesign --verify --deep --strict "$stage" || exit 12
/bin/mv "$target" "$backup"
/bin/mv "$stage" "$target"
/usr/bin/codesign --verify --deep --strict "$target" || exit 13
/usr/bin/open "$target" || exit 14
installed=1
/bin/rm -rf "$backup"
exit 0
