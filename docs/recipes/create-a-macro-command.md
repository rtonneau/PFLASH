# Create a macro command

## What this is

How to add a new `/command` that a macro (or interactive session) can issue,
backed by a messenger class. This project uses two different messenger
styles side by side; this recipe covers both, when to pick which, a known
`G4GenericMessenger` pitfall this project has already hit once, and what
changes between Serial and MT.

## Generic pattern

A macro command is a `G4UIcommand` (or one of its typed subclasses,
`G4UIcmdWith*`) registered under a `G4UIdirectory` in the global command
tree, with a `G4UImessenger` behind it that converts the string argument(s)
into a value and does something with it. Two ways to build one:

1. **Plain `G4UImessenger` subclass.** Construct explicit
   `G4UIcmdWithAString`/`G4UIcmdWithADoubleAndUnit`/`G4UIcmdWithABool`/
   `G4UIcmdWithAnInteger`/`G4UIcmdWithoutParameter` objects in the
   constructor, each `new`'d with `this` as the messenger
   (`geant4SourcePath/source/intercoms/include/G4UIcommand.hh`), and
   override `SetNewValue(G4UIcommand*, G4String)` to dispatch on command
   identity (`command == fpSomeCmd`) and `GetCurrentValue(G4UIcommand*)` if
   the command should answer `/control/getEnv`-style queries. Destructor
   `delete`s each command object. This is the lower-level style: you write
   the per-type `GetNewXValue`/`ConvertToString` calls yourself.
2. **`G4GenericMessenger`.** Construct one `G4GenericMessenger` owning a
   directory path, then declare each command with `DeclareProperty` (binds
   directly to a member variable via `G4AnyType`) or `DeclareMethod` (binds
   to a setter method) — see the pitfall below for why that choice matters.
   `DeclareMethodWithUnit`/`DeclarePropertyWithUnit` take a default unit for
   a `G4double`. No `SetNewValue` override needed; `G4GenericMessenger`
   implements it once and dispatches internally
   (`geant4SourcePath/source/intercoms/src/G4GenericMessenger.cc:228-274`).

Either way, `AvailableForStates(G4State_PreInit, ...)` (plain style) or
`.SetStates(G4State_PreInit, ...)` (generic style, on the `Command&` that
`DeclareProperty`/`DeclareMethod` returns) restricts which application
states the command can be issued in; issuing it outside those states is a
`fIllegalApplicationState` from the UI manager, not a C++ exception.

## dnachem-min reference implementation

**Plain `G4UImessenger`, attaching to a directory another component already
created:**

- `src/chemistry/ChemistrySelectMessenger.cc` — `/chem/select <name>` and
  `/chem/list`, both `G4UIcmdWithAString`/`G4UIcmdWithoutParameter` objects
  created in the constructor, `SetNewValue` dispatching on command identity
  and calling `ChemistryRegistry::Select`, `GetCurrentValue` answering with
  the selected name. The header's own comment explains why this is a plain
  `G4UImessenger` rather than `G4GenericMessenger`:
  `/chem/` already exists (created by `G4DNAChemistryManager`), so a second
  `G4GenericMessenger(this, "/chem/", ...)` would try to create a second
  `/chem/` directory
  (`header/chemistry/ChemistrySelectMessenger.hh:5-7`).
- `src/chemistry/MesoMessenger.cc` — five PreInit commands
  (`/chem/meso/handOverTime`, `voxelSize`, `timesPerDecade`,
  `spatialOutput`, `enable`), each a typed `G4UIcmdWith*` built in the
  constructor with `SetGuidance`/`SetParameterName`/`SetDefaultUnit`/
  `AvailableForStates(G4State_PreInit)`; `SetNewValue` validates each value
  (e.g. `/chem/meso/handOverTime` must be `> 0`, else a fatal
  `G4Exception`) before writing it into the process-wide
  `MesoSettings::Current()`. The header explains this one creates its own
  new `/chem/meso/` directory as a side effect of its first command's path,
  still under the existing `/chem/` (`header/chemistry/MesoMessenger.hh:10-11`).

**`G4GenericMessenger`, owning a brand-new directory:**

- `src/geometry/DnaChemistryWorld.cc:20-33` — constructs
  `G4GenericMessenger(this, "/chem/env/", ...)` (a directory that does not
  exist yet at that point) and declares `pH` with `DeclareProperty` (binds
  directly to the `fpH` member) and `halfBox` with `DeclareMethodWithUnit`
  (binds to the `SetHalfBox` setter, which validates and rebuilds the
  chemistry boundary — logic a plain property write could not run). Each
  returned `Command&` gets `.SetStates(G4State_PreInit)`. The comment at
  line 31 notes that `ScavengerMessenger`, attaching `/chem/env/scavenger`,
  must be constructed *after* `fMessenger` so that the `/chem/env/`
  directory already exists for it to attach to — the same existing-directory
  consideration as the plain-messenger case above, just within one file.
- `src/chemistry/DnaChemistryList.cc:155-176` — constructs
  `G4GenericMessenger(this, "/chem/reaction/", ...)` and declares `dump`
  with `DeclareProperty`, `timeBinsFixed` with `DeclareMethodWithUnit`, and
  `timeBinsList` with `DeclareProperty` (not `DeclareMethod` — see below).

## Plain `G4UImessenger` vs `G4GenericMessenger`, and new vs existing directory

- Pick a **plain `G4UImessenger`** when the command must attach to a
  directory another component already created, because
  `G4GenericMessenger`'s constructor always tries to create the directory it
  is given (`G4UImessenger::CreateDirectory`,
  `geant4SourcePath/source/intercoms/src/G4UImessenger.cc`); a second
  `G4GenericMessenger` on the same path double-creates it. `/chem/` falls in
  this category because `G4DNAChemistryManager` already owns it before any
  project messenger runs — both `ChemistrySelectMessenger` and
  `MesoMessenger` are plain `G4UImessenger`s for exactly this reason (see
  their header comments above). `MesoMessenger`'s own `/chem/meso/`
  subdirectory, by contrast, does not exist yet when it runs, so its first
  `G4UIcmdWith*` constructor call creates it as a side effect — still fine
  as a plain messenger, since nothing else needs to pre-exist there.
- Pick **`G4GenericMessenger`** when the command's directory is new and
  nothing else needs to attach to it afterward (or anything that does can
  be sequenced after it, as `ScavengerMessenger` is after
  `DnaChemistryWorld`'s own `fMessenger`): it is less boilerplate for a
  handful of commands backed by simple property writes or single-argument
  setters, at the cost of the `DeclareMethod` pitfall below.

## The `DeclareMethod` multi-token string truncation pitfall

`G4GenericMessenger::SetNewValue` (`geant4SourcePath/source/intercoms/src/G4GenericMessenger.cc:228-274`)
handles a `DeclareMethod` command by tokenizing the command's raw value and
taking exactly as many tokens as the bound method has arguments:

```cpp
// G4GenericMessenger.cc:254-268
G4Tokenizer tokens(newValue);
G4String paraValue;
for (std::size_t i = 0; i < m.method.NArg(); ++i) {
  G4String aToken = tokens();
  ...
  paraValue += aToken + " ";
}
m.method.operator()(m.object, paraValue);
```

A method bound with `DeclareMethod`/`DeclareMethodWithUnit` that takes one
`G4String` argument has `NArg() == 1`: the loop runs once, consumes only the
*first* whitespace-separated token of whatever the user typed, and silently
drops the rest — there is no error, no exception, just a truncated value.
This bites exactly on a command meant to carry several tokens in one
string, e.g. `"1 10 100 1000 picosecond"` for a list of time-bin edges plus
a unit.

This project hit this for real: `DnaChemistryList`'s
`/chem/reaction/timeBinsList` command needs the whole
`"<e1> <e2> ... <eN> <unit>"` string intact, so it is declared with
**`DeclareProperty`, not `DeclareMethod`**
(`src/chemistry/DnaChemistryList.cc:171-176`), with the choice called out
explicitly in the surrounding comment (`DnaChemistryList.cc:170`:
`// DeclareProperty, not DeclareMethod: see fReactionTimeBinsList's doc
comment.`) and spelled out in full at the member's own doc comment
(`header/chemistry/DnaChemistryList.hh:138-144`):

> A `DeclareProperty` (not `DeclareMethod`): `G4GenericMessenger`'s method
> dispatch re-tokenizes a combined multi-token command value by the bound
> function's argument count, truncating a `"<e1> ... <eN> <unit>"` string to
> just its first token; `DeclareProperty`'s `G4String::FromString()` assigns
> it intact.

`DeclareProperty` instead routes the raw value through the bound variable's
own `G4AnyType::FromString()` (`G4GenericMessenger.cc:245-248`), which for a
`G4String` member assigns the whole value verbatim — no tokenization, no
truncation. The cost is that any validation/parsing that would have lived in
a setter method now has to happen later, lazily, once the raw string is
actually used: `DnaChemistryList::ApplyReactionTimeBinning()`
(`header/chemistry/DnaChemistryList.hh:67-76`) is that deferred parse site,
documented as running only once the chemistry scheduler's end time is final
(from `RunAction::BeginOfRunAction`), not at macro-issue time.

**Rule of thumb:** use `DeclareMethod` for a single scalar value (bool, int,
double-with-unit, or a string that is a single token, e.g. a name), and
`DeclareProperty` for anything that is itself a multi-token string the
command's own logic must parse — or sidestep the question entirely with the
plain-`G4UImessenger` style's one-command-one-`G4UIcmdWith*`-type mapping,
where there is no generic re-tokenization step to go wrong.

## Serial vs MT

A `PreInit`-state command issued once in a macro *is* replayed to every
worker thread, by a specific, source-verified mechanism — not a general
property of the kernel assumed from familiarity. Traced in
`geant4SourcePath/source/{intercoms,run}`:

1. On the master thread, `G4UImanager::ApplyCommand` normally executes a
   command once and returns. In MT mode it does one extra thing: if
   `stackCommandsForBroadcast` is set and the target command's
   `ToBeBroadcasted()` flag is true (the default for `G4UIcmdWith*` and for
   directories/commands created through `G4UImessenger`/`G4GenericMessenger`
   unless a messenger explicitly calls `SetToBeBroadcasted(false)`, as the
   built-in `/control/*` commands do), the command string is appended to a
   master-side `commandStack`
   (`geant4SourcePath/source/intercoms/src/G4UImanager.cc:539-553`).
2. `stackCommandsForBroadcast` is turned on by
   `G4UImanager::SetMasterUIManager(true)`
   (`geant4SourcePath/source/intercoms/include/G4UImanager.hh:210-213`),
   which `G4MTRunManager`'s constructor calls on the master's `G4UImanager`
   (`geant4SourcePath/source/run/src/G4MTRunManager.cc:146`) — this is the
   switch that makes MT mode record commands at all; it is off in Serial.
3. `G4MTRunManager::PrepareCommandsStack()` drains that master
   `commandStack` into `uiCmdsForWorkers`
   (`geant4SourcePath/source/run/src/G4MTRunManager.cc:286-295`), and is
   called right before `CreateAndStartWorkers()` on `RunInitialization`
   (`G4MTRunManager.cc:416`, also at `G4MTRunManager.cc:723` for a later
   `/run/beamOn`).
4. Each worker, in `G4WorkerRunManager::DoWork`, on receiving a
   `NEXTITERATION` request (i.e. before that worker starts its share of the
   run), fetches the same command list via
   `mrm->GetCommandStack()` and replays every entry through its own
   thread-local `G4UImanager::ApplyCommand`
   (`geant4SourcePath/source/run/src/G4WorkerRunManager.cc:736-741`) —
   *before* calling `this->BeamOn(...)` a few lines later in the same block.

So a `PreInit` command such as `/chem/meso/voxelSize` or
`/process/chem/TimeStepModel`, issued once in a macro before
`/run/initialize`, reaches the master's `G4UImanager` once, gets queued by
step 1, and is then re-applied verbatim on every worker thread's own
`G4UImanager` by step 4, before that worker's event loop starts. **Correction
verified while writing [`add-a-new-chemistry.md`](add-a-new-chemistry.md):**
each worker calls `SetNewValue` again on the *same*, single messenger
object, not on a new, per-thread instance — `MesoMessenger`, like
`DnaChemistryList`/`PhysicsList`, is constructed exactly once per process,
before any worker thread exists (`PhysicsList`'s constructor builds
`DnaChemistryList` once, `sim.cc:144`; the kernel then hands that one
object's pointer to every worker rather than rebuilding it — see
[`add-a-new-chemistry.md`](add-a-new-chemistry.md#serial-vs-mt) for the
full, source-traced proof, including the exact `G4MTRunManagerKernel.cc`
lines that share the pointer). The replay is harmless because each worker's
call writes the same value into whatever process-wide state the messenger
is bound to (e.g. `MesoSettings::Current()`) — it is a repeated write to
shared state, not N independently-constructed objects converging on a
value. This project runs Serial by default (`sim.cc`,
`G4RunManagerType::MT` only with `--threads N`, N > 0, per `CLAUDE.md`), so
in the common case there is exactly one `G4UImanager` and no replay step at
all — the mechanism above only matters once `--threads` is used.

This does not need a dedicated merge step the way a per-thread scorer does
(see [`create-a-scorer.md`](create-a-scorer.md)'s Serial vs MT section): a
macro command's job is to configure state before the run starts, not to
accumulate results, so "replay the same string on every thread" is the
whole story — there is nothing analogous to `Run::Merge` for a macro
command.

## See also

- [`create-a-scorer.md`](create-a-scorer.md) — the MT story for a
  per-thread scorer/counter is different (merge after the run, not replay
  before it); contrast with the mechanism above.
- [`dnachem-min-macro-command-reference.md`](dnachem-min-macro-command-reference.md)
  — every macro command this project defines, including the ones built in
  this recipe's reference implementations.
- [`docs/adr/0008-selectable-chemistry-modes.md`](../adr/0008-selectable-chemistry-modes.md)
  — `/chem/select`, `/process/chem/TimeStepModel` and `/chem/meso/enable`
  in context.
