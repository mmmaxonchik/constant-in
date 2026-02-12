import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner, ATStatus

class TestCmov(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestCmov, self).__init__(*args, **kwargs)

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res1 = cls.runner.run("cmov1", args=["-v", "--dump-fcg"])
        cls.res1_noif = cls.runner.run("cmov1-noif", args=["-v", "--dump-fcg"])

    def setUp(self):
        pass

    @unittest.skip
    def test_cmov1_func(self):
        syscalls = self.res1.get_syscalls()
        self.assertIn(600, syscalls)
        self.assertIn(601, syscalls)

    @unittest.skip
    def test_cmov1_raw(self):
        syscalls = self.res1.get_syscalls()
        self.assertIn(602, syscalls)
        self.assertIn(603, syscalls)

    def test_cmov1_noif_func(self):
        syscalls = self.res1_noif.get_syscalls()
        self.assertIn(600, syscalls)
        self.assertIn(601, syscalls)

    def test_cmov1_noif_raw(self):
        syscalls = self.res1_noif.get_syscalls()
        self.assertIn(602, syscalls)
        self.assertIn(603, syscalls)


if __name__ == "__main__":
    unittest.main()
