-- Only the SHA-256 of each refresh token is stored; each token is deleted when used
create table if not exists refresh_tokens (
  token_hash text primary key,
  user_id bigint not null references users(id) on delete cascade,
  expires_at timestamptz not null,
  created_at timestamptz not null default now()
);

create index if not exists refresh_tokens_user_id_idx on refresh_tokens (user_id);

alter table refresh_tokens enable row level security;
revoke all on refresh_tokens from anon, authenticated;
grant select, insert, delete on refresh_tokens to service_role;

-- Creates the profile row and the login row in one transaction, so a failed
-- sign-up never leaves a profile without a login. Called by POST /auth/register.
create or replace function register_user(
  p_username text,
  p_email text,
  p_password_hash text,
  p_school text default null,
  p_year smallint default null,
  p_program text default null,
  p_courses text[] default '{}'
) returns bigint
language plpgsql
set search_path = public
as $$
declare
  new_id bigint;
begin
  insert into users (username, school, year, program, courses)
  values (p_username, p_school, p_year, p_program, coalesce(p_courses, '{}'))
  returning id into new_id;

  insert into user_auth (user_id, email, password_hash)
  values (new_id, p_email, p_password_hash);

  return new_id;
end;
$$;

-- Functions are executable by everyone by default; only the server may call this one
revoke execute on function register_user from public, anon, authenticated;
grant execute on function register_user to service_role;

notify pgrst, 'reload schema';
