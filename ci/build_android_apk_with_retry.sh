#!/usr/bin/env bash
# Retry only verified transient Maven/Gradle network throttling, NEVER
# compilation, toolchain, APK-signing, ELF-alignment, or packaging failures.
set -euo pipefail

: "${RUNNER_TEMP:?Only on an isolated GitHub Actions Android runner}"
for attempt in 1 2 3; do
  log="$RUNNER_TEMP/genycaster-android-package-attempt-${attempt}.log"
  set +e
  cmake --build build/android --target genycaster_make_apk --parallel 3 2>&1 | tee "$log"
  statuses=("${PIPESTATUS[@]}")
  set -e
  if [[ "${statuses[1]}" -ne 0 ]]; then
    echo "::error::Unable to capture Android build diagnostics"
    exit "${statuses[1]}"
  fi
  if [[ "${statuses[0]}" -eq 0 ]]; then
    echo "Android package build and Gradle dependencies succeeded on attempt $attempt"
    exit 0
  fi
  if ! grep -Eqi 'Received status code (429|502|503|504)|Too Many Requests|HTTP (429|502|503|504)' "$log"; then
    echo "::error::Non-network Android failure; refusing retry"
    exit "${statuses[0]}"
  fi
  if [[ "$attempt" -eq 3 ]]; then
    echo "::error::Android Gradle repository throttling persisted after bounded retries"
    exit "${statuses[0]}"
  fi
  echo "::warning::Temporary Maven/Gradle HTTP throttling; retrying the incremental Android build"
  sleep "$((12 * attempt))"
done
exit 1
