# Protocol fixed before the library/holdout comparison

Use 8 independent library histories and 8 holdout histories with distinct physics
and source seeds. Each is one incident 500 keV electron, sampled uniformly on the
entry face; never filter zero deposits or translate/clip clouds. Report sample
mean, sample standard deviation and standard error of Edep; report library versus
holdout difference divided by the combined standard error, with a diagnostic
6-standard-error bound (small samples do not establish production precision).

Fresh-process controls compare all initial fields exactly and chemistry counts
at 2 ps for 8 independently seeded source clouds. Before comparison, the criterion
is each aggregate species difference <= 6*sqrt(N_direct+N_import)+0.02*(N_direct+N_import).
This deliberately conservative exploratory bound is not a validated production
accuracy tolerance. Record effect sizes, counts and repeated-direct variability.
Use the same chemistry seed after construction, because BuildTrack draws random
momentum. Fresh IDs and internal ordering may still change trajectories.

Source-only tests use 100000 histories with lambda*T=8, test count mean and
variance at six analytic standard errors, and 100000 exponential waiting times
at six standard errors. Arrival and packet-choice engines are independent from
CLHEP transport/chemistry. Exposure ends at T, with no dose-triggered stopping.
