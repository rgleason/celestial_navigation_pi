// Android output adapter. GPL-3.0-or-later.
#pragma once
#include "NavigationAlgorithms.h"
#include "UtcDateTime.h"

inline wxString AndroidAlmanacCsv(const std::vector<AlmanacRow>& rows) {
  const wxString csv = AlmanacToCsv(rows);
  // Keep the shared column order and numeric serialization. Only the UTC
  // column needs an extension when the entered instant has milliseconds.
  wxString result = csv.BeforeFirst('\n') + "\n";
  wxString remaining = csv.AfterFirst('\n');
  for (const auto& row : rows) {
    const wxString line = remaining.BeforeFirst('\n');
    remaining = remaining.AfterFirst('\n');
    result += UtcDateTime::FormatInstant(row.utc,
        row.utc.GetMillisecond() ? "%Y-%m-%dT%H:%M:%S.%lZ"
                                 : "%Y-%m-%dT%H:%M:%SZ") +
        "," + line.AfterFirst(',') + "\n";
  }
  return result;
}
