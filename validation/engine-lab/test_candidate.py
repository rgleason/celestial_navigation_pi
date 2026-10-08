"""Offline experiment controls, reproducibility and component tests."""
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from apply_candidate import apply
from run import LAB, differences, invoke, verify_manifest


class CandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.work = LAB/".work"
        cls.binary = cls.work/"build-candidate/lunar-lab"
        cls.kernel = LAB.parents[1]/"eclipse/data/de440s.bsp"
        cls.case = {"id":"candidate-controls","utc":"2024-06-13T19:26:00Z","position":[41.3666666667,-71.4833333333]}

    def test_dut1_only_rotates_geographic_longitude(self):
        base = invoke(self.binary,self.kernel,self.case,"ephemeris",{})
        adjusted = invoke(self.binary,self.kernel,self.case,"ephemeris",{"dut1":0.5})
        for key in base:
            if key not in ("moon_gp_lon","sun_gp_lon"):
                self.assertAlmostEqual(base[key],adjusted[key],delta=1e-10)
        for body in ("moon","sun"):
            self.assertAlmostEqual((adjusted[body+"_gp_lon"]-base[body+"_gp_lon"])*3600,
                                   -15.041067*0.5,delta=0.001)

    def test_observer_model_leaves_geocentric_ephemeris_unchanged(self):
        base = invoke(self.binary,self.kernel,self.case,"ephemeris",{"dut1":-0.0172})
        adjusted = invoke(self.binary,self.kernel,self.case,"ephemeris",
                          {"dut1":-0.0172,"observer_astrometry":1})
        self.assertEqual(base,adjusted)

    def test_full_model_has_no_timezone_dependency(self):
        params = {"dut1":-0.0172,"observer_astrometry":1,"offset":0.123}
        expected = invoke(self.binary,self.kernel,self.case,"reference",params)
        for zone in ("Europe/London","Pacific/Honolulu","Asia/Kolkata"):
            self.assertEqual(expected,invoke(self.binary,self.kernel,self.case,"reference",params,timezone=zone))

    def test_equivalent_fractional_epoch_across_midnight(self):
        first = {**self.case,"utc":"2024-06-30T23:59:59.500Z"}
        second = {**self.case,"utc":"2024-07-01T00:00:00.623Z"}
        params = {"dut1":0.1,"observer_astrometry":1}
        self.assertEqual(invoke(self.binary,self.kernel,first,"reference",{**params,"offset":1.123}),
                         invoke(self.binary,self.kernel,second,"reference",params))

    def test_invalid_or_double_corrections_fail_explicitly(self):
        for params in ({"dut1":1},{"dut1":-1},{"observer_astrometry":1},
                       {"observer_astrometry":2},{"diurnal_aberration":-1},
                       {"observer_astrometry":1,"diurnal_aberration":1},
                       {"observer_astrometry":1,"ellipsoid":0}):
            with self.subTest(params=params), self.assertRaises(RuntimeError):
                invoke(self.binary,self.kernel,self.case,"reference",params)

    def test_saved_patch_reproduces_candidate_without_touching_baseline(self):
        with tempfile.TemporaryDirectory(prefix="reproduce-",dir=self.work) as temp:
            candidate = Path(temp)/"candidate"
            shutil.copytree(self.work/"baseline",candidate)
            apply(candidate)
            apply(candidate)  # idempotent, no overwrite
            manifest = json.loads((LAB/"experiments/observer-astrometry.json").read_text())
            for path in manifest["files"]:
                self.assertEqual((candidate/path).read_bytes(),(self.work/"candidate"/path).read_bytes())
            (candidate/"driver/runner.cpp").write_text("unrelated experiment\n")
            with self.assertRaisesRegex(ValueError,"other changes"):
                apply(candidate)
        verify_manifest(self.work/"baseline",self.work/"baseline.sha256")

    def test_analytic_aberration_and_observer_callback(self):
        with tempfile.TemporaryDirectory(prefix="components-",dir=self.work) as temp:
            binary = Path(temp)/"components"
            subprocess.run(["c++","-std=c++17","-O2",str(LAB/"candidate_component_tests.cpp"),
                            str(self.work/"candidate/engine/src/LunarDistanceEngine.cpp"),
                            "-I"+str(self.work/"candidate/engine/src"),"-o",str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=15)


if __name__ == "__main__":
    unittest.main()
