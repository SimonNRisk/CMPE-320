# CMPE-320

Backend for the CMPE 320 course repository at Queen's University.

## Build locally

From the repository root:

```sh
cmake -S backend -B build
cmake --build build
```

The server requires `SUPABASE_URL` and `SUPABASE_ANON_KEY` and listens on
`PORT` when set, or port `8080` by default.