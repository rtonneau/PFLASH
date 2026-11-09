// ExamplePhysicsTag: the physics/chemistry identity written into every
// PrecursorPacket (compatibilityTag) by `capture` and required by every reader.
//
// Geant4-free on purpose, so library checkers and tests can use it without a
// kernel. ExamplePhysicsList.cc static_asserts that it is built against the
// Geant4 version named here.
//
// Change the tag whenever anything that changes the captured precursor states
// changes: Geant4 version, EM-DNA physics constructor, chemistry constructor or
// time-step model, the water medium, or the capture age. Packets with another
// tag are then rejected instead of being mixed silently.
#pragma once

namespace ptirt
{

// One token (no whitespace), see PrecursorPacket.hh.
inline constexpr const char kPacketCompatibilityTag[] =
    "ptirt1_geant4-11.4.3_G4EmDNAPhysics_option2_G4EmDNAChemistry_option3_IRT_syn_pureWater_age1ps";

// Geant4-DNA puts every pre-chemical species (excited/ionised H2O, e_aq) at
// global time 1 ps, whatever the time of its creating interaction (no pulse
// information: G4DNAChemistryManager::CreateWaterMolecule and
// CreateSolvatedElectron use `picosecond + 0`). `capture` reads the molecules at that time, before
// the chemistry stage runs. In ns, the unit used by PrecursorPacket.
inline constexpr double kPrecursorAgeNs = 1.0e-3;

}  // namespace ptirt
