-- Trigram matching for fast, case-insensitive partial searches
create extension if not exists pg_trgm;

create table if not exists courses (
  course_code       text primary key,              -- e.g. 'ANAT 100'
  subject           text not null,                 -- e.g. 'ANAT'
  number            text not null,                 -- e.g. '100', '215A'
  title             text not null,
  units             numeric(4,2),
  description       text,
  learning_outcomes text,
  learning_hours    numeric,
  requirements      text,
  offering_term     text,
  offering_faculty  text,
  ceab              jsonb not null default '{}'::jsonb,
  extra_details     jsonb not null default '{}'::jsonb,
  source_urls       text[] not null default '{}',
  source_names      text[] not null default '{}',
  created_at        timestamptz not null default now(),
  updated_at        timestamptz not null default now()
);

-- Speeds up name searches like ?title=calc
create index if not exists courses_title_trgm_idx
  on courses using gin (title gin_trgm_ops);

-- Speeds up filtering by subject, like ?subject=ANAT
create index if not exists courses_subject_idx
  on courses (subject);

-- Speeds up searches by code, like ?code=MATH
create index if not exists courses_code_trgm_idx
  on courses using gin (course_code gin_trgm_ops);

-- Read-only access for the anon key
alter table courses enable row level security;

drop policy if exists "Anyone can read courses" on courses;
create policy "Anyone can read courses"
  on courses for select
  using (true);

grant usage on schema public to anon;
grant select on public.courses to anon;