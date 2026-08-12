/***************************************************************************
 * Compatibility wrapper for the bundled OpenCPN plugin API header.
 *
 * API 1.18 uses fixed-width integer types but the recorded opencpn-libs
 * revision does not include <cstdint> itself.  Include it here so clean
 * checkouts compile on toolchains which do not provide those types through
 * an unrelated transitive include.
 ***************************************************************************/

#ifndef _CELESTIAL_NAVIGATION_OCPN_API_COMPAT_H_
#define _CELESTIAL_NAVIGATION_OCPN_API_COMPAT_H_

#include <cstdint>

#include "ocpn_plugin.h"

#endif
