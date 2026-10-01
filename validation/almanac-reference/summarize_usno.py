#!/usr/bin/env python3
"""Summarize the offline analytical/DE440s/USNO almanac audit."""
import argparse
import csv
import json
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def value(row, name):
    return float(row[name]) if row[name] else None


def distance(a, b, circular=False):
    if a is None or b is None:
        return None
    delta = (a - b + 180) % 360 - 180 if circular else a - b
    return delta * 60  # arcminutes, signed


def maximum(values):
    values = [abs(x) for x in values if x is not None]
    return max(values) if values else None


def median(values):
    values = [abs(x) for x in values if x is not None]
    return statistics.median(values) if values else None


def fmt(x):
    return "—" if x is None else f"{x:.3f}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("comparison", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("horizons", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--software-commit", default="unknown")
    parser.add_argument("--kernel-sha256", default="unknown")
    args = parser.parse_args()
    with args.comparison.open(newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    manifest = json.loads(args.manifest.read_text())
    with args.horizons.open(newline="") as source:
        horizons = list(csv.DictReader(source, delimiter="\t"))
    bodies = ["Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn",
              "Aries", "Sirius", "Vega", "Arcturus", "Spica", "Antares",
              "Polaris"]
    by_body = defaultdict(lambda: defaultdict(list))
    for row in rows:
        by_body[row["body"]][row["mode"]].append(row)

    def errors(data, reference, predicted, circular=False):
        return [distance(value(row, predicted), value(row, reference), circular)
                for row in data]

    lines = [
        "# 2.9 Almanac accuracy audit — USNO comparison",
        "",
        "Audit date: 22 September 2026. Software: Celestial Navigation 2.9.0.0 "
        "development branch. Document revision: 1 (independent of software version).",
        f"Calculation source commit: `{args.software_commit}`; local DE440s "
        f"SHA-256: `{args.kernel_sha256}`.",
        "",
        "## Findings",
        "",
        "Across this sample, Sun, Venus, the other navigational planets, "
        "Aries and selected stars closely track the official USNO values. "
        "DE440s materially reduces the worst sampled lunar GHA difference "
        "from 0.194′ to 0.121′ at engine precision. The rounded lunar "
        "table still differs from USNO by as much as 0.154′; therefore "
        "this audit does **not** establish exact 0.1′ Nautical Almanac "
        "concordance for every Moon row. Four high-difference lunar "
        "epochs agree with independent JPL Horizons to about 0.001′ "
        "in GHA, supporting the DE440s result at those epochs. No plugin "
        "calculation was changed as part of this audit.",
        "",
        "## Method and coverage",
        "",
        f"- {len(manifest['queries'])} scheduled USNO Celestial Navigation API "
        f"epochs (two whole-second source requests each); {len(rows)//2} "
        "selected body/reference cases, each evaluated "
        "by both engine modes. Dates span 2015, 2024, 2025, 2026 and 2027, "
        "with dates on either side of the 2015 leap second (not the "
        "23:59:60 instant), seasons and dated "
        "IERS prediction coverage. Three locations were used per UTC epoch.",
        "- Reference time is UT1 = plugin UTC + its date-specific DUT1. "
        "In a direct check, USNO's API reported fractional-second text but "
        "computed at the whole "
        "second. Each reference value here interpolates independent calls at "
        "the adjacent whole seconds; GHA and azimuth use circular interpolation. "
        "The original uncorrected run is excluded from the results.",
        "- Source `DE440s` means the runtime actually selected the short kernel. "
        "Mars, Jupiter, Saturn, Aries and stars use the analytical path in "
        "both modes. The table includes the plugin's **actual rounded hourly "
        "Almanac cells** for the Sun, Moon, planets and Aries, as well as raw "
        "engine values. Each row retains its source URLs in the detailed TSV.",
        "- Errors below are absolute angular differences in arcminutes. "
        "GHA and azimuth differences wrap at 360°. A 0.1′ printed table "
        "has up to 0.05′ rounding error even with an identical underlying value.",
        "",
        "## GHA compared with USNO",
        "",
        "| Body | Cases | With kernel | Analytical raw median / max (′) | "
        "Kernel-mode raw median / max (′) | Analytical printed max (′) | "
        "Kernel-mode printed max (′) |",
        "| --- | ---: | --- | ---: | ---: | ---: | ---: |",
    ]
    for body in bodies:
        a = by_body[body]["Analytical"]
        d = by_body[body]["DE440s enabled"]
        if not a or not d:
            continue
        ar = errors(a, "ref_gha", "model_gha", True)
        dr = errors(d, "ref_gha", "model_gha", True)
        ap = errors(a, "ref_gha", "printed_gha", True)
        dp = errors(d, "ref_gha", "printed_gha", True)
        used = Counter(row["engine_source"] for row in d)
        source = ", ".join(f"{key} {n}" for key, n in sorted(used.items()))
        lines.append(f"| {body} | {len(a)} | {source} | "
                     f"{fmt(median(ar))} / {fmt(maximum(ar))} | "
                     f"{fmt(median(dr))} / {fmt(maximum(dr))} | "
                     f"{fmt(maximum(ap))} | {fmt(maximum(dp))} |")
    lines += [
        "",
        "## Declination compared with USNO",
        "",
        "| Body | Cases | Analytical raw median / max (′) | "
        "Kernel-mode raw median / max (′) | Analytical printed max (′) | "
        "Kernel-mode printed max (′) |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for body in bodies:
        if body == "Aries":
            continue
        a = by_body[body]["Analytical"]
        d = by_body[body]["DE440s enabled"]
        if not a or not d:
            continue
        ar = errors(a, "ref_dec", "model_dec")
        dr = errors(d, "ref_dec", "model_dec")
        ap = errors(a, "ref_dec", "printed_dec")
        dp = errors(d, "ref_dec", "printed_dec")
        lines.append(f"| {body} | {len(a)} | "
                     f"{fmt(median(ar))} / {fmt(maximum(ar))} | "
                     f"{fmt(median(dr))} / {fmt(maximum(dr))} | "
                     f"{fmt(maximum(ap))} | {fmt(maximum(dp))} |")

    lines += [
        "",
        "## Agreement at the printed 0.1′ rounding step",
        "",
        "A cell counts as a match when its difference from the USNO "
        "unrounded reference is at most 0.05′ (one half-step). These "
        "percentages describe agreement with **USNO's model**, not a "
        "pass/fail judgement about JPL's Moon position.",
        "",
        "| Body | GHA analytical / kernel mode | "
        "Declination analytical / kernel mode |",
        "| --- | ---: | ---: |",
    ]
    for body in ["Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn", "Aries"]:
        percentages = []
        for quantity in ["gha", "dec"]:
            for mode in ["Analytical", "DE440s enabled"]:
                cells = by_body[body][mode]
                differences = errors(cells, "ref_" + quantity,
                                     "printed_" + quantity, quantity == "gha")
                valid = [x for x in differences if x is not None]
                percentages.append(f"{sum(abs(x) <= .050001 for x in valid) / len(valid) * 100:.0f}%"
                                   if valid else "—")
        lines.append(f"| {body} | {percentages[0]} / {percentages[1]} | "
                     f"{percentages[2]} / {percentages[3]} |")

    aries_at = {(r["utc"], r["latitude"], r["longitude"], r["mode"]): r
                for r in rows if r["body"] == "Aries"}
    lines += [
        "",
        "## Printed star SHA at its 00:00 UTC table epoch",
        "",
        "Reference SHA is USNO star GHA minus USNO Aries GHA at the same "
        "UT1. Only visible stars returned by USNO at the table epoch qualify.",
        "",
        "| Star | Compared cells | Maximum printed SHA difference (′) |",
        "| --- | ---: | ---: |",
    ]
    for body in ["Sirius", "Vega", "Arcturus", "Spica", "Antares"]:
        errors_sha = []
        for row in by_body[body]["DE440s enabled"]:
            if not row["printed_sha"]:
                continue
            aries = aries_at.get((row["utc"], row["latitude"],
                                  row["longitude"], row["mode"]))
            if aries is None:
                continue
            pieces = row["printed_sha"].replace("'", "").split()
            printed = float(pieces[0]) + float(pieces[1]) / 60
            reference = (value(row, "ref_gha") - value(aries, "ref_gha")) % 360
            errors_sha.append(distance(printed, reference, True))
        lines.append(f"| {body} | {len(errors_sha)} | {fmt(maximum(errors_sha))} |")

    lines += [
        "",
        "## Computed altitude and azimuth compared with USNO",
        "",
        "| Body | Hc max analytical / kernel mode (′) | "
        "Zn max analytical / kernel mode (°) |",
        "| --- | ---: | ---: |",
    ]
    for body in bodies:
        if body == "Aries":
            continue
        a = by_body[body]["Analytical"]
        d = by_body[body]["DE440s enabled"]
        if not a or not d:
            continue
        ah = maximum(errors(a, "ref_hc", "model_hc"))
        dh = maximum(errors(d, "ref_hc", "model_hc"))
        az = maximum(errors(a, "ref_zn", "model_zn", True))
        dz = maximum(errors(d, "ref_zn", "model_zn", True))
        lines.append(f"| {body} | {fmt(ah)} / {fmt(dh)} | "
                     f"{fmt(az/60 if az is not None else None)} / "
                     f"{fmt(dz/60 if dz is not None else None)} |")

    lines += [
        "",
        "## Representative printed GHA entries",
        "",
        "| UTC | Body | USNO (°) | Analytical printed (°) | "
        "DE440s-mode printed (°) | Δ analytical (′) | Δ kernel mode (′) |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for body in ["Sun", "Moon", "Venus", "Mars", "Jupiter", "Saturn", "Aries"]:
        candidates = [r for r in by_body[body]["Analytical"]
                      if r["utc"].startswith("2024") and r["printed_gha"]
                      and r["latitude"] == "0"]
        if body == "Moon":
            candidates = [r for r in by_body[body]["Analytical"]
                          if r["printed_gha"]]
        if not candidates:
            candidates = [r for r in by_body[body]["Analytical"]
                          if r["printed_gha"]]
        if not candidates:
            continue
        if body == "Moon":
            def moon_kernel_error(row):
                kernel = next(r for r in by_body[body]["DE440s enabled"]
                              if (r["utc"], r["latitude"], r["longitude"]) ==
                                 (row["utc"], row["latitude"], row["longitude"]))
                return abs(distance(value(kernel, "printed_gha"),
                                    value(row, "ref_gha"), True))
            a = max(candidates, key=moon_kernel_error)
        else:
            a = candidates[len(candidates)//2]
        d = next(r for r in by_body[body]["DE440s enabled"]
                 if (r["utc"], r["latitude"], r["longitude"]) ==
                    (a["utc"], a["latitude"], a["longitude"]))
        ref = value(a, "ref_gha")
        lines.append(f"| {a['utc']} | {body} | {ref:.6f} | "
                     f"{value(a, 'printed_gha'):.6f} | "
                     f"{value(d, 'printed_gha'):.6f} | "
                     f"{fmt(distance(value(a,'printed_gha'), ref, True))} | "
                     f"{fmt(distance(value(d,'printed_gha'), ref, True))} |")

    lines += [
        "",
        "## Independent JPL Horizons checks of the lunar offset",
        "",
        "Horizons supplies geocentric apparent right ascension and "
        "declination. For these checks, GHA is USNO's well-matched GHA "
        "of Aries minus Horizons apparent RA. Horizons RA is printed to "
        "0.00001°, limiting this comparison to about 0.001′.",
        "",
        "| UTC | USNO Moon GHA (°) | Horizons-derived GHA (°) | "
        "Plugin DE440s GHA (°) | Plugin–Horizons (′) | "
        "Plugin–USNO (′) | Dec plugin–Horizons (′) |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for h in horizons:
        utc = h["utc"]
        moon = next(row for row in by_body["Moon"]["DE440s enabled"]
                    if row["utc"] == utc)
        aries = next(row for row in by_body["Aries"]["DE440s enabled"]
                     if row["utc"] == utc)
        jpl_gha = (value(aries, "ref_gha") - float(h["apparent_ra_deg"])) % 360
        plugin_gha = value(moon, "model_gha")
        lines.append(f"| {utc} | {value(moon, 'ref_gha'):.6f} | "
                     f"{jpl_gha:.6f} | {plugin_gha:.6f} | "
                     f"{fmt(distance(plugin_gha, jpl_gha, True))} | "
                     f"{fmt(distance(plugin_gha, value(moon, 'ref_gha'), True))} | "
                     f"{fmt(distance(value(moon, 'model_dec'), float(h['apparent_dec_deg'])))} |")
    lines += [
        "",
        "At these four epochs, the lunar DE440s path follows Horizons "
        "much more closely than the USNO service. This supports the lunar "
        "ephemeris calculation; it does not establish why the services "
        "differ or guarantee identical Nautical Almanac printed digits.",
    ]

    lines += [
        "",
        "## Interpretation and limits",
        "",
        "The reference is the USNO API output, not an assertion of an "
        "absolute 'true' angle. USNO cautions that the numerical precision "
        "returned by its API may exceed the accuracy of its displayed values. "
        "The short DE440s kernel is also not an independent reference for "
        "its own results; JPL Horizons provides the separate selected-case "
        "planet-centre check in `test/navigation_de440_tests.cpp`.",
        "",
        "The USNO Moon SD altitude correction is observer dependent and "
        "includes augmentation; the Almanac prints geocentric SD. Those "
        "columns must not be subtracted directly. USNO applies a "
        "centre-of-light phase convention for Venus. Printed star SHA is "
        "tabulated at the day's reference epoch, so the report compares "
        "star GHA/declination from the engine at the observation time and "
        "printed SHA at the matching midnight epoch separately.",
        "",
        "The Sun, navigational planets, Aries and sampled stars agree "
        "closely with USNO. The Moon is the exception: kernel-mode raw GHA "
        "reaches 0.121′ and the rounded table reaches 0.154′ from USNO. "
        "The analytical maximum is larger. At 2027-08-01 08:00 UTC, "
        "the printed DE440s GHA differs from USNO by 0.154′ versus 0.054′ "
        "for the analytical path, while the raw DE440s value matches "
        "Horizons within 0.001′. The four independent Horizons "
        "checks above favour the plugin's DE440s result at those epochs. "
        "A result outside 0.1′ can reflect reference-model differences "
        "or a defect and should be investigated by body, epoch and "
        "quantity; it is not silently accepted. This suite samples "
        "reference-visible bodies rather than every hourly row of a year. "
        "Air Almanac PDFs provide a separate printed cross-check, but "
        "their 1′ resolution cannot certify this plugin's 0.1′ tables. "
        "This audit did not systematically compare every printed HP/SD, "
        "v/d or altitude-correction table entry with an external source; "
        "the existing focused tests cover selected reductions and the "
        "geocentric lunar SD definition.",
        "",
        "## Data and official references",
        "",
        "- [USNO API documentation](https://aa.usno.navy.mil/data/api)",
        "- [USNO celestial-navigation data conventions]"
        "(https://aa.usno.navy.mil/data/celnav)",
        "- [JPL Horizons documentation]"
        "(https://ssd.jpl.nasa.gov/horizons/manual.html)",
        "- [USNO Air Almanac PDFs](https://aa.usno.navy.mil/publications/aira)",
        "- `schedule.tsv`: UTC, UT1, DUT1, location and forecast quality.",
        "- `reference.tsv`: USNO interpolated reference values and URLs.",
        "- `comparison.tsv`: every body in both modes, raw and printed values.",
        "- `manifest.json`: exact source requests and USNO API version.",
        "- `horizons-moon.tsv`: four JPL rows and exact query URLs.",
        "",
    ]
    args.report.write_text("\n".join(lines))
    print(f"Wrote {args.report}, {len(rows)} comparison rows")


if __name__ == "__main__":
    main()
