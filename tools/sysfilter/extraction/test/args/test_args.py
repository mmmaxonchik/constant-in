import os
import sys
import unittest
import itertools

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner, ATStatus
from tests import FCG_VACUUM, FCG_DIRECT, FCG_NAIVE
from tests import PASS_ALL, PASS_INIT, PASS_FINI, PASS_MAIN, \
    PASS_IFUNC, PASS_NSS

FCGS_ALL = [
    FCG_VACUUM,
    FCG_DIRECT,
    FCG_NAIVE,
]

PASSES_ALL = [
    PASS_ALL,
    PASS_INIT,
    PASS_FINI,
    PASS_MAIN,
    PASS_IFUNC,
    PASS_NSS,
]

PASSES_EXTRA = [x for x in PASSES_ALL if x != PASS_ALL]


class TestEntryData(unittest.TestCase):

    SYS_ALL = [600, 601, 602]
    SYS_INIT = [600]
    SYS_FINI = [602]
    SYS_MAIN = [601]

    RES_MAP = {
        PASS_INIT: SYS_INIT,
        PASS_FINI: SYS_FINI,
        PASS_MAIN: SYS_MAIN,
        PASS_ALL: SYS_ALL,
    }

    # Runs once for all tests
    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))

    def assert_pass(self, res, sys, fcg_type="vacuum", pass_type="all"):
        syscalls = res.get_syscalls(fcg_type=fcg_type,
                                    pass_type=pass_type)
        self.assertEqual(sys, syscalls)

    def assert_dl_single(self, res, func, value=None,
                         fcg_type=FCG_VACUUM, pass_type=PASS_ALL,
                         status=ATStatus.OK, target="dlsym"):
        at_info = res.get_at_info(fcg_type=fcg_type, pass_type=pass_type)

        at_func = [x for x in at_info \
                   if (x["in_function"] == func) and (x["target_function"] == target)]
        self.assertEqual(1, len(at_func))

        ati = at_func[0]
        self.assertEqual(status, ATStatus(ati["status"]))
        if value is not None:
            self.assertEqual(value, ati["value"])

    def test_simple(self):
        res = self.runner.run("libtest.so")
        syscalls = res.get_syscalls()
        self.assertEqual([600, 601, 602], syscalls)

    def test_a1(self):
        res = self.runner.run("libtest.so", args=["--multi-fcg"])
        self.assertTrue(res.has_pass(FCG_VACUUM, PASS_ALL))
        self.assertTrue(res.has_pass(FCG_NAIVE, PASS_ALL))
        self.assert_pass(res, self.SYS_ALL, fcg_type=FCG_VACUUM)
        self.assert_pass(res, self.SYS_ALL, fcg_type=FCG_DIRECT)
        self.assert_pass(res, self.SYS_ALL, fcg_type=FCG_NAIVE)

    def test_all_passes(self):
        res = self.runner.run("libtest.so", args=["--multi-fcg", "--multi-pass", "--dump-fcg"])
        for fcg, ctpass in itertools.product(FCGS_ALL, PASSES_ALL):

            self.assertTrue(res.has_pass(fcg, ctpass))

            if (fcg == FCG_VACUUM) and (ctpass in self.RES_MAP):
                sys = res.get_syscalls(fcg_type=fcg, pass_type=ctpass)
                self.assertEqual(self.RES_MAP[ctpass], sys)

        self.assertFalse(res.has_pass("potato", PASS_ALL))
        self.assertFalse(res.has_pass("naive", "potato"))

    def test_spec(self):
        res = self.runner.run("libtest.so", args=["--multi-passes", "vacuum", "--multi-fcg"])

        for ctpass in PASSES_ALL:
            self.assertTrue(res.has_pass(FCG_VACUUM, ctpass))

        for fcg in [FCG_DIRECT, FCG_NAIVE]:
            self.assertTrue(res.has_pass(fcg, PASS_ALL))
            for ctpass in PASSES_ALL:
                if ctpass == PASS_ALL:
                    continue

                self.assertFalse(res.has_pass(fcg, ctpass))

    def test_all_passes_dl(self):
        res = self.runner.run("dltest", args=["--multi-fcg", "--multi-pass", "--arg-mode", "--dump-fcg"])
        for fcg, ctpass in itertools.product(FCGS_ALL, PASSES_ALL):

            self.assertTrue(res.has_pass(fcg, ctpass))
            self.assertTrue(res.has_at_info(fcg_type=fcg, pass_type=ctpass))

            if (fcg == FCG_VACUUM) and (ctpass in self.RES_MAP):
                sys = res.get_syscalls(fcg_type=fcg, pass_type=ctpass)
                sys_gold = self.RES_MAP[ctpass]
                for nr in sys_gold:
                    self.assertIn(nr, sys)

    def test_some_argmode(self):
        res = self.runner.run("dltest", args=["--multi-passes", "vacuum",
                                              "--arg-mode-passes", "vacuum,naive",
                                              "--dump-fcg"])
        # Should have all passes for VCG
        for ctpass in PASSES_ALL:
            self.assertTrue(res.has_pass(FCG_VACUUM, ctpass))
            self.assertTrue(res.has_at_info(FCG_VACUUM, ctpass))

        # Should have argtrack data for pass all in naive
        self.assertTrue(res.has_pass(FCG_NAIVE, PASS_ALL))
        self.assertTrue(res.has_at_info(FCG_NAIVE, PASS_ALL))

        # Should NOT have other passes for naive
        for ctpass in PASSES_EXTRA:
            self.assertFalse(res.has_pass(FCG_NAIVE, ctpass))

        # Should not have pass direct
        self.assertFalse(res.has_pass(FCG_DIRECT, PASS_ALL))

    def test_argmode_gold(self):
        res = self.runner.run("dltest", override_args=["--multi-fcg",
                                                       "--multi-passes", "vacuum",
                                                       "--arg-mode-passes", "vacuum",
        ])
        # Should have all passes for VCG
        for ctpass in PASSES_ALL:
            self.assertTrue(res.has_pass(FCG_VACUUM, ctpass))
            self.assertTrue(res.has_at_info(FCG_VACUUM, ctpass))

            sys = res.get_syscalls(fcg_type=FCG_VACUUM, pass_type=ctpass)

            # Let's make sure the syscalls make sense while we're at it
            if ctpass in self.RES_MAP:
                sys_gold = self.RES_MAP[ctpass]
                for nr in sys_gold:
                    self.assertIn(nr, sys)

        # Should have data for other FCG types, but not multipass/argtrack info
        for ft in [FCG_NAIVE, FCG_DIRECT]:
            self.assertTrue(res.has_pass(ft, PASS_ALL))

            # Should NOT have argtrack info
            self.assertFalse(res.has_at_info(ft, PASS_ALL))

            # Should NOT have other passes
            for ctpass in PASSES_EXTRA:
                self.assertFalse(res.has_pass(ft, ctpass))


if __name__ == "__main__":
    unittest.main()
