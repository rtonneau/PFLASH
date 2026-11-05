---
bump: patch
floor: patch
---
- Fix Serial and MT running different chemistry with `/chem/env/scavenger O2` under IRT_syn: on MT workers, `O2` produced by two tracked molecules stayed tracked instead of joining the bulk pool. `Species.*` now has no `O_2^0` row in either mode, and MT results with O2 set change statistically. SBS and pure water keep the row.
