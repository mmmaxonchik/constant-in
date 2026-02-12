import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner, ATStatus

TEST_FUNC = "__sysfilter_argtrack_test"

class TestArgtrack(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestArgtrack, self).__init__(*args, **kwargs)

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("argtrack1", args=["-v",
                                                    "--arg-mode", "--dump-fcg"])
        cls.res2 = cls.runner.run("argtrack2", args=["-v",
                                                     "--arg-mode", "--dump-fcg"])
        cls.res4 = cls.runner.run("at4", args=["-v",
                                               "--arg-mode", "--dump-fcg"])
        cls.res7 = cls.runner.run("at7", args=["-v",
                                               "--arg-mode", "--dump-fcg"])

    def setUp(self):
        pass

    def get_at_func(self, res, in_function, target_func):
        pth_info = res.get_at_info(function=target_func)
        pth_this_func = [x for x in pth_info
                         if x["in_function"] == in_function]

        self.assertEqual(1, len(pth_this_func))
        return pth_this_func[0]

    def assertSingleArg(self, res, func, value=None,
                        status=ATStatus.OK, target="dlsym"):
        at_info = res.get_at_info()

        at_func = [x for x in at_info \
                   if (x["in_function"] == func) and (x["target_function"] == target)]
        self.assertEqual(1, len(at_func))

        ati = at_func[0]
        self.assertEqual(status, ATStatus(ati["status"]))
        if value is not None:
            self.assertEqual(value, ati["value"])

    def test_at1a(self):
        self.assertSingleArg(self.res, "a", "printf")

    @unittest.skip("Does not currently work.")
    def test_at1b(self):
        self.assertSingleArg(self.res, "b", "printf")

    def test_at1c(self):
        self.assertSingleArg(self.res, "c",
                             status=(ATStatus.NOT_ALL_CONSTANTS | ATStatus.NO_REF_STATE))

    # Read from data variable
    def test_at1d(self):
        self.assertSingleArg(self.res, "d",
                             status=(ATStatus.NOT_ALL_CONSTANTS | ATStatus.UNKNOWN_OP))

    # Read from argv
    def test_at1e(self):
        self.assertSingleArg(self.res, "main",
                             status=(ATStatus.NOT_ALL_CONSTANTS | ATStatus.UNKNOWN_OP))

    def test_at2c(self):
        self.assertSingleArg(self.res2, "c",
                             status=(ATStatus.NOT_ALL_CONSTANTS | ATStatus.NO_REF_STATE))

    # Can't find functions if they're AT
    def test_at4(self):
        self.assertSingleArg(self.res4, "",
                             status=(ATStatus.TARGET_IS_AT))

    @unittest.skip
    def test_at7_simple(self):
        self.assertSingleArg(self.res7, "main", target=TEST_FUNC,
                             value=42)


if __name__ == "__main__":
    unittest.main()
