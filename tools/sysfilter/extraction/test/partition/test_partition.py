import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestPartition(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestPartition, self).__init__(*args, **kwargs)

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("part", args=["--dump-fcg", "--multi-pass"])
        cls.res_func = cls.runner.run("part-func", args=["--dump-fcg", "--multi-pass"])
        cls.res_nss = cls.runner.run("part-nss", args=["--dump-fcg",
                                                       "--multi-pass"])
        cls.res_init = cls.runner.run("part-init", args=["--dump-fcg",
                                                         "--multi-pass"])

    def setUp(self):
        pass

    def test_part_simple(self):
        self.check_passes(self.res)

    def test_part_func(self):
        self.check_passes(self.res_func)

    def check_passes(self, res):
        self.check_all(res)
        self.check_init(res)
        self.check_main(res)
        self.check_fini(res)

    def check_all(self, res):
        self.assertIn(601, res.get_syscalls(pass_type="all"))
        self.assertIn(602, res.get_syscalls(pass_type="all"))
        self.assertIn(603, res.get_syscalls(pass_type="all"))
        self.assertNotIn(604, res.get_syscalls(pass_type="all"))

    def check_init(self, res):
        self.assertIn(601, res.get_syscalls(pass_type="init"))
        self.assertNotIn(602, res.get_syscalls(pass_type="init"))
        self.assertNotIn(603, res.get_syscalls(pass_type="init"))
        self.assertNotIn(604, res.get_syscalls(pass_type="init"))

    def check_main(self, res):
        self.assertNotIn(601, res.get_syscalls(pass_type="main"))
        self.assertIn(602, res.get_syscalls(pass_type="main"))
        self.assertEqual([602],
                         res.get_syscalls(pass_type="main"))

        self.assertNotIn(603, res.get_syscalls(pass_type="main"))
        self.assertNotIn(604, res.get_syscalls(pass_type="main"))

    def check_fini(self, res):
        self.assertNotIn(601, res.get_syscalls(pass_type="fini"))
        self.assertNotIn(602, res.get_syscalls(pass_type="fini"))
        self.assertIn(603, res.get_syscalls(pass_type="fini"))
        self.assertNotIn(604, res.get_syscalls(pass_type="fini"))

    def test_nss_no_default(self):
        self.assertEqual(0, len(self.res.get_syscalls(pass_type="nss")))

    def test_nss(self):
        sys_main = self.res_nss.get_syscalls(pass_type="main")
        sys_nss = self.res_nss.get_syscalls(pass_type="nss")

        self.assertIn(601, sys_main)
        self.assertNotIn(601, sys_nss)
        self.assertTrue(len(sys_nss) != 0)

    def test_init(self):
        self.assertIn(601, self.res_init.get_syscalls(pass_type="all"))
        self.assertIn(601, self.res_init.get_syscalls(pass_type="init"))

        # Syscall is not AT in main, so not found
        # This is why we need the atextra callgraph
        self.assertNotIn(601, self.res_init.get_syscalls(pass_type="main"))


if __name__ == "__main__":
    unittest.main()
