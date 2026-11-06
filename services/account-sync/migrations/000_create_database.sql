-- Database-owner provisioning; run with explicitly authorized DBA access.
-- Pass the private destination name from operator configuration, not source:
--   psql -v ON_ERROR_STOP=1 -v target_database=YOUR_PRIVATE_NAME \
--     -d postgres -f services/account-sync/migrations/000_create_database.sql
-- Do not store the actual database name or credentials in the repository.
-- Use a separate least-privilege runtime role and grant only needed privileges.
SELECT format('CREATE DATABASE %I WITH ENCODING ''UTF8'' TEMPLATE template0',
              :'target_database')
\gexec
