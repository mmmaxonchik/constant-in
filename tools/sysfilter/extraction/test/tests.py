import os
import pdb
import sys
import enum
import json
import pathlib
import logging
import traceback
import argparse
import subprocess

EXTRACT_BASE = pathlib.Path(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SYSFILTER_EXTRACT = EXTRACT_BASE / "app" / "sysfilter_extract"

FCG_VACUUM = "vacuum"
FCG_DIRECT = "direct"
FCG_NAIVE = "naive"

PASS_ALL = "all"
PASS_INIT = "init"
PASS_FINI = "fini"
PASS_MAIN = "main"
PASS_IFUNC = "ifunc"
PASS_NSS = "nss"


class ATStatus(enum.IntFlag):
    OK = 0
    NO_REF_STATE = 1
    UNKNOWN_OP = 2
    BAD_STRING = 4
    NOT_ALL_CONSTANTS = 8
    NO_CALL = 16
    ASSERT = 32
    TARGET_IS_AT = 64
    ADDRESS_RESOLUTION_FAILED = 128

    UNKNOWN = 65536


class ATReg(enum.Enum):
    RDI = 7
    RSI = 6
    RDX = 2
    RCX = 1


class RunResult():
    RV_SUCCESS = 0
    ARGTRACK_FAILED = "<failed>"

    def __init__(self, rv, output):
        self.rv = rv
        self.output = output

    def get_pass(self, fcg_type=None, pass_type=None):
        if fcg_type is None:
            fcg_types = self.output["analysis_modes"]
            if len(fcg_types) == 1:
                fcg_type = fcg_types[0]
            else:
                raise ValueError("Must specify FCG type")

        assert(self.rv == self.RV_SUCCESS)
        assert(fcg_type in self.output["analysis_modes"])

        fcg_data = self.output[fcg_type]
        pass_types = fcg_data["analysis"].keys()

        if (pass_type is None) and (len(pass_types) == 1):
            pass_type = list(pass_types)[0]

        assert("analysis" in fcg_data)
        assert(pass_type in fcg_data["analysis"].keys())

        pass_data = fcg_data["analysis"][pass_type]

        return pass_data

    def has_pass(self, fcg_type=None, pass_type=None):
        if fcg_type is None:
            fcg_types = self.output["analysis_modes"]
            if len(fcg_types) == 1:
                fcg_type = fcg_types[0]
            else:
                raise ValueError("Must specify FCG type")

        assert(self.rv == self.RV_SUCCESS)

        in_modes = fcg_type in self.output["analysis_modes"]
        in_data = fcg_type in self.output
        if in_modes and in_data:
            fcg_data = self.output[fcg_type]
            pass_in_list = pass_type in fcg_data["analysis_passes"]
            pass_in_data = pass_type in fcg_data["analysis"]
            return pass_in_list and pass_in_data
        else:
            return False

    def get_syscalls(self, fcg_type=None, pass_type=None):
        assert(self.rv == self.RV_SUCCESS)
        if (fcg_type is None) and (pass_type is None):
            return self.output["syscalls"]
        else:
            pass_data = self.get_pass(fcg_type, pass_type)

            return pass_data["syscalls"]

    def get_callgraph(self, fcg_type="vacuum", pass_type="all"):
        pass_data = self.get_pass(fcg_type, pass_type)
        if "callgraph" not in pass_data:
            raise ValueError("Result does not contain callgraph!  Was sysfilter run with --dump-fcg?")

        return pass_data["callgraph"]

    def callgraph_get_function(self, name, module=None,
                               fcg_type="vacuum", pass_type="all"):
        callgraph = self.get_callgraph(fcg_type=fcg_type, pass_type=pass_type)
        ret = None

        for func_tag, func_info in callgraph["funcs"].items():
            if func_info["name"] == name:
                if module is not None:
                    if func_info["module"] == module:
                        ret = func_info
                        break
                else:
                    ret = func_info
                    break

        return ret

    def callgraph_has_function(self, name, module=None,
                               fcg_type="vacuum", pass_type="all"):

        ret = self.callgraph_get_function(name, module, fcg_type, pass_type)
        return (ret is not None)

    def get_at_info(self, fcg_type="vacuum", pass_type="all",
                    function=None):
        assert(self.rv == self.RV_SUCCESS)
        pass_data = self.get_pass(fcg_type, pass_type)

        at_infos = pass_data["arg_track_values"]

        if function is None:
            return at_infos
        else:
            return [x for x in at_infos if x["target_function"] == function]

    def has_at_info(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)

        pass_data = self.get_pass(fcg_type, pass_type)
        return ("arg_track_values" in pass_data)

    def get_nss_frontend_funcs(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)

        pass_data = self.get_pass(fcg_type, pass_type)
        nss_data = pass_data["nss_info"]

        frontend_funcs = set()
        for k, v in nss_data["frontend_funcs_used"].items():
            frontend_funcs.update(v)

        return frontend_funcs

    def get_nss_backend_funcs(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)
        pass_data = self.get_pass(fcg_type, pass_type)
        nss_data = pass_data["nss_info"]

        backend_funcs = {x["function"] for x in
                         nss_data["backend_funcs_loaded"]}
        return backend_funcs

    def get_nss_legacy_frontend_funcs(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)

        pass_data = self.get_pass(fcg_type, pass_type)
        nss_data = pass_data["nss_info"]

        frontend_funcs = set()
        for k, v in nss_data["legacy_frontend_funcs_used"].items():
            frontend_funcs.update(v)

        return frontend_funcs

    def get_nss_legacy_backend_funcs(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)
        pass_data = self.get_pass(fcg_type, pass_type)
        nss_data = pass_data["nss_info"]

        backend_funcs = {x["function"] for x in
                         nss_data["legacy_backend_funcs_loaded"]}
        return backend_funcs

    def get_nss_database_info(self):
        assert(self.rv == self.RV_SUCCESS)
        nss_data = self.output["nss_db_info"]

        return nss_data

    def get_funcs_discovered(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)
        pass_data = self.get_pass(fcg_type, pass_type)
        discovered_funcs = pass_data["extra_funcs_discovered"];

        return discovered_funcs

    def get_names_discovered(self, fcg_type="vacuum", pass_type="all"):
        assert(self.rv == self.RV_SUCCESS)
        func_infos = self.get_funcs_discovered(fcg_type, pass_type)

        names = {x["function"] for x in func_infos}
        return names


class TestRunner():
    DEFAULT_ARGS = [
        "-v",
        "--full-json",
        "--arg-mode",
    ]

    def __init__(self, cwd=None):
        logging.basicConfig(level=logging.INFO)
        if cwd is None:
            self.cwd = pathlib.Path(os.getcwd())
        else:
            self.cwd = pathlib.Path(cwd)

    def rel_path(self, file_name):
        return self.cwd / file_name

    def run(self, target_bin, override_args=None,
            log_file=None, output_file=None, dry_run=False, args=None):

        def _bin_ext(bin_name, ext):
            return "{}.{}".format(str(bin_name), ext)

        bin_path = self.rel_path(target_bin)
        if not bin_path.exists():
            raise ValueError("Could not find test binary:  {}".format(str(bin_path)))

        cmd = [
            str(SYSFILTER_EXTRACT),
            str(bin_path),
        ]

        if override_args is not None:
            cmd.extend(override_args)
        else:
            cmd.extend(self.DEFAULT_ARGS)

        if args:
            cmd.extend(args)

        work_dir = self.cwd

        output_path = output_file if output_file is not None else \
            _bin_ext(target_bin, "json")

        log_path = log_file if log_file is not None else \
            _bin_ext(target_bin, "log")

        cmd.extend(["-o", str(output_path)])

        logging.info("EXTRACT {}".format(bin_path.relative_to(EXTRACT_BASE / "test")))

        if dry_run:
            cmd.insert(0, "echo")

        with open(log_path, "w") as log_fd:
            proc = subprocess.Popen(cmd,
                                    stdout=log_fd,
                                    stderr=log_fd,
                                    cwd=work_dir)

            stdout, stderr = proc.communicate()
            proc.poll()
            proc.wait()
            rv = proc.returncode

        if rv > 128:
            rv = -((~rv & 0xff) + 1)

        json_data = None
        if rv == RunResult.RV_SUCCESS:
            with open(self.rel_path(output_path), "r") as json_fd:
                json_data = json.load(json_fd)

        result = RunResult(rv=rv, output=json_data)
        return result


def main(input_args):
    pass


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    # except SystemExit as e:
    #     sys.exit(e.code)
    except:
        extype, value, tb = sys.exc_info()
        traceback.print_exc()
        pdb.post_mortem(tb)
