import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner, FCG_VACUUM, FCG_DIRECT, FCG_NAIVE, \
    PASS_ALL, PASS_INIT, PASS_MAIN, PASS_FINI


class TestCallgraph(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestCallgraph, self).__init__(*args, **kwargs)

    def get_sys(self, res, fcg_type=FCG_VACUUM, pass_type=PASS_ALL):
        syscalls = res.get_syscalls(fcg_type=fcg_type, pass_type=pass_type)
        ret = [nr for nr in syscalls if nr >= 600]

        return ret

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("fcg1", args=["--dump-fcg",
                                               "--multi-passes=vacuum,naive,direct,atextra"])
        cls.res_stripped = cls.runner.run("fcg1-stripped", args=["--dump-fcg",
                                                                 "--multi-passes=vacuum,naive,direct,atextra"])
        cls.res2 = cls.runner.run("fcg2", args=["--dump-fcg",
                                                "--multi-passes=vacuum,naive,direct,atextra"])

    def setUp(self):
        pass

    def test_vcg(self):
        syscalls = self.get_sys(self.res)
        self.assertEqual([600, 601, 603, 609, 610], syscalls)

    def test_vcg_stripped(self):
        syscalls = self.get_sys(self.res_stripped)
        self.assertEqual([600, 601, 603, 606, 607, 608, 609, 610], syscalls)

    def test_direct(self):
        syscalls = self.get_sys(self.res, fcg_type=FCG_DIRECT)

        self.assertEqual([600, 601, 609, 610], syscalls)

    def test_naive(self):
        syscalls = self.get_sys(self.res, fcg_type=FCG_NAIVE)
        self.assertEqual([600, 601, 603, 604, 605, 606, 607, 608, 609, 610], syscalls)

    def test_atextra(self):
        self.assertEqual([600, 601, 603], self.get_sys(self.res2, fcg_type=FCG_VACUUM))
        self.assertEqual([603], self.get_sys(self.res2, fcg_type="atextra"))


if __name__ == "__main__":
    unittest.main()
