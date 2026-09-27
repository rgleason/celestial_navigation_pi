#include "../src/AndroidAngleText.h"
#include <cassert>
#include <iostream>
#include <cmath>
#include <limits>
#include <random>
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
  assert(celestial_android::NumberText(.1) == "0.1");
  assert(celestial_android::NumberText(.2) == "0.2");
  assert(celestial_android::NumberText(46.415) == "46.415");
  assert(celestial_android::NumberText(-77.533415) == "-77.533415");
  assert(celestial_android::NumberText(std::nextafter(1.0, 2.0)) == "1.0000000000000002");
  const auto exact = [&](double expected) {
    const wxString text = celestial_android::NumberText(expected);
    assert(celestial_android::ParseAngleText(text, &value));
    assert(value == expected && std::signbit(value) == std::signbit(expected));
    // Numeric correction fields use wxString's parser rather than angle parsing.
    assert(text.ToDouble(&value));
    assert(value == expected && std::signbit(value) == std::signbit(expected));
  };
  for (double number : {0.0, -0.0, 1.234567891234123, -179.751074,
        std::numeric_limits<double>::min(), std::numeric_limits<double>::max()})
    exact(number);
  std::mt19937 random(2813);
  std::uniform_real_distribution<double> mantissa(-1.0, 1.0);
  std::uniform_int_distribution<int> exponent(-900, 900);
  for (int i = 0; i < 3000; ++i)
    exact(std::ldexp(mantissa(random), exponent(random)));
  std::cout << "Compact Android number display retains exact values and signs through both input parsers\n";
  std::cout << "Independent decimal/DMM/DMS, signs, scientific precision and invalid text passed\n";
}
