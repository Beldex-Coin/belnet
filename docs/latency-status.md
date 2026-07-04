# Latency fields in the status output

The daemon measures the end-to-end latency of every established path and, as
of this version, attributes and persists those measurements so they can be
inspected and used for hop selection.

## Per-path latency

Each path object in the status JSON (under `services.<name>.paths` /
`Builder::ExtractStatus`) now contains:

- `latencyMs` — the most recent measured end-to-end latency of the path in
  integer milliseconds (`0` if not yet measured). This is the same value as
  `intro.latency`, exposed as a plain integer for easy consumption by the
  mobile app and other RPC clients.

## Per-router latency estimates

The router-level status object contains:

- `routerLatency` — a map of router id -> estimated per-router latency
  contribution in integer milliseconds, for the 20 most-sampled router
  profiles.

The estimate is derived by attributing an equal share of each measured
established-path latency to every hop of the path except the first (the first
hop is a direct link and is measured separately). The accumulated values are
persisted in `profiles.dat` (bencode keys `la` = accumulated milliseconds,
`ln` = sample count); files written by older versions load fine (the fields
default to zero), and older versions ignore the new keys.

No existing JSON keys were renamed or removed.
