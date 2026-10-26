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


## 6. Bookmark preview and priority extension (schema v5)

Account sessions, Profile and Draft work with schema v4. Genycaster's
cross-device **Cast bookmark preview and priority** require the separate,
additive `005_bookmark_previews.sql` migration. It adds nullable display
snapshots and constrained priority metadata to existing private tables;
it never deletes or resets bookmarks. The backend remains compatible with
schema v4: v5-only fields are not queried before the migration exists.

Run under the server's root shell (redirection reads the migration as root;
the `postgres` OS account cannot traverse the restricted repository path):

```sh
sudo -u postgres pg_dump -Fc genycaster_db > /root/genycaster-before-v5.dump
sudo -u postgres psql -v ON_ERROR_STOP=1 -d genycaster_db \
  < /opt/genycaster/repository/services/account-sync/migrations/005_bookmark_previews.sql
sudo -u postgres psql -d genycaster_db \
  -c "SELECT version FROM schema_migrations ORDER BY version;"
sudo -u postgres psql -d genycaster_db \
  -c "SELECT count(*) AS bookmarks FROM bookmarks;"
pm2 startOrReload /opt/genycaster/caster-deployment/account-api.ecosystem.config.cjs --update-env
```

Expected migration includes `ALTER TABLE`, `CREATE INDEX`, `INSERT 0 1`
and `COMMIT`. Verify v5 exists before claiming portable previews and
priority are active. Existing saved Cast hashes remain canonical, and
previous local previews are safely backfilled by authenticated devices.
A missing preview cannot be manufactured from only a Snapchain hash:
`/v1/castById` also needs the author FID. The UI must show an honest
placeholder until a verified read or existing device supplies the preview.

The account-scoped saved-link cache is separate from OpenProof authority:
cached provider labels are never stored as proof or granted management
capability. Only an independently verified OpenProof bearer may update
`last_confirmed_at`; cookie-only imports remain explicitly unverified.
