**Wasel 2.0.14-w1** — based on upstream standalone 2.0.14 (build bde584ccbe78cbce sources).

- `wasel-remote-2.0.14.fwr`: end-to-end encrypted control from anywhere through a public MQTT broker
  (ChaCha20-Poly1305 from the chip ROM, self-tested; replay window 5 min; host names via DNS).
  Home Assistant Discovery is not included in this variant.
- `wasel-ha-2.0.14.fwr`: the upstream Home Assistant MQTT Discovery build, built by this repository's CI.

Everything else is upstream 2.0.14. Check the SHA-256 sums in `SHA256SUMS`.
A failed install may need SWD recovery; see README.
