# Ticket 03: per-chemistry-dissociation-hook

**Model:** opus
**Effort:** high

**Acceptance Criteria:**
- [ ] Chemistry gets an optional dissociation-channel builder; unset means `G4ChemDissociationChannels_option1`, so every existing Chemistry is unchanged.
- [ ] A `Farokhi2023` channel set reproduces the stock example's `EmDNAChemistry::ConstructDissociationChannels` (B1A1 0.30/0.15/0.55 and the rest as in the example).
- [ ] `DnaChemistryList::ConstructDissociationChannels` uses the selected Chemistry's builder.
- [ ] A unit test covers the registry default and the override.
- [ ] A new ADR records the hook, next to ADR 0007.

**Files to Touch:**
- `header/chemistry/ChemistryTypes.hh`
- `src/chemistry/DnaChemistryList.cc`
- `src/chemistry/catalog/Farokhi2023Reactions.cc`
- `src/chemistry/ChemistryRegistry.cc`
- `test/ChemistryRegistryTest.cc`
- `docs/adr/0009-per-chemistry-dissociation-channels.md`

**Verification Step:**

Run:
```bash
cd build-ninja && ctest -R ChemistryRegistry --output-on-failure
```

Expected:
The test passes, including the new default and override cases.

**Notes:**

Read ADR 0007 and its hook first and follow its shape. Check the example's channel list in full, including whether it registers dissociative-attachment and vibrational-decay channels, before copying it. Keep the new builder portable and project-agnostic like `PureWaterReactions.cc`.
