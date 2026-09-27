#include "../src/AndroidAngleText.h"
#include <cassert>
#include <iostream>
#include <cmath>
int main() {
  double value = 0;
  const auto valid = [&](const wxString& text, double expected) {
    assert(celestial_android::ParseAngleText(text, &value));
    assert(std::abs(value - expected) < 1e-12);
  };
  valid("30", 30); valid("-77.533415", -77.533415);
  valid("1.234567890123456e-7", 1.234567890123456e-7);
  valid("35 15.1234", 35.252056666666668);
  valid(wxString::FromUTF8("035° 15.123456789′"), 35 + 15.123456789 / 60);
  valid("077 31 18.114 W", -(77 + 31 / 60.0 + 18.114 / 3600.0));
  valid("43.236698 N", 43.236698);
  valid("-58 28.2", -58.47);
  for (const char* text : {"12abc", "", "NaN", "12 60", "12 0 60", "12.5 1", "N 12 S", "12E-", "-12 E"})
    assert(!celestial_android::ParseAngleText(text, &value));
  std::cout << "Independent decimal/DMM/DMS, signs, scientific precision and invalid text passed\n";
}
