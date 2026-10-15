/// \file Tonneau2025Table.hh
/// \brief Table 2 of Tonneau et al., Phys. Med. Biol. 70 (2025) 235021, as plain data
///
/// All 73 reactions of the paper's Table 2 (reactants, products, rate constant,
/// unit, row number R1..R73, source letter A..D). Standard library only: no
/// Geant4, no other dnachem-min header. Copy-paste portable to another project
/// (adjust the include path prefix on copy).
///
/// The table is data, not a Chemistry: a builder (a later ticket) turns it into
/// reaction-table entries and bulk reactions.
///
/// Species names are the paper's, in ASCII: "e-aq", "H.", "OH.", "OH-", "H3O+",
/// "O-", "O2-", "O3-", "HO2." (radical), "HO2-" (anion), "HO3", "O3", "O2",
/// "H2", "H2O2", "H2O". A builder maps them to project names (e-aq -> e_aq,
/// O2- -> O2m, ...). Same-species reactants or products are merged into one
/// term with a count ("OH. + OH." and "2 H." are written count 2).
/// Water is kept as written in the paper (e.g. R23 "H. + H2O"); a builder
/// that treats water as the solvent drops the "H2O" terms.
///
/// Routing: `bulk` is true for R1, R23 and R58..R73, the water pseudo-first-
/// order and acid-base/water equilibrium entries that a builder routes to a
/// bulk-reaction list instead of the particle reaction table. Other
/// first-order entries without water (R53, R55) and R56/R57 are not marked.
///
/// Rate-constant units (`Unit`): SecondOrder = M^-1 s^-1 (the default),
/// FirstOrder = s^-1 (pseudo-first-order in water, or a unimolecular decay),
/// ZeroOrder = M s^-1 (R58 only, water autoionisation).
///
/// Verification: every row was transcribed from the rendered PDF page 7 (journal
/// page 6) images, not from extracted text. The extracted text misaligns the
/// two-line rows around R20..R37 (values shifted by one row, R29..R37 lost
/// them), so only the images were trusted. R20..R37 and every other multi-value
/// row were re-read against the images.
///
/// Remaining uncertainties (the table does not say):
///   - R1 (2 e-aq + 2 H2O) and R24 (2 H.), R34 (2 HO2.), R41 (2 O2- ...),
///     R46 (2 O- ...): the table gives one k per row without saying whether
///     the rate law is k [A]^2 or 2 k [A]^2 (stoichiometric factor 2); the
///     builder must pick the project's like-species convention.
///   - R58 (2 H2O -> H3O+ + OH-, 2.11e-5): the table gives no unit; it is
///     taken as M s^-1 (zero order in the solute, the water concentration
///     being absorbed in k). R59 (1.18e11) is its M^-1 s^-1 reverse.
///   - R60, R62, R64, R57 (k < 1e6 with water on the left) are read as s^-1
///     (water absorbed), as are R23, R66, R69, R71, R73; R53 and R55 as s^-1.
///   - R43: the image shows the HO2- anion (O2- + HO2- -> O- + O2 + OH-),
///     not the HO2 radical.
///   - R56 prints the HO2 reactant without the radical dot; read as HO2.
///     (the radical), consistent with R57/R64/R65.
///   - R57 (O2- + H2O, 0.155) involves water but is not marked `bulk`, as
///     the ticket lists only R1, R23 and R58..R73.
#ifndef Tonneau2025Table_h
#define Tonneau2025Table_h 1

#include <string>
#include <vector>

namespace Tonneau2025Table
{
  /// Unit of a rate constant.
  enum class Unit
  {
    SecondOrder,  ///< M^-1 s^-1
    FirstOrder,   ///< s^-1
    ZeroOrder     ///< M s^-1
  };

  /// One side-term of a reaction: a species and its stoichiometric count.
  struct Term
  {
    std::string species;
    int count = 1;
  };

  /// One row of Table 2.
  struct Reaction
  {
    int id;                      ///< table row number, 1..73
    std::vector<Term> reactants;
    std::vector<Term> products;
    double k;                    ///< rate constant in `unit`
    Unit unit;
    char source;                 ///< 'A' Domnanich and Severin 2022, 'B' Joseph et al 2008,
                                 ///< 'C' Labarbe et al 2020, 'D' Buxton et al 1988
    bool bulk;                   ///< water pseudo-first-order / equilibrium entry: route to bulk
  };

  /// All 73 reactions, in table order (index i holds row i + 1). Built once.
  const std::vector<Reaction>& Reactions();

  /// The row with this id (1..73), or nullptr.
  const Reaction* Find(int id);

  /// "M^-1 s^-1", "s^-1" or "M s^-1".
  const char* UnitLabel(Unit unit);
}  // namespace Tonneau2025Table

#endif  // Tonneau2025Table_h
