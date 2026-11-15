// capture-check: read a packet library back and validate it (Geant4-free).
//
//   capture-check <dir> [--expect-count <N>] [--box-half-side-nm <h>]
//
// Loads <dir>/library.csv and every packet with the example's compatibility
// tag (LoadPacketLibrary: format, tag, bounds and the per-packet energy
// balance deposit + escaped = primary within tolerance, index/packet
// agreement), then requires:
//   - the expected packet count (when given);
//   - at least one molecule per packet;
//   - every molecule strictly inside the capture box (|x|, |y|, |z| < h, box
//     centre = origin), h = --box-half-side-nm, else each packet's own
//     captureHalfSide_nm; with the option, each packet must also record h
//     (relative tolerance 1e-9);
//   - a positive mean energy deposit per packet in the box.
// Prints one line per packet, then "CAPTURE_CHECK_OK ... meanBoxDepositEv=..."
// and exits 0, or prints "CAPTURE_CHECK_ERROR <reason>" and exits 1.
#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "CaptureLibrary.hh"
#include "DrsPhysicsTag.hh"
#include "PrecursorPacket.hh"

namespace
{

double ParsePositive(const std::string& option, const std::string& text)
{
  std::size_t used = 0;
  double value = 0;
  try
  {
    value = std::stod(text, &used);
  }
  catch (const std::exception&)
  {
    used = 0;
  }
  if (used != text.size() || !(value > 0) || !std::isfinite(value))
    throw std::runtime_error(option + " needs a positive number");
  return value;
}

int Run(int argc, char** argv)
{
  if (argc < 2 || argc % 2 != 0)
    throw std::runtime_error("usage: capture-check <dir> [--expect-count <N>] [--box-half-side-nm <h>]");
  const std::filesystem::path directory = argv[1];
  long long expectedCount = -1;
  double boxHalfSideNm = 0;  // 0 = use each packet's captureHalfSide_nm
  for (int i = 2; i < argc; i += 2)
  {
    const std::string option = argv[i];
    const std::string value = argv[i + 1];
    if (option == "--expect-count")
    {
      std::size_t used = 0;
      expectedCount = std::stoll(value, &used, 10);
      if (used != value.size() || expectedCount < 1)
        throw std::runtime_error("--expect-count needs a positive integer");
    }
    else if (option == "--box-half-side-nm")
    {
      boxHalfSideNm = ParsePositive(option, value);
    }
    else
    {
      throw std::runtime_error("unknown option " + option);
    }
  }

  const drs::PacketLimits limits;
  const auto library = drs::LoadPacketLibrary(directory, drs::kPacketCompatibilityTag, limits);
  if (expectedCount > 0 && library.packets.size() != static_cast<std::size_t>(expectedCount))
    throw std::runtime_error("expected " + std::to_string(expectedCount) + " packets, found " +
                             std::to_string(library.packets.size()));
  if (library.packets.empty()) throw std::runtime_error("the library holds no packet");

  double maxAbsCoordinateNm = 0;
  for (std::size_t i = 0; i < library.packets.size(); ++i)
  {
    const auto& packet = library.packets[i];
    const auto& entry = library.entries[i];
    if (packet.molecules.empty()) throw std::runtime_error(entry.file + " holds no molecule");
    // Restate the energy ledger explicitly (Read already enforces it).
    const double residual = packet.energyDepositEv + packet.escapedEnergyEv - packet.primaryEnergyEv;
    const double bound = limits.energyAbsTolEv + limits.energyRelTol * packet.primaryEnergyEv;
    if (!(std::abs(residual) <= bound))
      throw std::runtime_error(entry.file + ": energy ledger does not balance");

    const double h = boxHalfSideNm > 0 ? boxHalfSideNm : packet.captureHalfSideNm;
    // Relative tolerance: the packet stores the Geant4 length / nm (2 um / nm
    // is 1999.9999999999998).
    if (boxHalfSideNm > 0 && !(std::abs(packet.captureHalfSideNm - boxHalfSideNm) <= 1e-9 * boxHalfSideNm))
      throw std::runtime_error(entry.file + ": captureHalfSide_nm is not the expected box half side");
    for (const auto& molecule : packet.molecules)
      for (double x : molecule.positionNm)
      {
        if (!(std::abs(x) < h))
          throw std::runtime_error(entry.file + ": a molecule lies outside the capture box");
        maxAbsCoordinateNm = std::max(maxAbsCoordinateNm, std::abs(x));
      }

    std::cout << "packet " << entry.packetIndex << " " << entry.file << " seed=" << entry.physicsSeed
              << " molecules=" << packet.molecules.size() << " primaryEv=" << packet.primaryEnergyEv
              << " depositEv=" << packet.energyDepositEv << " escapedEv=" << packet.escapedEnergyEv
              << " residualEv=" << residual << " boundEv=" << bound << '\n';
  }

  const auto totals = drs::SumLibrary(library.entries);
  const double meanBoxDepositEv = totals.energyDepositEv / static_cast<double>(totals.packetCount);
  if (!(meanBoxDepositEv > 0)) throw std::runtime_error("the mean energy deposit in the box is not positive");
  std::ostringstream ok;
  ok.precision(17);
  ok << "CAPTURE_CHECK_OK packets=" << totals.packetCount << " molecules=" << totals.moleculeCount
     << " maxAbsResidualEv=" << totals.maxAbsResidualEv << " maxAbsCoordinateNm=" << maxAbsCoordinateNm
     << " meanBoxDepositEv=" << meanBoxDepositEv;
  std::cout << ok.str() << std::endl;
  return 0;
}

}  // namespace

int main(int argc, char** argv)
{
  try
  {
    return Run(argc, argv);
  }
  catch (const std::exception& error)
  {
    std::cerr << "CAPTURE_CHECK_ERROR " << error.what() << std::endl;
    return 1;
  }
}
