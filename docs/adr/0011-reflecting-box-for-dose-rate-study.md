# Reflecting, not periodic, chemistry box for the dose-rate study

_ADR 0011, recorded 2026-11-15 (session 2026-11-15__dose-rate-study)._

<!-- Format adapted from mattpocock-skills' domain-modeling (ADR-FORMAT.md): https://github.com/mattpocock/skills.git -->

The dose-rate study was requested with periodic chemistry boundaries "as in the UHDR example", but UHDR's periodic condition applies only to physics-stage electrons; its chemistry reflects molecules at the walls. The study's 4 um chemistry box is reflecting (a mirrored lattice, which also represents an infinite, uniformly irradiated medium), because under IRT_syn the Geant4 neighbour finder has no periodic images: a periodic wrap would silently miss reactions between molecules close across a face, while a minimum-image finder would need yet another kernel patch.
