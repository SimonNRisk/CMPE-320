# Design Document

What the system is meant to do, the API it exposes, why it is built this way, and what is unfinished. For components, flows, and build steps, see [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md).

Snapshot: `main` at `f5e14751` (2026-10-27), written 2026-11-04.

> **Source of this document:** it was reconstructed from the code and history, not from team design notes. The goals and rationale are inferred, so correct them where the team intended something else.

## Goals

**Inferred goals**

- Let students find a class by code, name, instructor, or term.
- Let students share links to resources for a class.
- Keep submissions hidden until an admin approves them, so the site can't be used to publish spam or unsafe links.
- Keep adding search fields cheap, since this is a small student team.

**Not implemented yet**

- Roles or an admin interface. Accounts exist, but no route outside `/auth` requires one.
- File uploads through the API. The `course-files` bucket suggests they're planned.
- Browsing by faculty. The frontend shell shows faculties, but `classes` has no faculty column.

## API

Only `GET /auth/me` requires a token. JSON errors use the form `{"error": "message"}`.

| Route | Purpose | Success |
| --- | --- | --- |
| `GET /` | Hello message (plain text) | `200` |
| `GET /classes` | List or search classes | `200`, array of `id, code, name, instructor, term` |
| `GET /classes/<id>` | Get one class with all columns | `200`, **array holding one object** |
| `GET /classes/<id>/resources` | List approved links, newest first | `200`, array of `id, title, url, created_at` |
| `POST /classes/<id>/resources` | Submit a link for review | `202`, `{"status":"pending_review"}` |
| `POST /auth/register` | Create an account from `email` and `password` | `201`, `{"status":"created"}`; `409` if the email is taken |
| `POST /auth/login` | Sign in with `email` and `password` | `200`, tokens; `401` if invalid |
| `POST /auth/refresh` | Swap a `refresh_token` for new tokens | `200`, tokens; `401` if invalid, expired, or already used |
| `POST /auth/logout` | Revoke a `refresh_token` | `204` |
| `GET /auth/me` | Get the signed-in user (`Authorization: Bearer <access_token>`) | `200`, `id, email, created_at`; `401` otherwise |

Tokens are returned as `access_token` (HS256 JWT, 15 minutes), `refresh_token` (random, 30 days, single use), `token_type`, and `expires_in`. Passwords must be 12 to 128 bytes.

### Search parameters

| Param | Matches | Max length | Allowed besides letters and digits |
| --- | --- | --- | --- |
| `name` | Contains | 100 | space, `-` |
| `code` | Starts with | 20 | `-` |
| `instructor` | Contains | 100 | space, `-`, `.`, `'` |
| `term` | Contains | 40 | space, `-` |
| `q` | `name`, `code`, or `instructor` contains | 100 | space, `-` |

Matching is case-insensitive. Multiple parameters are ANDed, blank values are ignored, and results are capped at 100 with no pagination.

### Submission rules

The body is `{"title": "...", "url": "..."}`.

- **Body:** at most 10,000 bytes.
- **`title`:** 1 to 120 characters after trimming, with no control characters.
- **`url`:** at most 2,048 characters, `http://` or `https://`, with a host name and only printable ASCII without spaces.

### Errors

| Status | When |
| --- | --- |
| `400` | Invalid search term or submission |
| `404` | Class not found on `GET /classes/<id>` or `POST`; a non-numeric ID also gets `404`, but in plain text from Crow |
| `413` | Submission body too large |
| `502` | Supabase unreachable, class check failed, or insert failed |
| Other | `GET /classes`, `GET /classes/<id>`, and `GET .../resources` relay Supabase's status and body when it returns an error |

### Behaviour clients should expect

- `GET /classes/<id>` returns `[{...}]`, not `{...}`.
- `GET /classes/999/resources` returns `200 []` for a class that doesn't exist; it doesn't return `404`.
- `q=Dr. Smith` returns `400` because `q` doesn't allow `.`, while `instructor=Dr. Smith` works.
- Characters outside ASCII, such as `é`, are rejected in search terms.
- `code` only matches from the start, so `code=320` won't find `CMPE-320`, but `q=320` will. A commit message says any part of the code can be searched, so check which behaviour is intended.

## Key decisions

| Decision | Likely reason | Trade-off |
| --- | --- | --- |
| Server sits between client and Supabase | Validation stays in one place, and the client never needs the Supabase key | Extra code and an extra network hop, and it duplicates some of what PostgREST and RLS offer |
| PostgREST over HTTP instead of a Postgres driver | One small client file and no connection pooling | Queries are built as URL strings, and a new request per call adds latency |
| Table-driven search (`SEARCH_FIELDS`) | A new field takes one line | Only suits simple per-column filters |
| Allowlist of characters per field | Blocks LIKE wildcards and PostgREST operators; `,`, `(`, `)`, `%`, and `_` stay rejected | Rejects some legitimate searches, as shown above. Widening a list needs care |
| Approval through a database default | Clients can't approve their own links, because the server builds the row itself | Depends on a table definition that isn't in the repo, and an admin must work in Supabase |
| Crow on a flat file layout | Simple build, with a small framework | Header-only Crow is sensitive to Asio versions, as the [build notes](SYSTEM_ARCHITECTURE.md#build-and-run) show |

## Security model

- **Identity:** the server verifies users itself with `authenticate()` in `auth.h`. Class and resource routes don't call it yet, and their database requests use the `anon` role, so RLS can't tell users apart. Auth routes use the `service_role` key, which bypasses RLS, so never pass that config to other routes.
- **Credentials:** passwords use Argon2id (64 MiB, 3 passes). Refresh tokens are stored as SHA-256 hashes, and each is deleted when used, so reuse fails even in a race. Login gives the same error and takes about the same time for an unknown email as for a wrong password. Registration does reveal whether an email is taken.
- **Injection:** search terms pass an allowlist and percent-encoding. Numeric IDs come from Crow's `<int>` route. Inserted JSON is built with `crow::json::wvalue`.
- **Unsafe links:** the scheme check blocks `javascript:`, `data:`, and `file:` URLs. It doesn't check whether a link's content is safe; that's left to the admin.
- **Secrets:** the anon key comes from the environment and isn't in source. `resources.cpp:75` has a commented-out check for a `secret_key` that `SupabaseConfig` no longer has.
- **Abuse:** there is no rate limiting, so anyone can fill the review queue or guess passwords. Each login or registration takes about 600 ms of CPU and 64 MiB on a worker thread, so a flood of them could also exhaust the server. The server itself speaks plain HTTP, so it needs a TLS proxy in production.

## Known gaps

Ordered by how much each one blocks progress.

| # | Gap | Impact | Suggested fix |
| --- | --- | --- | --- |
| 1 | No migration creates `class_resources`, and `anon` has no grants on it | On a database built from the migrations, both resource routes should fail. It may exist in the hosted project, which is unconfirmed | Add a migration with a foreign key, `approved boolean not null default false`, grants, and RLS: read approved rows, insert only with `approved = false` |
| 2 | The API sends no CORS headers | A browser client on another origin, such as Vite's dev server, can't read responses. Preflight returns `204` with no CORS headers | Use Crow's CORS middleware or a Vite dev proxy |
| 3 | The frontend isn't merged or connected | There is no working user-facing product | Merge without `node_modules/`, then add API calls |
| 4 | The standard build fails on this Mac | New setups may hit the same [curl and Asio errors](SYSTEM_ARCHITECTURE.md#build-and-run) | Drop `CONFIG` from `find_package(CURL ...)`, and pin or vendor Asio |
| 5 | There are no tests or CI config | Regressions in validation and query building would go unnoticed | Unit test the validators, then add API tests against a local Supabase stub |
| 6 | Accounts don't protect anything yet, and the storage policy expects Supabase Auth users, not `users` rows | Link submission stays anonymous, and the bucket can't be used with these accounts directly | Call `authenticate()` in routes that need it, and put uploads behind the server |
| 7 | Smaller issues | Non-numeric `PORT` crashes at startup; `GET .../resources` has no row limit; relayed Supabase errors can expose internal details; `trim_spaces` is duplicated | Fix alongside nearby work |

## Open questions

1. **Which routes should require an account?** Link submission is the obvious first candidate. Roles would be needed before admins can approve links in the app.
2. **Which remote is canonical?** The root points to GitHub and the nested checkout to the course GitLab. Pick one to avoid drift.
3. **Should `GET /classes/<id>` return an object?** That's the more conventional shape, and it's cheapest to change before a client depends on it.
4. **How should admins approve links?** Using the Supabase dashboard is enough for now. A review screen would need roles, so it depends on question 1.
5. **Links, files, or both?** The API only handles links, while the bucket implies uploads. Files add storage, moderation, and download-permission work, and the bucket has no read policy yet.

## Verification

Checked on 2026-11-04 on macOS:

- **Build:** the unmodified config failed as documented, and a patched copy outside the repo built successfully.
- **API:** the class and resource routes were run against a local stub that logged requests, which confirmed the statuses, validation, generated PostgREST paths, and missing CORS headers listed here.
- **Auth:** 69 end-to-end checks passed against real Postgres 17 and PostgREST v16.5, with Supabase-style roles. They covered the login flows, forged and expired tokens, refresh token reuse and races, direct `anon` and `authenticated` access to the auth tables, startup checks, and the `classes` routes, including the `or=` filter. The test script isn't in the repo.

**Not checked:**

- The `class_resources` routes against a real database, since the table has no migration
- The storage bucket migration
- The Docker image
- A real Supabase project, including its gateway and key formats
