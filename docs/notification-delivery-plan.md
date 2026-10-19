# Native and Web Push Delivery Plan (Qt 6.12)

Status: **Design only — no background push sender, device registration, or
permission prompts have been enabled in production.** The existing in-app
notifications and badge continue to work independently.

## Architectural boundary

The current `NotificationSurface.qml` renders the account-scoped notification
read model exposed by `PresentationBridge`. That model is an inbox, not a
background delivery mechanism. Platform notifications should subscribe to
**the same canonical, account-scoped notification identities**, through
a separate optional delivery capability. Do not generate background notifications
from whatever list a filter happens to display.

```
Farcaster notification source / authorized provider webhook
  -> notification ingester (canonical event ID, recipient FID, kind, timestamp)
  -> account-scoped deduplication and delivery outbox
  -> per-recipient preferences, consent, rate limits, quiet hours
  -> authorized devices / Web Push subscriptions
  -> APNs (native iOS), FCM/NotificationManager (native Android),
     Web Push/VAPID (browser/PWA)
  -> platform notification click -> authenticated Genycaster deep link
```

**Never** expose raw provider auth tokens, signer keys, message credentials
or browser PushSubscriptions to other users. Device tokens/subscriptions are
encrypted server-side, rotate and expire, and are removed on logout/account
switch/unsubscribe. Every send checks the authenticated recipient's account
scope; the same event delivered across devices uses one stable event ID and
a per-endpoint delivery record. Handle expiration (404/410), retries and backoff.

## Platform matrix

| Target | Foreground notification | Background / app closed | Permission |
| --- | --- | --- | --- |
| Web desktop (Chrome/Firefox/Edge/Safari as supported) | Existing QML inbox; optionally `ServiceWorkerRegistration.showNotification` | Web Push -> Service Worker `push` event -> `showNotification` | Browser Notifications permission plus PushManager subscription, initiated by a user gesture |
| iPhone/iPad **installed Home Screen web app**, iOS/iPadOS 16.4+ | Same inbox | Standards-based Web Push to installed PWA through service worker (uses APNs internally) | The user must add the website to the Home Screen first and opt in by tapping an explicit action; an ordinary Safari tab is not a substitute |
| Qt 6.12 desktop (Windows/Linux/macOS) | In-app inbox; optional tray notification `QSystemTrayIcon::showMessage` with availability check | OS-specific native notification implementation for reliable clicks/actions and persistent delivery | OS-dependent |
| Qt 6.12 native iOS | In-app inbox plus `UNUserNotificationCenter` for local alerts | APNs device token, entitlement, authenticated backend sender, deep-link handler | Explicit OS authorization; native signed app and APNs provisioning |
| Qt 6.12 native Android | In-app inbox plus `NotificationManager` through Qt JNI bridge | FCM token and messaging receiver/service; OS notification channel | `POST_NOTIFICATIONS` runtime permission on Android 13+ |

`QSystemTrayIcon` is a desktop convenience and **does not** replace APNs,
Android NotificationManager or background push. Prefer platform-native modules
isolated behind the shared notification delivery port.

## Web implementation steps

1. Keep `ui/wasm/genycaster-sw.js`'s **public-static-assets-only** cache
   allowlist. Add separate `push` and `notificationclick` listeners without
   caching user responses. Use `event.waitUntil()`; permit only same-origin,
   validated links and focus an existing app window when possible.
2. A separate opt-in button in account notification settings checks
   `Notification`, `PushManager`, and `navigator.serviceWorker.ready`;
   on iOS verify standalone/Home Screen installation. Request permission only
   in its click handler, never during startup.
3. Subscribe with `pushManager.subscribe({userVisibleOnly:true,
   applicationServerKey: <public VAPID key>})`; POST the subscription to an
   authenticated CSRF-protected account/device endpoint. Keep private VAPID
   keys only on the server.
4. Deliver pushes from the outbox and send a minimal title/body/tag plus a
   validated event link. On click, open/focus the app; hydrate the canonical
   notification from normal authenticated reads. Add subscription rotation and
   idempotent revocation at logout.
5. Test normal and denied permission, user-visible-only behavior, offline,
   account switch, unsubscribes, duplicate delivery, expired endpoints, and
   iOS PWA installed versus Safari-tab cases.

## Native implementation steps

1. Provide an optional provider-neutral `NotificationDeliveryPort` with
   `requestPermission`, `registerEndpoint`, `unregisterEndpoint`, and
   `openNotification` operations. Keep UI-thread callbacks queued.
2. iOS: a Qt Objective-C++ bridge for `UNUserNotificationCenter`, APNs token
   registration, notification response callbacks, entitlement/cert setup.
3. Android: Qt JNI bridge for notification channels, permission, FCM token
   refresh and native tap/deep-link intent. Avoid polling while backgrounded.
4. Desktop: native OS adapter; QSystemTrayIcon may be a limited fallback
   only when `supportsMessages()` returns true.
5. Reuse the same backend outbox and deduplication across all platforms.
   Do not mark in-app notifications read merely because a system toast was
   delivered or tapped without opening the target.

## Suggested rollout

Phase 1: fix/verify in-app notification layout and implement a consent/settings
surface with no automatic permission. Phase 2: authenticated Web Push backend
and Service Worker, tested on installed iOS PWA. Phase 3: native iOS/Android
bridges, APNs/FCM credentials and device management. Phase 4: optional desktop
delivery, quiet hours and full observability.

Relevant platform documentation:

- https://webkit.org/blog/13878/
- https://developer.apple.com/documentation/usernotifications/sending-web-push-notifications-in-web-apps-and-browsers
- https://developer.mozilla.org/en-US/docs/Web/API/ServiceWorkerRegistration/showNotification
- https://doc.qt.io/qt-6/qsystemtrayicon.html
