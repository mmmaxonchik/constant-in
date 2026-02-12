import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestHello(unittest.TestCase):
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

    def test_hello(self):
        self.do_simple("hello")

    def test_hello_noopt(self):
        self.do_simple("hello-noopt")

    def test_hello_nodebug(self):
        self.do_simple("hello-nodebug")


if __name__ == "__main__":
    unittest.main()
