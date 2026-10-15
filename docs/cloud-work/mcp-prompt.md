Read the codebase and get familiar with it. Don't build or install dependencies.

## Context
- This repo, dnachem-min, is a Geant4-DNA application. It depends on a local C++ library called G4Vox, which is not included here. I develop on Windows with Visual Studio.
- Geant4 source: https://github.com/rtonneau/geant4. Clone it shallow (git clone --depth 1) next to this repo, read-only, and don't build it. Focus on:
  - source/processes/electromagnetic/dna/ (physics, chemistry, IRT/SBS, molecules, reactions, scavengers)
  - the DNA-related run/scoring infrastructure
  - examples/extended/medical/dna/ (chem1..chem6, dnaphysics, microdosimetry, etc.)

## Goal
Propose a plan for a general MCP server for Geant4 and Geant4-DNA applications. It is not specific to dnachem-min. Treat dnachem-min as the reference consumer: the way it configures and runs simulations is the pattern to generalize. Plan only, no implementation.

The MCP server has two jobs:

1. Run simulations locally (top priority). The server runs on my Windows machine and lets Claude:
   - configure a run: macro, events/threads, physics list or constructor option, chemistry settings, output paths
   - launch any built Geant4 app, monitor progress, and handle failures or timeouts
   - collect and summarize outputs (scorer results, G-values, logs)
   Infer the generic run contract from how dnachem-min and the official DNA examples are run: entry points, macros, UI commands, CLI args, environment variables, and Geant4 data sets (G4LEDATA, G4DNA data, etc.). Then say how an arbitrary app would plug in, e.g. through a small per-project config file.

2. Scaffold common features. Survey Geant4-DNA and the DNA examples, list the extension points users repeatedly implement, and rank them by how often they recur and how much boilerplate they need. Examples:
   - scorers (G-value, dose, species counts)
   - chemistry setup (reaction tables, molecule definitions, scavengers, time steps)
   - physics constructor choice
   - primary generators, geometry, stepping/tracking actions
   - UI messengers and macros
   For each one, describe the template, the files it touches, and how it is registered. Use dnachem-min's conventions where they are good. Point out where they diverge from upstream Geant4.

## Deliverable
Write docs/mcp-plan.md with:
- a feature catalog from item 2, with source references (file paths in Geant4 and dnachem-min)
- the MCP tool list, with inputs and outputs per tool, split into "run" and "scaffold"
- the per-project config format, plus the dnachem-min instance of it as a worked example
- the language/SDK choice (start with Rust) with reasoning, and where the server should live (its own repo vs inside a project)
- how it finds builds and the Geant4 environment on Windows (and Linux if cheap)
- risks, and open questions for me