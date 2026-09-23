#ifndef CELESTIAL_SIGHT_PALETTE_H
#define CELESTIAL_SIGHT_PALETTE_H
#include <array>
#include <wx/colour.h>
#include <wx/intl.h>

// A small, colour-vision-friendly palette. Identity is also conveyed by the
// sight log and hover labels; colour alone must never identify an observation.
struct SightPaletteEntry {
  const char* name;
  unsigned char red, green, blue;
  wxColour Colour(unsigned char alpha = 255) const {
    return wxColour(red, green, blue, alpha);
  }
};
inline const std::array<SightPaletteEntry, 8>& SightPalette() {
  static const std::array<SightPaletteEntry, 8> colours = {{
      {"Blue", 0, 114, 178}, {"Orange", 230, 159, 0},
      {"Bluish green", 0, 158, 115}, {"Vermilion", 213, 94, 0},
      {"Reddish purple", 204, 121, 167}, {"Sky blue", 86, 180, 233},
      {"Yellow", 240, 228, 66}, {"Black", 0, 0, 0}}};
  return colours;
}
inline int SightPaletteIndex(const wxColour& colour) {
  for (size_t i = 0; i < SightPalette().size(); ++i) {
    const auto& c = SightPalette()[i];
    if (colour.Red() == c.red && colour.Green() == c.green &&
        colour.Blue() == c.blue) return static_cast<int>(i);
  }
  return -1;
}
inline wxString SightColourLabel(const wxColour& colour) {
  const int index = SightPaletteIndex(colour);
  if (index >= 0) return wxGetTranslation(SightPalette()[index].name);
  return wxString::Format(_("Custom (#%02X%02X%02X)"),
      unsigned(colour.Red()), unsigned(colour.Green()), unsigned(colour.Blue()));
}
#endif
