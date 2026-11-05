-- Trigram matching for fast, case-insensitive partial name searches
create extension if not exists pg_trgm;

create table if not exists classes (
  id bigint generated always as identity primary key,
  code text not null unique,        -- e.g. 'CMPE-320'
  name text not null,               -- e.g. 'Software Engineering I'
  description text,
  instructor text,
  term text,                        -- e.g. 'Fall 2026'
  created_at timestamptz default now()
);

create index if not exists classes_name_trgm_idx
  on classes using gin (name gin_trgm_ops);

-- Let the anon key read classes, but not change them
alter table classes enable row level security;

drop policy if exists "Anyone can read classes" on classes;
create policy "Anyone can read classes"
  on classes for select
  using (true);

-- Sample data (re-running this won't create duplicates)
insert into classes (code, name, description, instructor, term) values
  ('CMPE-320', 'Software Engineering I', 'Intro to software design and testing', 'Dr. Smith', 'Fall 2026'),
  ('CMPE-321', 'Software Engineering II', 'Project-based development', 'Dr. Lee', 'Winter 2027'),
  ('MATH-101', 'Calculus I', 'Limits, derivatives, and integrals', 'Dr. Patel', 'Fall 2026'),
  ('CISC-121', 'Introduction to Programming', 'Python basics', 'Dr. Chen', 'Fall 2026')
on conflict (code) do nothing;