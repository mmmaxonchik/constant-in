import os
import sys
import unittest

our_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, our_base)

from tests import TestRunner


class TestNss(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        super(TestNss, self).__init__(*args, **kwargs)

    @classmethod
    def setUpClass(cls):
        cls.runner = TestRunner(os.path.dirname(__file__))

    def setUp(self):
        pass

    # def test_gethostbyname(self):
    #     self.res = self.runner.run("nsstest", args=["--dump-fcg"])
    #     frontend_used = self.res.get_nss_frontend_funcs()
    #     backend_used = self.res.get_nss_backend_funcs()

    #     self.assertIn("gethostbyname", frontend_used)
    #     self.assertTrue(len(backend_used) > 0)

    def test_getaddrinfo(self):
        self.res = self.runner.run("getaddrinfo", args=["--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertIn("getaddrinfo", frontend_used)
        self.assertTrue(len(backend_used) > 0)
        self.assertIn("_nss_files_gethostbyname2_r", backend_used)
        self.assertIn("_nss_dns_gethostbyname2_r", backend_used)

    def test_getaliasbyname(self):
        self.res = self.runner.run("getaliasbyname",
                                   args=["--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertEqual({"getaliasbyname"}, frontend_used)
        self.assertTrue(len(backend_used) > 0)
        self.assertIn("_nss_files_getaliasbyname_r", backend_used)

    def test_nofunc_failure(self):
        self.res = self.runner.run("nsstest", args=["--dump-fcg",
                                                    "--nss-config", "nss-fake.conf"])
        self.assertTrue(self.res.rv != 0)

    @unittest.skip
    def test_lib_failure_ok(self):
        self.res = self.runner.run("getaliasbyname",
                                   args=["--dump-fcg", "--nss-config", "nss-fake.conf"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertEqual({"getaliasbyname"}, frontend_used)
        self.assertTrue(len(backend_used) > 0)
        self.assertIn("_nss_files_getaliasbyname_r", backend_used)

    def test_gethostbyname_disc(self):
        self.res = self.runner.run("nsstest", args=["--nss-discover", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertIn("gethostbyname", frontend_used)
        self.assertTrue(len(backend_used) > 0)

        names_discovered = self.res.get_names_discovered()
        self.assertIn("gethostbyname", names_discovered)

    def test_getaddrinfo_disc(self):
        self.res = self.runner.run("getaddrinfo", args=["--nss-discover", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertIn("getaddrinfo", frontend_used)
        self.assertTrue(len(backend_used) > 0)
        self.assertIn("_nss_files_gethostbyname2_r", backend_used)
        self.assertIn("_nss_dns_gethostbyname2_r", backend_used)

        names_discovered = self.res.get_names_discovered()
        self.assertIn("getaddrinfo", names_discovered)

    def test_getaliasbyname_disc(self):
        self.res = self.runner.run("getaliasbyname",
                                   args=["--nss-discover", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertEqual({"getaliasbyname"}, frontend_used)
        self.assertTrue(len(backend_used) > 0)
        self.assertIn("_nss_files_getaliasbyname_r", backend_used)

        names_discovered = self.res.get_names_discovered()
        self.assertIn("getaliasbyname", names_discovered)

    @unittest.skip
    def test_getaddrinfo_reportonly(self):
        self.res = self.runner.run("getaddrinfo",
                                   args=["--nss-discover", "--nss-report-only", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertIn("getaddrinfo", frontend_used)
        self.assertTrue(len(backend_used) == 0)

        names_discovered = self.res.get_names_discovered()
        self.assertIn("getaddrinfo", names_discovered)

    def test_getaddrinfo_legacy(self):
        self.res = self.runner.run("getaddrinfo",
                                   args=["--nss-discover",
                                         "--nss-report-legacy", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        legacy_frontend_used = self.res.get_nss_legacy_frontend_funcs()
        legacy_backend_used = self.res.get_nss_legacy_backend_funcs()

        self.assertIn("getaddrinfo", frontend_used)
        self.assertTrue(len(backend_used) > 0)

        self.assertTrue(len(frontend_used) >= len(legacy_frontend_used))
        self.assertTrue(len(backend_used) >= len(legacy_backend_used))

    @unittest.skip
    def test_getgrent_multi_iteration(self):
        self.res = self.runner.run("getgrent",
                                   args=["--nss-discover", "--dump-fcg"])
        frontend_used = self.res.get_nss_frontend_funcs()
        backend_used = self.res.get_nss_backend_funcs()

        self.assertIn("getgrent", frontend_used)
        self.assertIn("getpwent", frontend_used)
        self.assertIn("_nss_files_getgrent_r", backend_used)
        self.assertIn("_nss_files_getpwent_r", backend_used)

    def test_getaddrinfo_disc_fcg(self):
        self.res = self.runner.run("getaddrinfo", args=["--multi-fcg",
                                                        "--nss-discover", "--dump-fcg"])
        sys_vacuum = set(self.res.get_syscalls(fcg_type="vacuum"))
        sys_naive = set(self.res.get_syscalls(fcg_type="naive"))

        self.assertTrue(len(sys_naive) >= len(sys_vacuum))
        self.assertEqual(0, len(sys_vacuum.difference(sys_naive)))

        names_discovered = self.res.get_names_discovered(fcg_type="naive")
        self.assertIn("getaddrinfo", names_discovered)


if __name__ == "__main__":
    unittest.main()
