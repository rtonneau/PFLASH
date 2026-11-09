# Implementation Plan

**Session:** molecular-finder-degenerate-coordinates
**Date:** 2026-11-08T08:27:03.261Z
**Estimated effort:** 1 day

## Strategy

Preserve baseline, trace construction, correct private kernel, compare to brute force and native contacts, review and commit.

## Tickets Overview

- **Ticket 1:** Diagnose and validate the private molecular finder correction.

## Sequencing Rationale

Baseline precedes edits; independent review follows validation.

## Risks & Mitigation

- **Risk:** Unbounded allocations or false chemical neighbors → **Mitigation:** hard job caps, brute-force oracle and native contacts.

## Assumptions

- Shared Geant4 and previous scratch evidence remain unchanged. No PR or push.
