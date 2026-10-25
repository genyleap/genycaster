-- Database-owner provisioning; run with explicitly authorized DBA access.
-- Never embed production passwords in migrations or repository config.
-- Use a separate least-privilege runtime role and grant only required privileges.
CREATE DATABASE genycaster_db WITH ENCODING 'UTF8' TEMPLATE template0;
