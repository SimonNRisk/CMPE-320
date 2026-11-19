-- Student profile data
create table if not exists users (
  id bigint generated always as identity primary key,
  username text not null unique,
  -- Optional at sign-up
  school text,
  year smallint check (year between 1 and 5),
  program text,
  courses text[] not null default '{}',   -- course codes, e.g. {'CMPE-320','CISC-121'}
  created_at timestamptz not null default now()
);

-- Login credentials, one row per user, sharing the user's id
create table if not exists user_auth (
  user_id bigint primary key references users(id) on delete cascade,
  email text not null unique check (email = lower(email)),
  -- Argon2id encoded string, e.g. $argon2id$v=19$m=65536,t=3,p=1$<salt>$<hash>
  -- It holds the salt, memory (m), passes (t) and threads (p) along with the hash.
  -- Never store the plain password.
  password_hash text not null check (password_hash like '$argon2id$%')
);

-- Only the server's service-role key may touch these tables.
-- Supabase grants new public tables to anon and authenticated by default, so revoke that.
alter table users enable row level security;
alter table user_auth enable row level security;
revoke all on users, user_auth from anon, authenticated;
grant select, insert, update, delete on users, user_auth to service_role;
