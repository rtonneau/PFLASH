/// \file OutputFormats.cc
/// \brief Registry of the output products and the file formats chosen for each.
/// Dependencies: standard library only.
/// Thread safety: see OutputFormats.hh; PreInit writes on the master only.

#include "scoring/OutputFormats.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>

namespace OutputFormats
{
  namespace
  {
    struct ProductInfo
    {
      Product product;
      const char *commandName;
      const char *fileStem;
    };

    constexpr std::size_t kProductCount = 7;

    const std::array<ProductInfo, kProductCount> kProducts = {{
        {Product::Species, "species", "Species"},
        {Product::SpeciesMeso, "speciesMeso", "SpeciesMeso"},
        {Product::Reactions, "reactions", "Reactions"},
        {Product::PhysicsInteractions, "physicsInteractions", "PhysicsInteractions"},
        {Product::TrackLengths, "trackLengths", "TrackLengths"},
        {Product::PreChemical, "preChemical", "PreChemical"},
        {Product::SpeciesMesoSpatial, "speciesMesoSpatial", "SpeciesMesoSpatial"},
    }};

    // A stored selection: formats in canonical order; empty means `none`.
    using Selection = std::vector<Format>;

    struct State
    {
      bool hdf5Available = true;
      Selection defaultFormats{Format::Csv};
      std::array<std::optional<Selection>, kProductCount> own{};
    };

    State &Instance()
    {
      static State state;
      return state;
    }

    std::size_t Index(Product product)
    {
      return static_cast<std::size_t>(product);
    }

    std::string Lower(std::string text)
    {
      std::transform(text.begin(), text.end(), text.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      return text;
    }

    bool FormatFromName(const std::string &name, Format &format)
    {
      for (Format candidate : AllFormats())
      {
        if (FormatName(candidate) == name)
        {
          format = candidate;
          return true;
        }
      }
      return false;
    }

    bool Accepts(Product product, Format format)
    {
      const std::vector<Format> accepted = AcceptedFormats(product);
      return std::find(accepted.begin(), accepted.end(), format) != accepted.end();
    }

    std::string AllFormatsText()
    {
      std::string text;
      for (Format format : AllFormats())
        text += (text.empty() ? "" : " ") + FormatName(format);
      return text;
    }

    // Shared parser. `product` is null for the default list. On success `out` is
    // the selection in canonical order (empty for `none`).
    bool Parse(const Product *product, const std::string &tokens, Selection &out, std::string &err)
    {
      const std::string where = (product != nullptr)
                                    ? "/output/" + CommandName(*product) + "/format"
                                    : std::string("/output/defaultFormat");
      const std::string accepted =
          (product != nullptr) ? AcceptedFormatsText(*product) : AllFormatsText();

      std::istringstream stream(tokens);
      std::vector<std::string> words;
      for (std::string word; stream >> word;)
        words.push_back(Lower(word));

      if (words.empty())
      {
        err = where + ": no format given. Accepted formats: " + accepted + ", or none.";
        return false;
      }

      bool hasNone = false;
      std::array<bool, 4> chosen{};
      for (const std::string &word : words)
      {
        if (word == "none")
        {
          hasNone = true;
          continue;
        }
        Format format{};
        if (!FormatFromName(word, format))
        {
          err = where + ": unknown format '" + word + "'. Accepted formats: " + accepted +
                ", or none.";
          return false;
        }
        if (product != nullptr && !Accepts(*product, format))
        {
          err = where + ": the " + FileStem(*product) + " product does not accept '" + word +
                "'. Accepted formats: " + accepted + ", or none.";
          return false;
        }
        if (format == Format::H5 && !Instance().hdf5Available)
        {
          err = where +
                ": 'h5' needs HDF5, but this build has none. Reconfigure with "
                "-DDNACHEM_WITH_HDF5=ON (HDF5 C++ installed), or choose another format.";
          return false;
        }
        chosen[static_cast<std::size_t>(format)] = true;
      }

      const bool anyFormat = std::find(chosen.begin(), chosen.end(), true) != chosen.end();
      if (hasNone && anyFormat)
      {
        err = where + ": 'none' cannot be combined with another format.";
        return false;
      }

      out.clear();
      for (Format format : AllFormats())
      {
        if (chosen[static_cast<std::size_t>(format)])
          out.push_back(format);
      }
      return true;
    }
  } // namespace

  const std::vector<Product> &AllProducts()
  {
    static const std::vector<Product> products = [] {
      std::vector<Product> list;
      for (const ProductInfo &info : kProducts)
        list.push_back(info.product);
      return list;
    }();
    return products;
  }

  const std::vector<Format> &AllFormats()
  {
    static const std::vector<Format> formats = {Format::Txt, Format::Csv, Format::H5, Format::Root};
    return formats;
  }

  std::string CommandName(Product product)
  {
    return kProducts[Index(product)].commandName;
  }

  std::string FileStem(Product product)
  {
    return kProducts[Index(product)].fileStem;
  }

  std::string FormatName(Format format)
  {
    switch (format)
    {
    case Format::Txt:
      return "txt";
    case Format::Csv:
      return "csv";
    case Format::H5:
      return "h5";
    case Format::Root:
      return "root";
    }
    return "";
  }

  std::string Extension(Format format)
  {
    return FormatName(format);
  }

  std::vector<Format> AcceptedFormats(Product product)
  {
    if (product == Product::SpeciesMesoSpatial)
      return {Format::H5, Format::Root};
    return {Format::Txt, Format::Csv, Format::H5, Format::Root};
  }

  std::string AcceptedFormatsText(Product product)
  {
    std::string text;
    for (Format format : AcceptedFormats(product))
      text += (text.empty() ? "" : " ") + FormatName(format);
    return text;
  }

  void SetHdf5Available(bool available)
  {
    Instance().hdf5Available = available;
  }

  bool SetDefault(const std::string &tokens, std::string &err)
  {
    Selection selection;
    if (!Parse(nullptr, tokens, selection, err))
      return false;
    Instance().defaultFormats = selection;
    return true;
  }

  bool SetProduct(Product product, const std::string &tokens, std::string &err)
  {
    Selection selection;
    if (!Parse(&product, tokens, selection, err))
      return false;
    Instance().own[Index(product)] = selection;
    return true;
  }

  std::vector<Format> Effective(Product product)
  {
    const State &state = Instance();
    const std::optional<Selection> &own = state.own[Index(product)];
    if (own)
      return *own;
    if (product == Product::SpeciesMesoSpatial)
      return {};

    std::vector<Format> result;
    for (Format format : state.defaultFormats)
    {
      if (!Accepts(product, format))
        continue;
      if (format == Format::H5 && !state.hdf5Available)
        continue;
      result.push_back(format);
    }
    return result;
  }

  void ResetForTest()
  {
    Instance() = State();
  }
} // namespace OutputFormats
