"""Test the oracle and failure reporting independently of engine success."""
import copy
import json
import shutil
import tempfile
from pathlib import Path
import unittest
from accuracy import assess_inverse, inverse_eligibility, verify_accuracy_references, verify_raytrace_table, verify_epoch, verify_dut1_references
from accuracy_reference import decode, expected_sight, separation, utc_shift
from run import LAB


class AccuracyTests(unittest.TestCase):
    def setUp(self):
        self.spec = json.loads((LAB/"accuracy-grid.json").read_text())
        self.directory = LAB/"references/accuracy-20260909"

    def test_angular_oracle_special_cases(self):
        self.assertAlmostEqual(separation(0,0,90,0),90)
        self.assertAlmostEqual(separation(0,0,180,0),180)
        self.assertAlmostEqual(separation(359.999,0,0.001,0),0.002)
        self.assertAlmostEqual(separation(0,90,90,90),0)

    def test_independent_limb_contact_conventions(self):
        moon = {"ra":0,"dec":0,"alt":30,"diameter":1800}
        sun = {"ra":60,"dec":0,"alt":60,"diameter":1800}
        near = expected_sight(moon,sun,0,0,0)
        far = expected_sight(moon,sun,2,2,2)
        self.assertAlmostEqual(near["distance"],59.5)
        self.assertAlmostEqual(far["distance"],60.5)
        self.assertAlmostEqual(near["moon_alt"],59.5)
        self.assertAlmostEqual(near["sun_alt"],120.5)
        self.assertAlmostEqual(far["moon_alt"],60.5)

    def test_watch_shift_is_label_only(self):
        self.assertEqual(utc_shift("2024-06-13T19:26:00Z",-23),"2024-06-13T19:25:37Z")

    def test_reference_epoch_requires_exact_seconds(self):
        verify_epoch("2024-Jun-13 19:26", "2024-06-13T19:26:00Z")
        verify_epoch("2016-Dec-31 23:59:59", "2016-12-31T23:59:59Z")
        with self.assertRaisesRegex(ValueError, "epoch mismatch"):
            verify_epoch("2016-Dec-31 23:59", "2016-12-31T23:59:59Z")

    def test_reference_set_validates(self):
        result = verify_accuracy_references(self.directory,self.spec)
        self.assertEqual(len(result),25)

    def test_dut1_input_is_bound_to_dated_raw_response(self):
        result = verify_dut1_references(LAB/"references/dut1-20260909",self.spec)
        self.assertAlmostEqual(result["after-2016-leap"]-result["before-2016-leap"],1)
        with tempfile.TemporaryDirectory(prefix="celestial-dut1-test-") as temp:
            directory = Path(temp)/"reference"
            shutil.copytree(LAB/"references/dut1-20260909",directory)
            manifest = json.loads((directory/"manifest.json").read_text())
            manifest["epochs"]["2017-01-01T00:00:00Z"]["dut1_seconds"] -= 0.001
            (directory/"manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,"DUT1 value"):
                verify_dut1_references(directory,self.spec)

    def test_tampered_normalized_value_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="celestial-accuracy-test-") as temp:
            directory = Path(temp)/"reference"
            shutil.copytree(self.directory,directory)
            manifest = json.loads((directory/"manifest.json").read_text())
            key = manifest["cases"][0]["responses"]["moon_topocentric"]
            manifest["responses"][key]["decoded"]["alt"] += 0.01
            (directory/"manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,"Expected values changed"):
                verify_accuracy_references(directory,self.spec)

    def test_wrong_epoch_request_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="celestial-accuracy-test-") as temp:
            directory = Path(temp)/"reference"
            shutil.copytree(self.directory,directory)
            manifest = json.loads((directory/"manifest.json").read_text())
            key = manifest["cases"][0]["responses"]["moon_topocentric"]
            manifest["responses"][key]["parameters"]["START_TIME"] = "'1973-07-01 12:00:00'"
            (directory/"manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,"query mismatch"):
                verify_accuracy_references(directory,self.spec)

    def test_airless_and_refracted_headers(self):
        manifest = json.loads((self.directory/"manifest.json").read_text())
        case = manifest["cases"][0]
        for mode in ("topocentric","refracted"):
            meta = manifest["responses"][case["responses"]["sun_"+mode]]
            value = decode(json.loads((self.directory/meta["file"]).read_text()), mode=="refracted")
            self.assertTrue({"ra","dec","alt","az","diameter"} <= value.keys())

    def test_published_raytrace_values_bound_to_source(self):
        source = LAB/".work/baseline/engine/eclipse/third_party/erfa/src/refco.c"
        verify_raytrace_table(self.spec,source)
        changed = copy.deepcopy(self.spec)
        changed["raytrace_refraction"]["observed_zenith_deg_and_refraction_arcsec"][0][1] += 1
        with self.assertRaisesRegex(ValueError,"Raytrace table"):
            verify_raytrace_table(changed,source)

    def test_bad_geometry_is_explicitly_excluded(self):
        moon = {"ra":0,"dec":0,"alt":30,"az":90}
        sun = {"ra":60,"dec":0,"alt":60,"az":90}
        self.assertIn("weak altitude-circle crossing geometry",inverse_eligibility(moon,sun,self.spec["policy"]))

    def test_inverse_does_not_choose_different_branches_for_position_and_time(self):
        entry = {"outputs":{"baseline":{"candidate_count":2,
            "candidate_0_position_count":1,"candidate_0_0_lat":10,"candidate_0_0_lon":20,"candidate_0_seconds":25,
            "candidate_1_position_count":1,"candidate_1_0_lat":-40,"candidate_1_0_lon":-100,"candidate_1_seconds":23}}}
        values = []
        def check(entry,name,metric,actual,expected,tolerance,units):
            values.append((metric,abs(actual-expected)<=tolerance))
        assess_inverse(entry,{"position":[10,20]},check,self.spec["policy"])
        self.assertEqual(values,[("position_recovery",True),("clock_recovery",False)])


if __name__ == "__main__":
    unittest.main()
