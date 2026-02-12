import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestOutput(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))
        cls.res = cls.runner.run("hello")

    def setUp(self):
        pass

    def _assert_scope(self, scope, lib_name, has_symbols):
        self.assertIn(lib_name, scope)
        self.assertEqual(scope[lib_name]["has_symbols"], has_symbols)

    def test_analysis_scope(self):
        self.assertIn("analysis_scope", self.res.output)

        scope = self.res.output["analysis_scope"]
        self.assertIn("(executable)", scope)
        self.assertIn("liba.so", scope)

        self._assert_scope(scope, "(executable)", True)
        self._assert_scope(scope, "liba.so", True)

    def test_analysis_scope_nodebug(self):
        res = self.runner.run("hello-stripped")

        self.assertIn("analysis_scope", res.output)

        scope = res.output["analysis_scope"]

        self._assert_scope(scope, "(executable)", False)
        self._assert_scope(scope, "liba.so", True)

    def test_analysis_scope_nolibdebug(self):
        res = self.runner.run("hello-libstripped")

        self.assertIn("analysis_scope", res.output)

        scope = res.output["analysis_scope"]

        self._assert_scope(scope, "(executable)", True)
        self._assert_scope(scope, "liba-stripped.so", False)

    def test_callgraph_info(self):
        pass_data = self.res.get_pass()
        self.assertIn("callgraph_info", pass_data)
        cg_info = pass_data["callgraph_info"]

        self.assertTrue(cg_info["num_direct_edges"] > 0)
        self.assertTrue(cg_info["num_functions"] > 0)
        self.assertTrue(cg_info["num_implicit_sources"] > 0)
        self.assertTrue(cg_info["num_implicit_targets"] > 0)


if __name__ == "__main__":
    unittest.main()
