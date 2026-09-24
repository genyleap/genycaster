# Feed Failure Recovery and Identity Continuity

Status: implemented checkpoint for the production read UI.

## Problem observed on the real UI

The default Home surface can select the network-wide Trending feed before an
identity is open. A public Hypersnap node may remain reachable at `/v1/info`
while a metrics-backed feed endpoint returns an HTTP 5xx response. Treating
that condition as a dead-end leaves the application visually functional but
without a useful path to content.

The provider adapter already maps HTTP 5xx responses to `Unavailable`; this
checkpoint keeps that semantic distinction and improves recovery around it.

## Changes

### Provider diagnostics

Non-2xx Hypersnap responses now retain a bounded, whitespace-normalized excerpt
of the provider response body in the internal error message. User-facing copy
remains provider-neutral; debug diagnostics can now reveal whether the node
reported a disabled feed service, missing metrics indexer, or another server
condition instead of exposing only the numeric HTTP status.

Provider contract coverage asserts that HTTP 503 remains `Unavailable` and the
bounded provider message reaches diagnostics.

### Connection reuse and bounded feed deadlines

The Qt Network transport now reuses one `QNetworkAccessManager` per worker
thread and proxy policy instead of constructing a new manager for every HTTP
request. This preserves Qt thread affinity while allowing DNS/TCP/TLS and HTTP
connection reuse across sequential reads on the same executor. It directly
reduces repeated stalls that remain in the `Connecting` stage.

Following is the latency-sensitive Home path and uses a 5 second request
deadline. Trending, channel discovery, and profile-feed reads use 6 seconds.
The default Trending UI page is six rows while normal profile pages remain ten
rows. Cached local rows remain
visible while refresh is attempted, and the existing circuit breaker still
prevents repeated transient failures from turning into an unbounded retry
loop. The shorter budgets are degradation policy, not a claim that a remote
public node can always be reached.

When a transient timeout/unavailable/transport failure occurs but usable saved
data is already on screen, presentation now treats the condition as a
non-blocking **Saved** state rather than a top-level warning. Retry and detailed
transport diagnostics remain available. If no saved data exists, the same
failure remains a visible Source issue.

### No dead-end Trending failure

When Trending fails without cached rows, the empty state now offers a concrete
recovery path:

- if an identity is open, continue to Following;
- otherwise, open the identity picker and switch to Following after successful
  resolution;
- Retry remains available for the original feed.

This does not silently substitute one feed for another. The user chooses the
recovery action and the active feed remains explicit.

### Reachable network, unavailable capability

OS reachability can remain `Reachable` while a derived provider endpoint stalls
waiting for response headers. A reconnect signal will never fire in that state.
The Qt composition root therefore performs a low-frequency 35-second automatic
retry only while a source is unavailable, the runtime is idle, and the OS still
reports reachable. The capability circuit cooldown remains 30 seconds, so this
recovery probe does not bypass health policy or become a tight retry loop.

### Identity continuity

The last successfully resolved username is stored in `QSettings`. On the next
normal launch, Genycaster restores that identity and opens Following first.
This avoids making a fragile global feed endpoint a mandatory startup
dependency after onboarding.

Test/visual fixture modes do not consume the persisted identity.

### Navigation remains interactive before identity selection

Following/Profile/navigation entries are no longer inert disabled controls.
When no identity is open, selecting an identity-scoped surface opens the
identity picker. This preserves discoverability and removes dead controls from
the production shell.

## Architectural boundaries

- QML still performs no network or storage work.
- Provider response bodies terminate in the Hypersnap adapter and debug error
  metadata; provider-specific semantics are not introduced into Domain or
  Application.
- Persisted identity preference is UI composition state, not protocol data.
- Feed switching is an explicit presentation intent and does not rewrite the
  underlying read contract.

## Verification in the container

Verified:

- architecture guard passes;
- `provider-hypersnap` configures cleanly;
- the modified Hypersnap feed implementation compiles in the provider-enabled
  UI-disabled build;
- the modified provider contract-test translation unit compiles;
- QML files have balanced structural delimiters and remain registered through
  the existing UI target.

Not verified in this container:

- Qt `qmllint`;
- macOS/Clang 22 `ui-debug` build;
- live provider response body from the user's Mac;
- warning-fatal runtime verification of the new automatic source recovery.

The next Mac run should expand **Details** on any remaining Source issue. The
new diagnostic should include the server's bounded response body, which is the
next authoritative clue if Trending remains unavailable.
