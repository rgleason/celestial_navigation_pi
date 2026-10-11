#ifndef CELESTIAL_SIGHT_PALETTE_H
#define CELESTIAL_SIGHT_PALETTE_H
#include <array>
#include <wx/colour.h>
#include <wx/intl.h>

// Keep the original named colours alongside the eight colour-vision-friendly
// choices. Persisted ColourName values are a user's labels, not a palette key.
struct SightPaletteEntry {
  const char* name;
  unsigned char red, green, blue;
  wxColour Colour(unsigned char alpha = 255) const {
    return wxColour(red, green, blue, alpha);
  }
};
inline const std::array<SightPaletteEntry, 48>& SightPalette() {
  static const std::array<SightPaletteEntry, 48> colours = {{
      {"Blue", 0, 114, 178}, {"Orange", 230, 159, 0},
      {"Bluish green", 0, 158, 115}, {"Vermilion", 213, 94, 0},
      {"Reddish purple", 204, 121, 167}, {"Sky blue", 86, 180, 233},
      {"Yellow", 240, 228, 66}, {"Black", 0, 0, 0},
      {"Medium Violet Red", 199, 21, 133},
      {"Midnight Blue", 25, 25, 112}, {"Orange (classic)", 255, 165, 0},
      {"Plum", 221, 160, 221}, {"Purple", 128, 0, 128},
      {"Red", 255, 0, 0}, {"Salmon", 250, 128, 114},
      {"Slate Blue", 106, 90, 205}, {"Spring Green", 0, 255, 127},
      {"Orange Red", 255, 69, 0}, {"Orchid", 218, 112, 214},
      {"Pale Green", 152, 251, 152}, {"Pink", 255, 192, 203},
      {"Brown", 165, 42, 42}, {"Blue (classic)", 0, 0, 255},
      {"Green Yellow", 173, 255, 47}, {"Goldenrod", 218, 165, 32},
      {"Blue Violet", 138, 43, 226}, {"Aquamarine", 127, 255, 212},
      {"Cadet Blue", 95, 158, 160}, {"Coral", 255, 127, 80},
      {"Cornflower Blue", 100, 149, 237}, {"Forest Green", 34, 139, 34},
      {"Gold", 255, 215, 0}, {"Thistle", 216, 191, 216},
      {"Turquoise", 64, 224, 208}, {"Violet", 238, 130, 238},
      {"Sea Green", 46, 139, 87}, {"Sky Blue (classic)", 135, 206, 235},
      {"Yellow Green", 154, 205, 50}, {"Indian Red", 205, 92, 92},
      {"Light Blue", 173, 216, 230}, {"Lime Green", 50, 205, 50},
      {"Magenta", 255, 0, 255}, {"Maroon", 128, 0, 0},
      {"Medium Goldenrod", 234, 234, 173},
      {"Medium Orchid", 186, 85, 211},
      {"Medium Sea Green", 60, 179, 113},
      {"Violet Red", 208, 32, 144}, {"Yellow (classic)", 255, 255, 0}}};
  return colours;
}
// A varied automatic cycle; the lighter named colours remain available for
// manual choice, and the chart halo keeps dark lines legible on dark charts.
inline const std::array<size_t, 16>& DefaultSightPaletteIndices() {
  static const std::array<size_t, 16> indices = {{
      8, 9, 17, 29, 16, 12, 33, 28, 30, 25, 14, 27, 44, 45, 46, 7}};
  return indices;
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
