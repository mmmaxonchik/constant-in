import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestDl(unittest.TestCase):
    def setUp(self):
        self.runner = TestRunner(os.path.dirname(__file__))

    def do_simple(self, test_bin):
        res = self.runner.run(test_bin)

        syscalls = res.get_syscalls()

        # Don't make assumptions on the full syscall set, but we
        # should have at least 20 as a sanity check
        self.assertTrue(len(syscalls) > 20)

        # Get all results for a single callgraph pass
        pass_data = res.get_pass()

        # We shouldn't have any extraction failures for a minimal program
        self.assertTrue(len(pass_data["failures"]) == 0)

    def test_usedata(self):
        self.do_simple("usedata")

    def test_true(self):
        self.do_simple("true")

    def test_true_extra(self):
        res = self.runner.run("true", args=["--dl-file", "dl-libextra.dl_json"])
        syscalls = res.get_syscalls()
        self.assertIn(700, syscalls)

    @unittest.skip
    def test_true_extra_asdep(self):
        res = self.runner.run("true", args=["--extra-as-dep",
                                            "--dl-file", "dl-libextra.dl_json"])
        syscalls = res.get_syscalls()
        self.assertIn(700, syscalls)

    @unittest.skip
    def test_true_dl(self):
        res = self.runner.run("true", args=["--extra-as-dep",
                                            "--dl-file", "dl-libdata.dl_json"])
        syscalls = res.get_syscalls()

        self.assertIn(600, syscalls)

    @unittest.skip
    def test_true_pthread_asdep(self):
        res = self.runner.run("true", args=["--extra-as-dep",
                                            "--dl-file", "dl-pthread.dl_json"])
        syscalls = res.get_syscalls()
        self.assertTrue(len(syscalls) > 30)


if __name__ == "__main__":
    unittest.main()
