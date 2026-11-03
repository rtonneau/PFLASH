// Macro builder model for the dnachem-min explorer (docs/explorer/index.html#builder).
//
// One configuration model drives the guide, the validation and the macro preview,
// so they cannot drift apart. Pure JavaScript, no DOM, no network: build.js inlines
// this file into index.html (window.MacroBuilder); macro-builder.test.js requires it.
//
// Every command, default, unit and rule below was read from the dnachem-min source
// or the configured Geant4 source (refs in each field's `refs`). When a messenger
// changes, update this file and its tests; see docs/explorer/README.md.
(function (root, factory) {
  'use strict';
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.MacroBuilder = api;
}(typeof self !== 'undefined' ? self : this, function () {
  'use strict';

  // ---- units (symbols from G4UnitsTable.cc; values in a common base per category) ----
  const UNITS = {
    time: { ps: 1e-12, ns: 1e-9, us: 1e-6, ms: 1e-3, s: 1 },
    length: { nm: 1e-9, um: 1e-6, mm: 1e-3 },
    energy: { eV: 1, keV: 1e3, MeV: 1e6 }
  };
  const O2_UNITS = ['%', 'M', 'mM', 'uM'];   // ScavengerSpec::Parse
  const O2_SATURATION_M = 0.0013;            // ScavengerSpec kO2SaturationMolarity (kH)
  const MAX_PIXELS = 65536;                  // MesoSettings kMaxPixels
  // G4EmDNAPhysics_option2.cc: emaxDNA = 1 MeV, the upper limit of the Geant4-DNA electron models.
  const E_DNA_MAX_EV = 1e6;

  const CHEMISTRIES = [
    // node: the Chemistry's box in the explorer Module map (architecture.json), its full description.
    { value: 'PureWater', label: 'PureWater', node: 'pure-water', note: 'Default. Base pure-water reactions plus the full O2-derived network and an acid-base buffer (pH has an effect).', buffer: true },
    { value: 'BoscoloChem', label: 'BoscoloChem', node: 'boscolo', note: 'Table 1 of Boscolo et al. 2020 (26 reactions). No acid-base buffer: pH has no chemical effect.', buffer: false },
    { value: 'Tonneau2025', label: 'Tonneau2025', node: 'tonneau', note: 'Table 2 of Tonneau et al. 2025, adds the HO3 molecule. Its 73 rows become 63 bimolecular and 24 bulk reactions here (some rows are both; R58 is the pH buffer). Has buffer ions: pH has an effect.', buffer: true },
    { value: 'Farokhi2023', label: 'Farokhi2023', node: 'farokhi', note: 'The 9 base-water reactions of Farokhi et al. 2023. No acid-base buffer: pH has no chemical effect.', buffer: false }
  ];
  function chemistryInfo(name) { return CHEMISTRIES.filter(function (c) { return c.value === name; })[0] || null; }

  const STEPS = [
    { id: 'basics', short: 'Basics', title: 'Basics and logging', intro: 'Name the macro and choose how much the simulation prints while it runs.' },
    { id: 'chemistry', short: 'Chemistry', title: 'Chemistry model', intro: 'Choose the reaction network and how chemistry time advances. These are fixed before the simulation is initialised.' },
    { id: 'timing', short: 'Timing', title: 'Timing and mesoscopic stage', intro: 'How long chemistry runs and, with the mesoscopic stage, when and on what mesh it takes over.',
      groups: [
        { id: 'time', title: 'Timing', intro: 'When the chemistry of each event stops, and how reaction counts are grouped in time.' },
        { id: 'meso', title: 'Mesoscopic stage', intro: 'Used only with IRT_syn and the mesoscopic stage on (step 2).', rule: 'mesoParams' }
      ] },
    { id: 'environment', short: 'Environment', title: 'Water environment', intro: 'The water box and what is dissolved in it.' },
    { id: 'beam', short: 'Beam', title: 'Beam', intro: 'The particle shot into the water and when its track is cut short.' },
    { id: 'runs', short: 'Runs', title: 'Runs and output', intro: 'How many events to simulate, the random seeds, and when results are written.' },
    { id: 'review', short: 'Review', title: 'Review and export', intro: 'Check the configuration and save the macro.' }
  ];

  const SRC = {
    logger: ['src/core/DnaLoggerMessenger.cc', 'src/core/DnaLogger.cc'],
    runVerbose: ['G4: source/run/src/G4RunMessenger.cc (/run/verbose, range 0..4)'],
    trackingVerbose: ['G4: source/tracking/src/G4TrackingMessenger.cc (/tracking/verbose, range >= -1)'],
    solvation: ['G4: source/processes/electromagnetic/utils/src/G4EmLowEParametersMessenger.cc', 'G4: source/processes/electromagnetic/dna/models/src/G4DNAOneStepThermalizationModel.cc (unset = Meesungnoen2002)'],
    chemistry: ['src/chemistry/ChemistrySelectMessenger.cc', 'src/chemistry/BuiltInChemistries.cc', 'docs/adr/0002-named-chemistries.md'],
    model: ['src/chemistry/DnaChemistryList.cc (CheckTimeStepModel)', 'G4: source/processes/electromagnetic/utils/src/G4EmLowEParametersMessenger.cc', 'docs/adr/0008-selectable-chemistry-modes.md'],
    meso: ['src/chemistry/MesoMessenger.cc', 'src/chemistry/MesoSettings.cc', 'docs/adr/0006-irt-syn-and-mesoscopic-chemistry.md'],
    endTime: ['src/chemistry/DnaChemistryList.cc (CheckTimeStepModel sets 1 s / 1 us at /run/initialize)', 'G4: source/processes/electromagnetic/dna/management/src/G4SchedulerMessenger.cc'],
    bins: ['src/chemistry/DnaChemistryList.cc (ApplyReactionTimeBinning)', 'src/scoring/ReactionCounter.cc (ParseBinEdgesList)'],
    o2: ['src/geometry/ScavengerMessenger.cc', 'src/geometry/ScavengerSpec.cc', 'docs/adr/0004-scavenger-reactions-per-chemistry.md'],
    pH: ['src/geometry/DnaChemistryWorld.cc', 'src/chemistry/catalog/PureWaterReactions.cc', 'docs/adr/0001-baseline-acid-base-buffer.md'],
    halfBox: ['src/geometry/DnaChemistryWorld.cc (SetHalfBox)'],
    outputDir: ['src/core/OutputDirMessenger.cc', 'src/core/OutputDir.cc'],
    gun: ['src/actions/PrimaryGeneratorAction.cc', 'G4: source/event/src/G4ParticleGunMessenger.cc'],
    dnaLimit: ['src/physics/PhysicsList.cc (G4EmDNAPhysics_option2)', 'G4: source/physics_lists/constructors/electromagnetic/src/G4EmDNAPhysics_option2.cc (emaxDNA = 1 MeV)'],
    position: ['src/actions/PrimaryGeneratorAction.cc (position set once in the constructor, kept between events)', 'src/geometry/DetectorConstruction.cc (water box centred on the origin)', 'G4: source/event/src/G4ParticleGunMessenger.cc (/gun/position)'],
    killer: ['src/scoring/PrimaryKiller.cc', 'src/geometry/DetectorConstruction.cc (defaults 500 keV / 1 MeV)'],
    runs: ['G4: source/run/src/G4RunMessenger.cc (/run/beamOn, /random/setSeeds)', 'src/scoring/RunAccumulatorMessenger.cc', 'src/core/OutputDir.cc (ConfigureSubdir)']
  };

  // ---- configuration ----
  function defaults() {
    return {
      title: '',
      filename: 'my_macro.in',
      logLevel: 'Quiet',
      runVerbose: '0',
      trackingVerbose: '0',
      solvation: 'default',
      chemistry: 'PureWater',
      model: 'IRT_syn',
      meso: true,
      endTimeMode: 'default',
      endTime: { value: '1', unit: 'ms' },
      handOver: { value: '5', unit: 'ns' },
      voxel: { value: '6.25', unit: 'nm' },
      timesPerDecade: '10',
      spatialOutput: false,
      binsMode: 'default',
      binWidth: { value: '1', unit: 'ns' },
      binList: { values: '1 10 100 1000', unit: 'ps' },
      o2: false,
      o2Value: { value: '21', unit: '%' },
      pH: '7',
      halfBox: { value: '500', unit: 'um' },
      outputDirMode: 'default',
      outputDir: '',
      particle: 'e-',
      energy: { value: '10', unit: 'keV' },
      beamZ: { value: '0', unit: 'um' },
      killerMode: 'default',
      eLossMin: { value: '500', unit: 'keV' },
      eLossMax: { value: '1', unit: 'MeV' },
      runs: [{ events: '2', seeds: '', dump: 'reset', name: '' }]
    };
  }
  const MESO_DEFAULTS = { handOver: { value: '5', unit: 'ns' }, voxel: { value: '6.25', unit: 'nm' }, timesPerDecade: '10', spatialOutput: false };

  function clone(x) { return JSON.parse(JSON.stringify(x)); }
  function get(cfg, path) {
    return String(path).split('.').reduce(function (o, k) { return o === undefined || o === null ? undefined : o[k]; }, cfg);
  }
  // Returns a new configuration; only the listed paths change.
  function applyPatch(cfg, patch) {
    const out = clone(cfg);
    Object.keys(patch).forEach(function (path) {
      const keys = path.split('.');
      let o = out;
      for (let i = 0; i < keys.length - 1; i++) o = o[keys[i]];
      o[keys[keys.length - 1]] = clone(patch[path]);
    });
    return out;
  }
  // Fills fields missing from an older saved configuration with defaults.
  function normalize(cfg) {
    const d = defaults();
    const out = Object.assign(d, cfg || {});
    if (!Array.isArray(out.runs) || !out.runs.length) out.runs = defaults().runs;
    return out;
  }

  // ---- parsing helpers ----
  const NUMBER_RE = /^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/;
  function num(text) {
    const s = String(text === undefined || text === null ? '' : text).trim();
    if (!s) return { empty: true };
    if (!NUMBER_RE.test(s)) return { bad: true };
    const v = Number(s);
    return isFinite(v) ? { value: v } : { bad: true };
  }
  function fmt(v) { return String(Number(v)); }
  function inBase(q, category) { const n = num(q.value); return n.value === undefined ? null : n.value * UNITS[category][q.unit]; }
  function sameQ(a, b) { const x = num(a.value), y = num(b.value); return a.unit === b.unit && x.value !== undefined && x.value === y.value; }

  function mesoOn(cfg) { return cfg.model !== 'SBS' && !!cfg.meso; }
  function mesoCustomized(cfg) {
    return !sameQ(cfg.handOver, MESO_DEFAULTS.handOver) || !sameQ(cfg.voxel, MESO_DEFAULTS.voxel) ||
      String(cfg.timesPerDecade).trim() !== MESO_DEFAULTS.timesPerDecade || !!cfg.spatialOutput;
  }
  function phCustomized(cfg) { const n = num(cfg.pH); return !(n.value === 7); }
  function killerApplies(cfg) { return cfg.particle === 'e-'; }
  // Effective chemistry end time in seconds (null when the custom value is invalid).
  function endTimeSeconds(cfg) {
    if (cfg.endTimeMode === 'custom') return inBase(cfg.endTime, 'time');
    return mesoOn(cfg) ? 1 : 1e-6;
  }
  function describeSeconds(s) {
    if (s === null) return 'invalid';
    const units = [['s', 1], ['ms', 1e-3], ['us', 1e-6], ['ns', 1e-9], ['ps', 1e-12]];
    for (const u of units) if (s >= u[1]) return fmt(+(s / u[1]).toPrecision(6)) + ' ' + u[0];
    return fmt(s / 1e-12) + ' ps';
  }

  // ---- availability rules (option or field level) ----
  // null = available; otherwise { kind: 'incompatible' | 'unsupported', why, fix? }.
  const RULES = {
    modeIRT: function () {
      return { kind: 'unsupported', why: 'Classic IRT is accepted by Geant4 but dnachem-min stops at /run/initialize (UnsupportedTimeStepModel): only IRT_syn and SBS are supported.' };
    },
    // The time-step model decides; the mesoscopic choice follows it. Under SBS the
    // stage is off automatically and the IRT_syn choice is kept for a switch back.
    mesoField: function (cfg) {
      if (cfg.model === 'SBS') {
        return { kind: 'automatic', why: 'SBS has no mesoscopic stage, so it is off. To choose, switch the time-step model to IRT_syn.',
          fix: { label: 'Switch the time-step model to IRT_syn', patch: { model: 'IRT_syn' } } };
      }
      return null;
    },
    mesoParams: function (cfg) {
      if (mesoOn(cfg)) return null;
      return { kind: 'incompatible', why: 'These settings only apply to the mesoscopic stage, which is off.',
        fix: { label: cfg.model === 'SBS' ? 'Use IRT_syn with the mesoscopic stage' : 'Turn the mesoscopic stage on',
          patch: cfg.model === 'SBS' ? { model: 'IRT_syn', meso: true } : { meso: true } } };
    },
    pH: function (cfg) {
      const c = chemistryInfo(cfg.chemistry);
      if (c && !c.buffer) {
        return { kind: 'incompatible', why: cfg.chemistry + ' has no acid-base buffer, so /chem/env/pH has no chemical effect.',
          fix: { label: 'Switch the Chemistry to PureWater', patch: { chemistry: 'PureWater' } } };
      }
      return null;
    },
    killer: function (cfg) {
      if (killerApplies(cfg)) return null;
      return { kind: 'incompatible', why: 'The primary-killer energy cuts act only on a primary electron (track 1, e-); with ' + cfg.particle + ' they do nothing.',
        fix: { label: 'Switch the particle to e-', patch: { particle: 'e-' } } };
    }
  };

  // ---- fields (the guide renders these; validation and generation read the same config) ----
  function q(id, opts) { return Object.assign({ id: id, path: id }, opts); }
  const FIELDS = [
    q('title', { step: 'basics', type: 'text', label: 'Description', required: false,
      help: 'One line describing what this macro is for. It is written as the first comment of the file.',
      tech: 'Written as a "#" comment line; the macro reader ignores it.', placeholder: 'e.g. 10 keV electrons in air-saturated water' }),
    q('logLevel', { step: 'basics', type: 'choice', label: 'Project log level', required: false,
      options: ['Quiet', 'Error', 'Warning', 'Info', 'Debug', 'Trace'].map(function (v) {
        return { value: v, label: v + (v === 'Quiet' ? ' (default)' : '') };
      }),
      help: 'How much dnachem-min itself reports. Info gives one line per chemistry stage and dump; Debug and Trace are for diagnosing problems.',
      tech: '/dnaLogger/verbose <level>, any application state. Default Quiet (written only when changed).', refs: SRC.logger }),
    q('runVerbose', { step: 'basics', type: 'number', label: 'Geant4 run verbosity', required: false, integer: true,
      help: 'Geant4 run-manager messages. 0 is silent.', tech: '/run/verbose 0..4, default 0 (written only when changed).', refs: SRC.runVerbose }),
    q('trackingVerbose', { step: 'basics', type: 'number', label: 'Geant4 tracking verbosity', required: false, integer: true,
      help: 'Prints every step of every track. Leave at 0 except for debugging a single event: output is huge.',
      tech: '/tracking/verbose >= -1, default 0 (written only when changed).', refs: SRC.trackingVerbose }),

    q('chemistry', { step: 'chemistry', type: 'choice', label: 'Chemistry (reaction network)', required: true,
      options: CHEMISTRIES.map(function (c) {
        return { value: c.value, label: c.label + (c.value === 'PureWater' ? ' (default)' : ''), help: c.note,
          link: { label: 'Full description of ' + c.label, node: c.node } };
      }),
      help: 'The set of chemical reactions. Molecules and water dissociation are shared; the reactions and their rates come from the selected Chemistry.',
      tech: '/chem/select <name>, PreInit (before /run/initialize). Only one Chemistry per process.', refs: SRC.chemistry }),
    q('model', { step: 'chemistry', type: 'choice', label: 'Chemistry time-step model', required: true,
      options: [
        { value: 'IRT_syn', label: 'IRT_syn (default)', help: 'Independent reaction times, synchronised. Fast; can hand over to the mesoscopic stage.' },
        { value: 'SBS', label: 'SBS', help: 'Step-by-step diffusion of every molecule. Slower; every reaction fully diffusion-controlled; no mesoscopic stage (turned off automatically).' },
        { value: 'IRT', label: 'IRT (classic)', help: 'Geant4 offers it, dnachem-min does not.', rule: 'modeIRT' }
      ],
      help: 'How the simulation advances chemistry in time. Choosing SBS turns the mesoscopic stage off.',
      tech: '/process/chem/TimeStepModel IRT_syn|SBS, PreInit. Always written so the mode is explicit.', refs: SRC.model }),
    q('meso', { step: 'chemistry', type: 'bool', label: 'Mesoscopic stage', required: true, rule: 'mesoField',
      // Shown value: under SBS the stage is off whatever the kept IRT_syn choice is.
      effective: function (c) { return c.model === 'SBS' ? false : c.meso; },
      options: [
        { value: true, label: 'On (default with IRT_syn)', help: 'At the hand-over time the remaining molecules move to counts on a cell mesh, which is much faster for long times (up to 1 s by default).' },
        { value: false, label: 'Off', help: 'The particle-based stage runs to the end time (1 us by default). No SpeciesMeso output.' }
      ],
      help: 'Whether the IRT_syn particle-based chemistry hands over to a faster mesh-based model at the hand-over time. Only IRT_syn has it.',
      tech: '/chem/meso/enable true|false, PreInit, written with IRT_syn only. Under SBS nothing is written: the code turns the stage off itself, and an explicit true would be fatal (MesoWithSbs).', refs: SRC.meso }),
    q('solvation', { step: 'chemistry', type: 'choice', label: 'Electron solvation model', required: false,
      options: [
        { value: 'default', label: 'Not set (Geant4 uses Meesungnoen2002)', help: 'No command is written.' },
        { value: 'Ritchie1994', label: 'Ritchie1994', help: 'Used by most project macros.' },
        { value: 'Meesungnoen2002', label: 'Meesungnoen2002', help: 'Same model as "not set", written explicitly.' },
        { value: 'Terrisol1990', label: 'Terrisol1990' }
      ],
      help: 'How far a slowed-down electron travels before it becomes a solvated electron (e_aq). It shifts the initial e_aq distribution.',
      tech: '/process/dna/e-SolvationSubType, PreInit. Geant4 also accepts Kreipl2009 and Meesungnoen2002_amorphous; they are not offered because no dnachem-min macro or test exercises them.', refs: SRC.solvation }),

    q('endTimeMode', { step: 'timing', group: 'time', type: 'choice', label: 'Chemistry end time', required: true,
      options: [
        { value: 'default', label: 'Default', help: '1 s with the mesoscopic stage, 1 us without. A full 1 s run is slow.' },
        { value: 'custom', label: 'Custom', help: 'For example 1 ms for a quick smoke test.' }
      ],
      help: 'When the chemistry of each event stops.',
      tech: '/scheduler/endTime <value> <unit>, written after /run/initialize: initialization resets the end time to the mode default.', refs: SRC.endTime }),
    q('endTime', { step: 'timing', group: 'time', type: 'quantity', category: 'time', label: 'End time', required: true, showIf: function (c) { return c.endTimeMode === 'custom'; },
      help: 'Must be greater than 0.', refs: SRC.endTime }),
    q('handOver', { step: 'timing', type: 'quantity', category: 'time', label: 'Hand-over time', required: false, rule: 'mesoParams', group: 'meso',
      help: 'When the particle-based stage hands over to the mesoscopic stage. Default 5 ns.',
      tech: '/chem/meso/handOverTime <value> <unit>, PreInit, > 0. Written only when changed.', refs: SRC.meso }),
    q('voxel', { step: 'timing', type: 'quantity', category: 'length', label: 'Requested mesh cell size', required: false, rule: 'mesoParams', group: 'meso',
      help: 'Target side of one mesh cell. The mesh uses the power-of-2 cell count closest to it, at most 65536 cells per side. Default 6.25 nm.',
      tech: '/chem/meso/voxelSize <value> <unit>, PreInit, > 0. Written only when changed.', refs: SRC.meso }),
    q('timesPerDecade', { step: 'timing', type: 'number', integer: true, label: 'Record times per decade', required: false, rule: 'mesoParams', group: 'meso',
      help: 'How many output times per factor of 10 in time, from the hand-over time to the end time. Default 10.',
      tech: '/chem/meso/timesPerDecade <n>, PreInit, >= 1. Written only when changed.', refs: SRC.meso }),
    q('spatialOutput', { step: 'timing', type: 'bool', label: 'Spatial snapshots (HDF5)', required: false, rule: 'mesoParams', group: 'meso', group: 'meso',
      options: [{ value: false, label: 'Off (default)' }, { value: true, label: 'On: write SpeciesMesoSpatial.h5' }],
      help: 'Saves where the species are on the mesh at every record time. The file can be very large.',
      tech: '/chem/meso/spatialOutput true, PreInit. Written only when on.', refs: SRC.meso }),
    q('binsMode', { step: 'timing', group: 'time', type: 'choice', label: 'Reaction-count time bins', required: false,
      options: [
        { value: 'default', label: 'Built-in table (default)', help: 'A fixed 7-edge table.' },
        { value: 'fixed', label: 'Fixed width' },
        { value: 'list', label: 'Explicit edges' }
      ],
      help: 'How reaction counts are grouped in time in Reactions.Txt (particle-based stage only). The two custom forms cannot be combined.',
      tech: '/chem/reaction/timeBinsFixed or /chem/reaction/timeBinsList, PreInit; issuing both is fatal (ConflictingTimeBins).', refs: SRC.bins }),
    q('binWidth', { step: 'timing', group: 'time', type: 'quantity', category: 'time', label: 'Bin width', required: true, showIf: function (c) { return c.binsMode === 'fixed'; },
      help: 'Bins of this width up to the end time.', refs: SRC.bins }),
    q('binList', { step: 'timing', group: 'time', type: 'list', category: 'time', label: 'Bin edges', required: true, showIf: function (c) { return c.binsMode === 'list'; },
      help: 'Upper bin edges separated by spaces, all in one unit.', refs: SRC.bins }),

    q('halfBox', { step: 'environment', type: 'quantity', category: 'length', label: 'Water box half side', required: false,
      help: 'The water box is the whole geometry: it bounds the tracks, chemistry diffusion and the number of dissolved scavenger molecules. Default 500 um (a 1 mm cube).',
      tech: '/chem/env/halfBox <value> <unit>, PreInit, > 0. Written only when changed.', refs: SRC.halfBox }),
    q('o2', { step: 'environment', type: 'bool', label: 'Dissolved oxygen (O2)', required: false,
      options: [{ value: false, label: 'None (default, anoxic)' }, { value: true, label: 'Add dissolved O2' }],
      help: 'O2 reacts with e_aq, H and O- through the selected Chemistry\'s bulk reactions. Every Chemistry here has them.',
      tech: '/chem/env/scavenger O2 <value> <unit>, PreInit. Other scavenger species are not offered: no Chemistry here has bulk reactions for them, so they would be inert.', refs: SRC.o2 }),
    q('o2Value', { step: 'environment', type: 'quantity', category: 'o2', label: 'O2 concentration', required: true, showIf: function (c) { return c.o2; },
      help: '% means percent of a pure-O2 atmosphere (Henry\'s law, kH = 0.0013 M): 21 % is air-saturated water (2.73e-4 M). 0 means absent.', refs: SRC.o2 }),
    q('pH', { step: 'environment', type: 'number', label: 'pH', required: false, rule: 'pH',
      help: 'Sets the bulk H3O+ / OH- buffer concentrations used by the acid-base reactions. Default 7.',
      tech: '/chem/env/pH <value>, PreInit. The code does not range-check it. Written only when changed.', refs: SRC.pH }),

    q('particle', { step: 'beam', type: 'choice', label: 'Particle', required: true,
      options: [
        { value: 'e-', label: 'Electron, e- (default)', help: 'The project\'s main use case.' },
        { value: 'proton', label: 'Proton', help: 'Used by the Farokhi2023 comparison macros.' }
      ],
      help: 'The primary particle. It moves along +z from a start point on the central axis of the box (below).',
      tech: '/gun/particle, after /run/initialize. Defaults in PrimaryGeneratorAction: e-, 2 MeV, (0,0,0), +z.', refs: SRC.gun }),
    q('energy', { step: 'beam', type: 'quantity', category: 'energy', label: 'Kinetic energy', required: true,
      help: 'Must be greater than 0, and at most 1 MeV for electrons (Geant4-DNA limit). Project tests use 10 keV electrons.',
      tech: '/gun/energy <value> <unit>. The Geant4-DNA electron models of G4EmDNAPhysics_option2 stop at 1 MeV (emaxDNA); above it standard EM physics would take over, so higher electron energies are not offered.',
      refs: SRC.gun.concat(SRC.dnaLimit) }),
    q('beamZ', { step: 'beam', type: 'quantity', category: 'length', label: 'Start position on the beam axis (z)', required: true,
      help: 'The beam starts at (0, 0, z) and moves along +z. 0 (default) is the box centre; a negative value starts it upstream, and a value close to minus the half side makes it cross the whole box. Must be strictly inside the box.',
      tech: '/gun/position 0 0 <z> <unit>, after /run/initialize, written only when z is not 0. x, y and the +z direction stay fixed.', refs: SRC.position }),
    q('killerMode', { step: 'beam', type: 'choice', label: 'Primary-electron energy cuts', required: false, rule: 'killer',
      options: [
        { value: 'default', label: 'Code defaults (500 keV / 1 MeV)', help: 'Already active in every run, no command written.' },
        { value: 'custom', label: 'Custom' },
        { value: 'off', label: 'Off: no energy cuts', help: 'The primary electron is followed until it stops or leaves the box.' }
      ],
      help: 'The primary electron is stopped once it has lost "stop after" energy, and the whole event is discarded once it has lost "abort after".',
      tech: '/primaryKiller/eLossMin and /primaryKiller/eLossMax; "Off" writes /primaryKiller/energyCuts false, which disables every primary-electron energy cut and keeps only the box cut. These commands exist only after /run/initialize. /primaryKiller/minKineticE (kill the primary at or below a kinetic energy) is not offered.', refs: SRC.killer }),
    q('eLossMin', { step: 'beam', type: 'quantity', category: 'energy', label: 'Stop the primary after losing', required: true, rule: 'killer', showIf: function (c) { return c.killerMode === 'custom'; }, refs: SRC.killer }),
    q('eLossMax', { step: 'beam', type: 'quantity', category: 'energy', label: 'Abort the event after losing', required: true, rule: 'killer', showIf: function (c) { return c.killerMode === 'custom'; }, refs: SRC.killer }),

    q('runs', { step: 'runs', type: 'runs', label: 'Sub-runs', required: true,
      help: 'Each sub-run simulates some events. Results accumulate across sub-runs until a dump writes and resets them.',
      tech: '/random/setSeeds, /run/beamOn <events>, then /run/dumpDataAndReset [prefix] or /run/dumpDataAndResetToDir <subfolder>.', refs: SRC.runs }),
    q('outputDirMode', { step: 'runs', type: 'choice', label: 'Output folder', required: false,
      options: [
        { value: 'default', label: 'Default (<build>/results, or --dir)', help: 'Recommended: choose the folder at launch with --dir.' },
        { value: 'custom', label: 'Fixed in the macro' }
      ],
      help: 'Where all output files go.',
      tech: '/run/outputDir <path>, PreInit. Launching with a different --dir is then fatal (ConflictingOutputDir).', refs: SRC.outputDir }),
    q('outputDir', { step: 'runs', type: 'text', label: 'Output folder path', required: true, showIf: function (c) { return c.outputDirMode === 'custom'; },
      help: 'Its parent folder must already exist. No spaces or "#".', placeholder: 'results_o2' })
  ];
  const FIELD = {};
  FIELDS.forEach(function (f) { FIELD[f.id] = f; });

  function fieldAvailability(cfg, f) { return f.rule ? RULES[f.rule](cfg) : null; }
  function groupAvailability(cfg, g) { return g && g.rule ? RULES[g.rule](cfg) : null; }
  // The value a control shows (a field may follow another one, e.g. meso under SBS).
  function displayValue(cfg, f) { return f.effective ? f.effective(cfg) : get(cfg, f.path); }
  function optionAvailability(cfg, o) { return o.rule ? RULES[o.rule](cfg) : null; }
  function visible(cfg, f) { return !f.showIf || f.showIf(cfg); }

  // ---- validation ----
  const BLOCKING = { incomplete: true, invalid: true, incompatible: true, unsupported: true };
  function validate(cfg) {
    const issues = [];
    function add(field, kind, message, resolutions, severity) {
      const step = (FIELD[String(field).split('.')[0]] || {}).step || 'review';
      issues.push({ field: field, step: step, kind: kind, severity: severity || (BLOCKING[kind] ? 'error' : kind),
        message: message, resolutions: resolutions || [] });
    }
    function positive(field, qty, what) {
      const n = num(qty.value);
      if (n.empty) add(field, 'incomplete', what + ' is required.');
      else if (n.bad) add(field, 'invalid', what + ': "' + qty.value + '" is not a number.');
      else if (!(n.value > 0)) add(field, 'invalid', what + ' must be greater than 0.');
      return n.value > 0;
    }
    function integer(field, text, min, what, maxv) {
      const s = String(text).trim();
      if (!s) { add(field, 'incomplete', what + ' is required.'); return false; }
      if (!/^[+-]?\d+$/.test(s)) { add(field, 'invalid', what + ' must be a whole number.'); return false; }
      const v = Number(s);
      if (v < min || (maxv !== undefined && v > maxv)) {
        add(field, 'invalid', what + ' must be ' + (maxv !== undefined ? 'between ' + min + ' and ' + maxv : 'at least ' + min) + '.');
        return false;
      }
      return true;
    }
    function unitOk(field, qty, units) {
      if (units.indexOf(qty.unit) < 0) { add(field, 'invalid', 'Unit "' + qty.unit + '" is not supported here (use ' + units.join(', ') + ').'); return false; }
      return true;
    }
    function safeText(field, text, what) {
      if (/[\s#"]/.test(text)) { add(field, 'invalid', what + ' must not contain spaces, quotes or "#" (Geant4 splits macro parameters at spaces and treats "#" as a comment).'); return false; }
      return true;
    }

    // Basics
    integer('runVerbose', cfg.runVerbose, 0, 'Run verbosity', 4);
    integer('trackingVerbose', cfg.trackingVerbose, -1, 'Tracking verbosity');
    if (/[\r\n]/.test(cfg.title)) add('title', 'invalid', 'The description must be a single line.');

    // Chemistry
    if (!chemistryInfo(cfg.chemistry)) add('chemistry', 'invalid', 'Unknown Chemistry "' + cfg.chemistry + '".');
    if (cfg.model === 'IRT') add('model', 'unsupported', RULES.modeIRT().why, [{ label: 'Use IRT_syn', patch: { model: 'IRT_syn' } }, { label: 'Use SBS', patch: { model: 'SBS' } }]);
    else if (cfg.model !== 'IRT_syn' && cfg.model !== 'SBS') add('model', 'invalid', 'Unknown time-step model "' + cfg.model + '".');

    // Timing
    if (cfg.endTimeMode === 'custom') {
      unitOk('endTime', cfg.endTime, Object.keys(UNITS.time));
      positive('endTime', cfg.endTime, 'End time');
    }
    const endS = endTimeSeconds(cfg);
    if (mesoOn(cfg)) {
      unitOk('handOver', cfg.handOver, Object.keys(UNITS.time));
      unitOk('voxel', cfg.voxel, Object.keys(UNITS.length));
      const hoOk = positive('handOver', cfg.handOver, 'Hand-over time');
      positive('voxel', cfg.voxel, 'Mesh cell size');
      integer('timesPerDecade', cfg.timesPerDecade, 1, 'Record times per decade');
      const ho = inBase(cfg.handOver, 'time');
      if (hoOk && endS !== null && ho >= endS) {
        add('handOver', 'warning', 'The hand-over time (' + describeSeconds(ho) + ') is not before the end time (' + describeSeconds(endS) +
          '): chemistry stops before the hand-over, so the mesoscopic stage never runs.');
      }
      const side = inBase(cfg.halfBox, 'length'), vox = inBase(cfg.voxel, 'length');
      if (side > 0 && vox > 0 && UNITS.length[cfg.halfBox.unit] && UNITS.length[cfg.voxel.unit]) {
        const p = pixelCount(2 * side, vox);
        if (p.capped) {
          add('voxel', 'info', 'The mesh is capped at ' + MAX_PIXELS + ' cells per side, so cells will be ' +
            fmt(+(2 * side / MAX_PIXELS / 1e-9).toPrecision(4)) + ' nm instead of the requested ' + fmt(vox / 1e-9) + ' nm (expected for the default 1 mm box; see ADR 0006).');
        }
      }
    } else if (mesoCustomized(cfg)) {
      // Not blocking: the stage being off is a valid choice (SBS turns it off by
      // itself). The kept values are reported, not dropped, and not written.
      const resolutions = [{ label: 'Discard the mesoscopic settings (restore defaults)', patch: clone(MESO_DEFAULTS) }];
      resolutions.push(cfg.model === 'SBS' ? { label: 'Use IRT_syn with the mesoscopic stage', patch: { model: 'IRT_syn', meso: true } } : { label: 'Turn the mesoscopic stage back on', patch: { meso: true } });
      add('handOver', 'warning', 'Mesoscopic settings were changed, but the mesoscopic stage is off' + (cfg.model === 'SBS' ? ' (SBS has none)' : '') +
        ', so they have no effect. They are kept here but not written to the macro.', resolutions);
    }
    if (cfg.binsMode === 'fixed') {
      unitOk('binWidth', cfg.binWidth, Object.keys(UNITS.time));
      if (positive('binWidth', cfg.binWidth, 'Bin width') && endS !== null) {
        const n = Math.ceil(endS / inBase(cfg.binWidth, 'time'));
        if (n > 100000) add('binWidth', 'warning', 'This gives about ' + n.toExponential(2) + ' bins up to the ' + describeSeconds(endS) + ' end time; the bin table is built in memory before the run. Use a wider bin or explicit edges.');
      }
    } else if (cfg.binsMode === 'list') {
      unitOk('binList', cfg.binList, Object.keys(UNITS.time));
      const tokens = String(cfg.binList.values).trim().split(/\s+/).filter(Boolean);
      if (!tokens.length) add('binList', 'incomplete', 'Enter at least one bin edge.');
      const bad = tokens.filter(function (t) { return num(t).value === undefined; });
      if (bad.length) add('binList', 'invalid', '"' + bad[0] + '" is not a valid edge value.');
      else if (tokens.some(function (t) { return num(t).value <= 0; })) add('binList', 'warning', 'Edges at or below 0 never receive a reaction.');
    }

    // Environment
    unitOk('halfBox', cfg.halfBox, Object.keys(UNITS.length));
    positive('halfBox', cfg.halfBox, 'Half side');
    if (cfg.o2) {
      unitOk('o2Value', cfg.o2Value, O2_UNITS);
      const n = num(cfg.o2Value.value);
      if (n.empty) add('o2Value', 'incomplete', 'O2 concentration is required.');
      else if (n.bad) add('o2Value', 'invalid', 'O2 concentration: "' + cfg.o2Value.value + '" is not a number.');
      else if (n.value < 0) add('o2Value', 'invalid', 'The O2 concentration cannot be negative.');
      else if (n.value === 0) add('o2Value', 'info', '0 means no dissolved O2 (same as not adding it).');
    }
    const ph = num(cfg.pH);
    if (ph.empty) add('pH', 'incomplete', 'pH is required (default 7).');
    else if (ph.bad) add('pH', 'invalid', 'pH: "' + cfg.pH + '" is not a number.');
    else if (RULES.pH(cfg) && phCustomized(cfg)) {
      add('pH', 'incompatible', RULES.pH(cfg).why + ' The changed pH would be inert, so it is not written.',
        [{ label: 'Reset pH to 7', patch: { pH: '7' } }, { label: 'Switch the Chemistry to PureWater', patch: { chemistry: 'PureWater' } }]);
    }

    // Beam
    if (['e-', 'proton'].indexOf(cfg.particle) < 0) add('particle', 'invalid', 'Unknown particle "' + cfg.particle + '".');
    if (unitOk('energy', cfg.energy, Object.keys(UNITS.energy)) && positive('energy', cfg.energy, 'Kinetic energy') &&
        cfg.particle === 'e-' && inBase(cfg.energy, 'energy') > E_DNA_MAX_EV) {
      add('energy', 'unsupported', 'Electrons above 1 MeV are outside the Geant4-DNA electron models (G4EmDNAPhysics_option2, emaxDNA = 1 MeV).',
        [{ label: 'Set 1 MeV', patch: { energy: { value: '1', unit: 'MeV' } } }]);
    }
    if (unitOk('beamZ', cfg.beamZ, Object.keys(UNITS.length))) {
      const z = num(cfg.beamZ.value), half = inBase(cfg.halfBox, 'length');
      if (z.empty) add('beamZ', 'incomplete', 'Start position is required (0 is the box centre).');
      else if (z.bad) add('beamZ', 'invalid', 'Start position: "' + cfg.beamZ.value + '" is not a number.');
      else if (half > 0 && UNITS.length[cfg.halfBox.unit] && Math.abs(inBase(cfg.beamZ, 'length')) >= half) {
        add('beamZ', 'invalid', 'The start position must be strictly inside the water box: |z| < half side (' + qty(cfg.halfBox) + ').',
          [{ label: 'Start at the box centre', patch: { beamZ: { value: '0', unit: cfg.beamZ.unit } } }]);
      }
    }
    if (cfg.killerMode === 'custom') {
      if (!killerApplies(cfg)) {
        add('killerMode', 'incompatible', RULES.killer(cfg).why + ' The custom cuts are not written.',
          [{ label: 'Use the code defaults (remove custom cuts)', patch: { killerMode: 'default' } }, { label: 'Switch the particle to e-', patch: { particle: 'e-' } }]);
      } else {
        unitOk('eLossMin', cfg.eLossMin, Object.keys(UNITS.energy));
        unitOk('eLossMax', cfg.eLossMax, Object.keys(UNITS.energy));
        const a = positive('eLossMin', cfg.eLossMin, 'Stop threshold'), b = positive('eLossMax', cfg.eLossMax, 'Abort threshold');
        if (a && b && inBase(cfg.eLossMax, 'energy') < inBase(cfg.eLossMin, 'energy')) {
          add('eLossMax', 'warning', 'The abort threshold is below the stop threshold, so events are discarded before the primary would be stopped.');
        }
      }
    }

    // Runs and output
    if (!cfg.runs.length) add('runs', 'incomplete', 'Add at least one sub-run.');
    const prefixes = {}, subdirs = {};
    cfg.runs.forEach(function (r, i) {
      const p = 'runs.' + i + '.', label = 'Sub-run ' + (i + 1);
      integer(p + 'events', r.events, 1, label + ': number of events');
      const seeds = String(r.seeds || '').trim();
      if (seeds) {
        const t = seeds.split(/\s+/);
        if (t.length < 2) add(p + 'seeds', 'invalid', label + ': give at least two seeds (Geant4 ignores a single value).');
        else if (t.some(function (s) { return !/^\d+$/.test(s); })) add(p + 'seeds', 'invalid', label + ': seeds must be whole numbers separated by spaces.');
      }
      const name = String(r.name || '').trim();
      if (r.dump === 'reset') {
        if (name && !/^[A-Za-z0-9._-]+$/.test(name)) add(p + 'name', 'invalid', label + ': the prefix may only use letters, digits, ".", "_" and "-".');
        if (prefixes[name] !== undefined) {
          add(p + 'name', 'invalid', label + ': ' + (name ? 'prefix "' + name + '"' : 'a dump without prefix') + ' was already used by sub-run ' + (prefixes[name] + 1) + '; reusing it is fatal (DuplicateDumpPrefix).');
        } else prefixes[name] = i;
        if (name === 'EndOfRun_') add(p + 'name', 'warning', label + ': "EndOfRun_" is the prefix of the automatic exit dump; choose another to avoid overwriting.');
      } else if (r.dump === 'dir') {
        if (!name) add(p + 'name', 'incomplete', label + ': enter a subfolder name.');
        else if (!/^[A-Za-z0-9._/-]+$/.test(name)) add(p + 'name', 'invalid', label + ': the subfolder may only use letters, digits, ".", "_", "-" and "/".');
        else if (name.charAt(0) === '/' || name.split('/').indexOf('..') >= 0) add(p + 'name', 'invalid', label + ': the subfolder must be relative and must not contain "..".');
        else if (subdirs[name] !== undefined) add(p + 'name', 'invalid', label + ': subfolder "' + name + '" was already used by sub-run ' + (subdirs[name] + 1) + '; reusing it is fatal (DuplicateDumpSubdir).');
        else subdirs[name] = i;
      } else if (r.dump !== 'none') add(p + 'dump', 'invalid', label + ': unknown dump choice.');
    });
    if (cfg.runs.length && cfg.runs[cfg.runs.length - 1].dump === 'none') {
      add('runs.' + (cfg.runs.length - 1) + '.dump', 'info', 'The last sub-run is not dumped: its data are written at exit with the prefix EndOfRun_ (safety net).');
    }
    if (cfg.outputDirMode === 'custom') {
      const d = String(cfg.outputDir || '').trim();
      if (!d) add('outputDir', 'incomplete', 'Enter the output folder path.');
      else safeText('outputDir', d, 'The output folder path');
    }
    return issues;
  }
  function errors(issues) { return issues.filter(function (i) { return i.severity === 'error'; }); }

  function pixelCount(boxSide, target) {   // MesoSettings::PixelCount / PixelCountCapped
    let best = 1, bestDiff = Math.abs(boxSide - target);
    for (let k = 1; k <= 30; k++) {
      const n = Math.pow(2, k), d = Math.abs(boxSide / n - target);
      if (d <= bestDiff) { best = n; bestDiff = d; }
    }
    return { pixels: Math.min(best, MAX_PIXELS), capped: best > MAX_PIXELS };
  }

  // ---- filename ----
  function sanitizeFilename(name) {
    let s = String(name || '').trim().replace(/\s+/g, '_').replace(/[^A-Za-z0-9._-]/g, '');
    s = s.replace(/^[.-]+/, '');
    s = s.replace(/(\.in)+$/i, '').replace(/\.+$/, '');
    if (!s) s = 'macro';
    return s.slice(0, 120) + '.in';
  }

  // ---- generation ----
  function qty(x) { return fmt(x.value) + ' ' + x.unit; }
  function generate(cfg) {
    const lines = [];
    function c(text) { lines.push({ text: text ? '# ' + text : '#', field: null }); }
    function blank() { if (lines.length && lines[lines.length - 1].text !== '') lines.push({ text: '', field: null }); }
    function cmd(text, field) { lines.push({ text: text, field: field }); }
    const meso = mesoOn(cfg), file = sanitizeFilename(cfg.filename);
    const title = String(cfg.title || '').replace(/[\r\n]+/g, ' ').trim();

    c(title || 'dnachem-min simulation macro');
    c('Created with the dnachem-min explorer Macro builder (docs/explorer/index.html#builder).');
    c('Configuration checked against the dnachem-min source; this file has not been run.');
    c('Run from the build directory:  ./sim ' + file);
    c('Mode: ' + cfg.chemistry + ', ' + cfg.model + (meso ? ' + mesoscopic stage' : (cfg.model === 'SBS' ? '' : ', particle-based stage only')) +
      ', end time ' + describeSeconds(endTimeSeconds(cfg)) + (cfg.endTimeMode === 'custom' ? '' : ' (default)') + '.');

    const verb = [];
    if (cfg.logLevel !== 'Quiet') verb.push(['/dnaLogger/verbose ' + cfg.logLevel, 'logLevel']);
    if (String(cfg.runVerbose).trim() !== '0') verb.push(['/run/verbose ' + String(cfg.runVerbose).trim(), 'runVerbose']);
    if (String(cfg.trackingVerbose).trim() !== '0') verb.push(['/tracking/verbose ' + String(cfg.trackingVerbose).trim(), 'trackingVerbose']);
    if (verb.length) { blank(); c('Logging'); verb.forEach(function (v) { cmd(v[0], v[1]); }); }

    blank();
    c('---- Before /run/initialize (PreInit settings) ----');
    if (cfg.solvation !== 'default') { c('Electron solvation (thermalization) model'); cmd('/process/dna/e-SolvationSubType ' + cfg.solvation, 'solvation'); }
    c('Chemistry: reaction network');
    cmd('/chem/select ' + cfg.chemistry, 'chemistry');
    c('Chemistry time-step model and mesoscopic stage');
    cmd('/process/chem/TimeStepModel ' + cfg.model, 'model');
    // SBS has no mesoscopic stage and the code turns it off itself; an explicit
    // "true" would be fatal, so nothing is written for it under SBS.
    if (cfg.model === 'SBS') c('SBS: no mesoscopic stage (off automatically)');
    else cmd('/chem/meso/enable ' + (cfg.meso ? 'true' : 'false'), 'meso');
    if (meso) {
      if (!sameQ(cfg.handOver, MESO_DEFAULTS.handOver)) cmd('/chem/meso/handOverTime ' + qty(cfg.handOver), 'handOver');
      if (!sameQ(cfg.voxel, MESO_DEFAULTS.voxel)) cmd('/chem/meso/voxelSize ' + qty(cfg.voxel), 'voxel');
      if (String(cfg.timesPerDecade).trim() !== MESO_DEFAULTS.timesPerDecade) cmd('/chem/meso/timesPerDecade ' + String(cfg.timesPerDecade).trim(), 'timesPerDecade');
      if (cfg.spatialOutput) { c('Per-event mesh snapshots in SpeciesMesoSpatial.h5 (large file)'); cmd('/chem/meso/spatialOutput true', 'spatialOutput'); }
    }
    if (cfg.binsMode === 'fixed') { c('Reaction-count time bins of fixed width'); cmd('/chem/reaction/timeBinsFixed ' + qty(cfg.binWidth), 'binWidth'); }
    if (cfg.binsMode === 'list') {
      c('Reaction-count time bin edges');
      cmd('/chem/reaction/timeBinsList ' + String(cfg.binList.values).trim().split(/\s+/).map(function (t) { return num(t).value === undefined ? t : fmt(t); }).join(' ') + ' ' + cfg.binList.unit, 'binList');
    }
    const env = [];
    if (!sameQ(cfg.halfBox, { value: '500', unit: 'um' })) env.push(['Water box half side (the whole geometry)', '/chem/env/halfBox ' + qty(cfg.halfBox), 'halfBox']);
    if (cfg.o2) {
      const o2 = num(cfg.o2Value.value).value;
      const note = cfg.o2Value.unit === '%' && o2 !== undefined ? ' (' + (o2 / 100 * O2_SATURATION_M).toExponential(3) + ' M)' : '';
      env.push(['Dissolved O2 bulk scavenger' + note, '/chem/env/scavenger O2 ' + fmt(cfg.o2Value.value) + ' ' + cfg.o2Value.unit, 'o2Value']);
    }
    if (phCustomized(cfg) && !RULES.pH(cfg) && num(cfg.pH).value !== undefined) env.push(['Bulk water pH', '/chem/env/pH ' + fmt(cfg.pH), 'pH']);
    env.forEach(function (e) { c(e[0]); cmd(e[1], e[2]); });
    if (cfg.outputDirMode === 'custom') { c('Output folder (launching with a different --dir is fatal)'); cmd('/run/outputDir ' + String(cfg.outputDir).trim(), 'outputDir'); }

    blank();
    cmd('/run/initialize', null);

    blank();
    c('---- After /run/initialize ----');
    if (cfg.endTimeMode === 'custom') {
      c('Chemistry end time (overrides the ' + (meso ? '1 s' : '1 us') + ' mode default)');
      cmd('/scheduler/endTime ' + qty(cfg.endTime), 'endTime');
    }
    const zMoved = num(cfg.beamZ.value).value !== undefined && num(cfg.beamZ.value).value !== 0;
    c('Beam: starts at (0, 0, ' + (zMoved ? qty(cfg.beamZ) : '0') + ') along +z');
    cmd('/gun/particle ' + cfg.particle, 'particle');
    cmd('/gun/energy ' + qty(cfg.energy), 'energy');
    if (zMoved) cmd('/gun/position 0 0 ' + qty(cfg.beamZ), 'beamZ');
    if (cfg.killerMode === 'custom' && killerApplies(cfg)) {
      c('Primary-electron energy cuts (code defaults 500 keV / 1 MeV)');
      cmd('/primaryKiller/eLossMin ' + qty(cfg.eLossMin), 'eLossMin');
      cmd('/primaryKiller/eLossMax ' + qty(cfg.eLossMax), 'eLossMax');
    } else if (cfg.killerMode === 'off' && killerApplies(cfg)) {
      c('Primary-electron energy cuts off: the primary is followed until it stops or leaves the box');
      cmd('/primaryKiller/energyCuts false', 'killerMode');
    }

    cfg.runs.forEach(function (r, i) {
      const p = 'runs.' + i + '.';
      blank();
      c('---- Sub-run ' + (i + 1) + ' ----');
      const seeds = String(r.seeds || '').trim().split(/\s+/).filter(Boolean).join(' ');
      if (seeds) cmd('/random/setSeeds ' + seeds, p + 'seeds');
      cmd('/run/beamOn ' + String(r.events).trim(), p + 'events');
      const name = String(r.name || '').trim();
      if (r.dump === 'reset') cmd('/run/dumpDataAndReset' + (name ? ' ' + name : ''), p + 'name');
      else if (r.dump === 'dir') cmd('/run/dumpDataAndResetToDir ' + name, p + 'name');
      else c('No dump here: results keep accumulating into the next dump.');
    });

    const text = lines.map(function (l) { return l.text; }).join('\n') + '\n';
    return { text: text, lines: lines };
  }

  // ---- summary (shared by the preview and the review step) ----
  function summary(cfg) {
    const meso = mesoOn(cfg);
    const events = cfg.runs.reduce(function (a, r) { const n = Number(String(r.events).trim()); return a + (isFinite(n) ? n : 0); }, 0);
    const dumps = cfg.runs.filter(function (r) { return r.dump !== 'none'; }).length;
    return [
      ['Chemistry', cfg.chemistry],
      ['Time-step model', cfg.model + (meso ? ' + mesoscopic stage' : ', no mesoscopic stage')],
      ['End time', describeSeconds(endTimeSeconds(cfg)) + (cfg.endTimeMode === 'custom' ? '' : ' (default)')],
      ['Water box', 'half side ' + qty(cfg.halfBox) + (cfg.o2 ? ', O2 ' + cfg.o2Value.value + ' ' + cfg.o2Value.unit : ', no O2')],
      ['Beam', cfg.particle + ', ' + cfg.energy.value + ' ' + cfg.energy.unit + ', from z = ' + cfg.beamZ.value + ' ' + cfg.beamZ.unit +
        (cfg.particle === 'e-' ? ', energy cuts ' + ({ default: 'default', custom: 'custom', off: 'off' })[cfg.killerMode] : '')],
      ['Runs', cfg.runs.length + ' sub-run' + (cfg.runs.length === 1 ? '' : 's') + ', ' + events + ' event' + (events === 1 ? '' : 's') + ', ' + dumps + ' dump' + (dumps === 1 ? '' : 's')]
    ];
  }

  // A quick smoke-test preset (CLAUDE.md Testing: 10 keV e-, beamOn 2, 1 ms end time).
  function smokePreset(cfg) {
    return applyPatch(cfg, { endTimeMode: 'custom', endTime: { value: '1', unit: 'ms' }, particle: 'e-', energy: { value: '10', unit: 'keV' },
      runs: [{ events: '2', seeds: '', dump: 'reset', name: '' }] });
  }
  function changedCount(cfg) {
    const d = defaults();
    return Object.keys(d).filter(function (k) { return k !== 'filename' && JSON.stringify(d[k]) !== JSON.stringify(cfg[k]); }).length;
  }

  return {
    STEPS: STEPS, FIELDS: FIELDS, FIELD: FIELD, UNITS: UNITS, O2_UNITS: O2_UNITS, CHEMISTRIES: CHEMISTRIES, RULES: RULES,
    groupAvailability: groupAvailability, displayValue: displayValue,
    defaults: defaults, normalize: normalize, applyPatch: applyPatch, get: get, validate: validate, errors: errors,
    generate: generate, summary: summary, sanitizeFilename: sanitizeFilename, fieldAvailability: fieldAvailability,
    optionAvailability: optionAvailability, visible: visible, mesoOn: mesoOn, endTimeSeconds: endTimeSeconds,
    describeSeconds: describeSeconds, smokePreset: smokePreset, changedCount: changedCount, pixelCount: pixelCount
  };
}));
