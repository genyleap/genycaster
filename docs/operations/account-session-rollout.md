# Genycaster account-session rollout (database is canonical)

**Do not restart the account API or publish a new WASM until all gates pass.**
This runbook is for an operator with PostgreSQL DBA and Nginx configuration
permissions. The Remote Operations connector currently runs in restricted mode
and cannot make these privileged changes.

## 1. Before changing production

- Verify the active Genycaster release and keep its immutable directory.
- Confirm the PostgreSQL database is `genycaster_db`, not OpenProof's database.
- Verify service health at `https://caster.genyleap.com/api/v1/cloud/app_updates`.
- Record a normal database backup using the operator's established backup flow.

## 2. Apply the additive migration as database administrator

From the Genycaster repository on the server:

```sh
sudo -u postgres psql -v ON_ERROR_STOP=1 -d genycaster_db \
  -f /opt/genycaster/repository/services/account-sync/migrations/004_account_sessions.sql
sudo -u postgres psql -v ON_ERROR_STOP=1 -d genycaster_db \
  -c "SELECT version FROM schema_migrations WHERE version = 4;"
sudo -u postgres psql -v ON_ERROR_STOP=1 -d genycaster_db \
  -c "SELECT has_table_privilege('genycaster_api', 'account_sessions', 'SELECT,INSERT,DELETE');"
```

Expect schema version 4 and `has_table_privilege = t`. The migration creates
only the independent `account_sessions` table and its expiry index. The API role
gets only SELECT, INSERT and DELETE there; never grant schema ownership.

## 3. Match the API's draft-body limit at Nginx

In the **existing** `location ^~ /api/v1/cloud/` block of
`/etc/nginx/sites-available/caster.genyleap.com`, replace its current
`client_max_body_size 24k;` with `client_max_body_size 208k;`.
Do not alter other locations or global limits.

```sh
sudo nginx -t
sudo systemctl reload nginx
```

## 4. Activate the backend, then the client

Use the existing PM2 deployment configuration:

```sh
pm2 startOrReload /opt/genycaster/caster-deployment/account-api.ecosystem.config.cjs --update-env
```

Before publishing WASM, check that:

- `GET /api/v1/cloud/app_updates` responds 200 with JSON.
- Unauthenticated `GET /api/v1/cloud/session` responds 401.
- Unauthenticated draft/profile reads respond 401.
- A legitimate signed-in device can create a session with a verified
  OpenProof/Farcaster bearer, and `GET /session` with its host-only cookie
  returns the same verified FID. Do not log real session cookies or bearer
  tokens in health-check output.
- A cookie-backed PUT requires the CSRF value; mismatched owner remains isolated.
- Explicit sign-out revokes the session.

Only then run the existing validated WASM build, release-staging and
atomic deployment pipeline. Keep the previous immutable public release
for rollback.

## 5. Verify the intended user experience

With the **same verified Farcaster FID** on two signed-in browsers:

1. Set a category, profession or link on one device; open the profile on
   the second device and verify it reads the database value.
2. Write a non-published Cast draft such as `Hello, Qt!` on the first;
   open the Composer on the second and verify the saved draft appears.
3. Publish or clear that draft, then verify the deletion is reflected on the
   second device without resurrecting the old version.
4. Log out of one device: its account session must cease authorizing reads,
   while the other device remains signed in.
5. Repeat after reloading the browser on both devices; the signed-in experience
   must not require pressing a manual Sync button.

Do not consider the feature complete until the live tests pass. No Farcaster
private keys, wallet custody phrases or OpenProof tokens belong in
Genycaster account-session storage.
