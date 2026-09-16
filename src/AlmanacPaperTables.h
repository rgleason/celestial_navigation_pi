/******************************************************************************
 * Calculator-free paper tables generated from public mathematical methods.
 ******************************************************************************/

#ifndef CELESTIAL_NAVIGATION_ALMANAC_PAPER_TABLES_H
#define CELESTIAL_NAVIGATION_ALMANAC_PAPER_TABLES_H

#include "AlmanacGenerator.h"

struct AgetonReductionResult {
  bool valid = false;
  double computedAltitude = 0.0;
  double azimuthTrue = 0.0;
  wxString error;
};

class AlmanacPaperTables {
public:
  static unsigned IncrementPageCount();
  static unsigned ReductionPageCount();
  static unsigned AltitudeCorrectionPageCount();
  static unsigned DirectReductionPageCount(const AlmanacRequest& request);
  static unsigned InstructionPageCount();
  static unsigned PageCount(const AlmanacRequest& request);
  static void Append(const AlmanacRequest& request, AlmanacDocument* document);

  // Exposed for independent numerical regression tests of the printed table.
  static long AgetonA(double degrees);
  static long AgetonB(double degrees);
  static AgetonReductionResult ReduceAgeton(double latitude,
                                             double declination,
                                             double localHourAngle,
                                             bool quantizeToPrintedTable = true);
};

#endif
