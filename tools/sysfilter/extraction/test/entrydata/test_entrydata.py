import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestEntryData(unittest.TestCase):

    # Runs once for all tests
    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))

    def test_has_single(self):
        res = self.runner.run("libb.so",
                              args=["--entry-data", "bmod"])
        syscalls = res.get_syscalls()
        self.assertEqual([503], syscalls)

    def test_nested(self):
        res = self.runner.run("libb.so",
                              args=["--entry-data", "xdata"])
        syscalls = res.get_syscalls()
        self.assertEqual([503, 601, 602], syscalls)

    def test_symbol_data(self):
        res = self.runner.run("libb.so",
                              args=["--entry-symbol", "xdata"])
        syscalls = res.get_syscalls()
        self.assertEqual([503, 601, 602], syscalls)

    def test_symbol_func(self):
        res = self.runner.run("libb.so",
                              args=["--entry-symbol", "bf3"])
        syscalls = res.get_syscalls()
        self.assertEqual([603], syscalls)


if __name__ == "__main__":
    unittest.main()
