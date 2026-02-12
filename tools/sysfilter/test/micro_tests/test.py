import unittest
import os
import subprocess
import json
from parameterized import parameterized
from subprocess import PIPE

# TODO: Fix shell=True as SAST says it's a high severity bug.

class SysfilterTest(unittest.TestCase):
	root_dir = os.getcwd()
	tests = str(subprocess.run("ls -d */", shell=True, stdout=PIPE, stderr=PIPE).stdout)[2:].split("/\\n")[:-1]
	
	@parameterized.expand(tests)
	def test_sysfilter(self, dir):
		os.chdir(dir)
		self.build(dir)
		self.sysfilter_extract(dir)
		self.sysfilter_enforce(dir, inverse=True)
		self.run_program(dir, pass_test=False)
		self.sysfilter_enforce(dir, inverse=True)
		self.run_program(dir, pass_test=True)

	def tearDown(self):
		"""
		Helps returning to the correct directory even if execution stops due to an error.
		"""
		os.chdir(self.root_dir)

	def build(self, filename):
		build_process = subprocess.run("make")
		self.assertEqual(build_process.returncode, 0)

	def sysfilter_extract(self, filename):
		extract_process = subprocess.run("../../../extraction/app/build_x86_64/sysfilter_extract -o "+ filename+ "_syscall_list.json "+ filename,shell=True,)
		# extract_process = subprocess.run("../sysfilter_extract -o "+filename+"_syscall_list.json "+filename,shell=True)
		self.assertEqual(extract_process.returncode, 0)

	def sysfilter_enforce(self, filename, inverse=False):
		if inverse:
			total_syscalls_present = 335
			total_syscall_list = {i for i in range(total_syscalls_present)}
			with open(filename + "_syscall_list.json") as f:
				current_syscall = set(json.load(f))
			current_syscall_complement = list(total_syscall_list - current_syscall)
			with open(filename + "_syscall_list.json", "w") as outfile:
				json.dump(current_syscall_complement, outfile)

		enforce_process = subprocess.run("bash ../../../enforcement/sysfilter_enforce "+ filename+ " "+ filename+ "_syscall_list.json ",shell=True,)
		self.assertEqual(enforce_process.returncode, 0)

	def run_program(self, filename, pass_test=True):
		run_process = subprocess.run("make test", shell=True)
		if pass_test:
			self.assertEqual(run_process.returncode, 0)
		else:
			self.assertNotEqual(run_process.returncode, 0)


if __name__ == "__main__":
	unittest.main()
