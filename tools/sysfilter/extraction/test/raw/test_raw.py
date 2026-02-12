import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestRaw(unittest.TestCase):

    # Runs once for all tests
    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("raw")
        cls.res_at = cls.runner.run("raw-at")

    def test_has_func(self):
        syscalls = self.res.get_syscalls()
        self.assertIn(514, syscalls)

    def test_has_raw(self):
        syscalls = self.res.get_syscalls()
        self.assertIn(512, syscalls)

    def test_failures(self):
        pass_data = self.res.get_pass()
        failed_funcs = [x["func"] for x in pass_data["failures"]]

        self.assertIn("test_func_fail", failed_funcs)
        self.assertIn("test_raw_fail", failed_funcs)
        self.assertIn("test_fail_arg", failed_funcs)

    # def test_raw_is_at(self):
    #     syscalls = self.res_at.get_syscalls()

    #     pass_data = self.res.get_pass()
    #     failed_funcs = [x["func"] for x in pass_data["failures"]]

    #     self.assertTrue((600 in syscalls) or ("main" in failed_funcs))


if __name__ == "__main__":
    unittest.main()
