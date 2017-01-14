# Presto Web rendering engine: Opera 12.15

This repository contains the Presto rendering engine, used up to Opera 12.

.-Peace out

---

## build on linux

1. apply `fix_flower_build.patch` from root of repository
2. place curl sources to `modules`
3. `./flower -v --without-kde4`

On 64-bit systems, you may need build plugin-wrapper manually.

See `flower` output with `-j1` for additional info

Original patch borrowed from http://paste.fedoraproject.org/526781/32598714/