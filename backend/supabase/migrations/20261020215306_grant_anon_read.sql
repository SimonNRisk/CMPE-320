-- Let the anon role read the classes table (RLS policy still controls rows)
grant usage on schema public to anon;
grant select on public.classes to anon;