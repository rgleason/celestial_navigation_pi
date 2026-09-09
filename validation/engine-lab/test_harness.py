"""Tests of the comparison machinery, including intentional bad inputs."""
import copy
import json
import unittest
from pathlib import Path
import tempfile
from run import LAB, differences, dm, seconds, session_rows, verify_manifest, verify_references


class HarnessTests(unittest.TestCase):
    def setUp(self):
        self.corpus = json.loads((LAB / "corpus.json").read_text())

    def test_comparison_detects_numeric_change(self):
        self.assertEqual(differences({"x": 1}, {"x": 1}), {})
        self.assertEqual(differences({"x": 1}, {"x": 2})["x"]["delta"], 1)

    def test_comparison_detects_missing_fields(self):
        self.assertIn("x", differences({"x": 1}, {}))

    def test_unavailable_values_keep_their_status(self):
        self.assertEqual(differences({"sigma": "inf"}, {"sigma": "inf"}), {})
        self.assertIn("sigma", differences({"sigma": "inf"}, {"sigma": 1}))

    def test_degree_minutes_not_decimal_minutes(self):
        self.assertAlmostEqual(dm("85:40.3"), 85 + 40.3 / 60)
        with self.assertRaises(ValueError):
            dm("85:60")

    def test_watch_intervals_cross_midnight(self):
        self.assertEqual(seconds("2024-07-01T00:00:01Z", "2024-06-30T23:59:59Z"), 2)

    def test_session_preserves_raw_readings_and_unique_ids(self):
        point = self.corpus["observations"][0]
        original = copy.deepcopy(point)
        rows = [line.split() for line in session_rows(point).splitlines()]
        self.assertEqual(len(rows), 8)
        self.assertEqual(len({ident for row in rows for ident in row[6:]}), 8)
        self.assertEqual([float(rows[0][i]) for i in (0, 4, 5)], [-90, -210, -150])
        self.assertEqual([float(rows[1][i]) for i in (0, 4, 5)], [-90, 390, 330])
        self.assertEqual(point, original)

    def test_printed_average_is_not_replaced_by_arithmetic_mean(self):
        point = self.corpus["observations"][0]
        sun = [dm(r["angle_dm"]) for r in point["readings"] if r["kind"] == "sun_altitude"]
        self.assertAlmostEqual((sum(sun)/2 - dm(point["published_reduced_example"]["sun_alt_dm"])) * 60, 0.5)

    def test_reference_values_are_bound_to_raw_response(self):
        directory = LAB / "references/horizons-20260909"
        manifest = json.loads((directory / "manifest.json").read_text())
        verify_references(directory, manifest, self.corpus)
        manifest["cases"][0]["expected"]["moon_alt_deg"] += 0.01
        with self.assertRaisesRegex(ValueError, "Expected values"):
            verify_references(directory, manifest, self.corpus)

    def test_empty_reference_grid_is_rejected(self):
        directory = LAB / "references/horizons-20260909"
        manifest = json.loads((directory / "manifest.json").read_text())
        manifest["cases"] = []
        with self.assertRaisesRegex(ValueError, "Incomplete reference"):
            verify_references(directory, manifest, self.corpus)

    def test_modified_baseline_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="celestial-lab-test-") as temporary:
            root = Path(temporary)
            (root / "engine").mkdir()
            (root / "driver").mkdir()
            # Deliberately wrong checksum, not an engine source alteration.
            (root / "engine/example").write_text("altered")
            manifest = root / "manifest"
            manifest.write_text("0" * 64 + "  engine/example\n")
            with self.assertRaisesRegex(ValueError, "Frozen baseline modified"):
                verify_manifest(root, manifest)


if __name__ == "__main__":
    unittest.main()
