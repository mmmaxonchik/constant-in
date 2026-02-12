import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner, ATReg

FUNC_PTH = "pthread_create@@GLIBC_2.2.5"


class TestPartition(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestPartition, self).__init__(*args, **kwargs)

    def get_at_func(self, res, in_function, target=FUNC_PTH, reg=ATReg.RDX):
        pth_info = res.get_at_info(function=target)
        pth_this_func = [x for x in pth_info
                         if x["in_function"] == in_function and ((reg is None) or \
                                                                 (ATReg(x["register"]) == reg))]

        self.assertEqual(1, len(pth_this_func))
        return pth_this_func[0]

    def get_at_funcname(self, res, in_function, target=FUNC_PTH, reg=ATReg.RDX):
        ati = self.get_at_func(res, in_function, target=target, reg=reg)
        self.assertEqual(0, ati["status"])
        return ati["function_from_value"]

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("part-pthread", args=["--dump-fcg", "--arg-mode"])
        cls.res_lib = cls.runner.run("part-libpth",
                                     args=["--dump-fcg", "--arg-mode"])
        cls.res_wrap = cls.runner.run("part-wrap",
                                      args=["--dump-fcg", "--arg-mode"])
        cls.res_clone = cls.runner.run("part-clone",
                                       args=["--dump-fcg", "--arg-mode"])

    def setUp(self):
        pass

    def test_simple(self):
        self.assertEqual("do_thing", self.get_at_funcname(self.res, "main"))

    def test_lib_local(self):
        self.assertEqual("libpth_do_thing",
                         self.get_at_funcname(self.res_lib, "libpth_do_thread"))

    def test_bin_plt(self):
        self.assertEqual("libpth_do_ext",
                         self.get_at_funcname(self.res, "do_plt"))

    def test_bin_arg_fail(self):
        ati = self.get_at_func(self.res, "do_thing_wrap")
        self.assertTrue(ati["status"] != 0)

    def test_wrap_at(self):
        syscalls = self.res_wrap.get_syscalls()

        self.assertIn(603, syscalls)

    # def test_wrap_at_entry(self):
    #     res = self.runner.run("part-wrap",
    #                           args=["--dump-fcg", "--arg-mode",
    #                                 "--entry-point", "run_as_thread",
    #                                 "--entry-point-only"])

    #     self.assertTrue(res.callgraph_has_function("run_as_thread"))
    #     self.assertTrue(not res.callgraph_has_function("main"))

    #     syscalls = res.get_syscalls()
    #     self.assertIn(603, syscalls)

    def test_wrap_arg(self):
        self.assertEqual("run_as_thread",
                         self.get_at_funcname(self.res_wrap, "do_simple", reg=ATReg.RDX))
        # self.assertEqual("do_syscall",
        #                  self.get_at_funcname(self.res_wrap, "do_simple", reg=ATReg.RCX))

    @unittest.skip
    def test_wrap_data(self):
        self.assertEqual("do_data_syscall",
                         self.get_at_funcname(self.res_wrap, "do_wrap_data", reg=ATReg.RCX))

    @unittest.skip
    def test_wrap_bogus(self):
        ati = self.get_at_func(self.res_wrap, "do_bogus", reg=ATReg.RCX)
        self.assertTrue(ati["status"] != 0)

    def test_wrap_multi_data(self):
        pth_info = self.res_wrap.get_at_info(function=FUNC_PTH)
        pth_this = [x for x in pth_info if x["in_function"] == "do_wrap_multi_data"]
        self.assertTrue(all([x["status"] == 0 for x in pth_this]))

        ati_func = [x for x in pth_this if ATReg(x["register"]) == ATReg.RDX]
        #ati_args = [x for x in pth_this if ATReg(x["register"]) == ATReg.RCX]

        self.assertEqual(1, len(ati_func))
        self.assertEqual("run_as_thread_multi_data", ati_func[0]["function_from_value"])

        # self.assertEqual(2, len(ati_args))

        # arg_values = [x["function_from_value"] for x in ati_args]
        # self.assertIn("do_data_syscall", arg_values)
        # self.assertIn("libpth_do_ext", arg_values)

    @unittest.skip
    def test_wrap_data_local(self):
        ati = self.get_at_func(self.res_wrap, "do_wrap_data_local", reg=ATReg.RCX)
        self.assertTrue(ati["status"] != 0)

    @unittest.skip
    def test_wrap_local(self):
        ati = self.get_at_func(self.res_wrap, "do_local", reg=ATReg.RDX)
        self.assertTrue(ati["status"] != 0)

    def test_clone(self):
        self.assertEqual("clone_child",
                         self.get_at_funcname(self.res_clone, "do_clone",
                                              target="clone", reg=ATReg.RDI))

    def test_fork(self):
        ati = self.get_at_func(self.res_clone, "do_fork", target="fork", reg=None)
        self.assertEqual(0, ati["status"])

    def test_unshare(self):
        ati = self.get_at_func(self.res_clone, "do_clone", target="unshare", reg=None)
        self.assertEqual(0, ati["status"])
        self.assertEqual(42, ati["value"])


if __name__ == "__main__":
    unittest.main()
