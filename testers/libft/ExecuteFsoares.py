import logging
import os
from pipes import quote
import re
import subprocess
from typing import List

from halo import Halo
from pexpect import run
from testers.libft.BaseExecutor import remove_ansi_colors
from utils.ExecutionContext import get_timeout_script, has_bonus, is_strict
from utils.TerminalColors import CT

logger = logging.getLogger("fsoares")

test_regex = re.compile(r"ft_(\w+)\s*: (.*)")


class ExecuteFsoares():

	def __init__(self, tests_dir, temp_dir, to_execute: List[str], missing) -> None:
		self.folder = "fsoares"
		self.temp_dir = os.path.join(temp_dir, self.folder)
		self.to_execute = to_execute
		self.missing = missing
		self.tests_dir = os.path.join(tests_dir, self.folder)
		self.git_url = None

	def execute(self):
		self.compile_test()
		result = self.execute_tests()
		logger.info(f"result: {result}")
		return self.show_failed(result)

	def compile_test(self):
		os.chdir(self.temp_dir)
		logger.info(f"On directory {os.getcwd()}")

		print()
		text = f"{CT.CYAN}Compiling tests: {CT.B_WHITE}{self.folder}{CT.NC} (my own)"
		with Halo(text=text) as spinner:
			for func in self.to_execute:
				strict = " -DSTRICT_MEM" if is_strict() else ""
				bonus = " list_utils.c" if has_bonus() else ""
				command = (f"gcc{strict} -Wall -Wextra -Werror utils.c{bonus} " +
				           f"test_{func}.c malloc_mock.c -L. -lft -o test_{func}.out -ldl")
				logger.info(f"executing {command}")
				res = subprocess.run(command, shell=True, capture_output=True, text=True)
				logger.info(res)
				if res.returncode != 0:
					spinner.fail()
					print(res.stderr)
					raise Exception("Problem compiling the tests")
			spinner.succeed()

	def execute_tests(self):
		print(f"{CT.CYAN}Testing:{CT.NC}")
		spinner = Halo(placement="right")

		def parse_output(output: str):
			lines = output.splitlines()
			if lines[-1] == "":
				lines = lines[:-1]
			match = test_regex.match(lines[-1])
			return (match.group(1), match.group(2), lines)

		def get_output(func, output):
			if "Alarm clock" in output:
				output = f"ft_{func.ljust(13)}: {CT.B_YELLOW}Infinite Loop{CT.NC}\n"
			spinner.stop()
			print(output, end="")
			spinner.start()
			return output

		def execute_test(func):
			spinner.start(f"ft_{func.ljust(13)}:")
			out, code = run("sh -c " + quote(f"{get_timeout_script()} ./test_{func}.out"), withexitstatus=1)
			output = out.decode('ascii', errors="backslashreplace");
			logger.info(output)
			output = get_output(func, output)
			return parse_output(remove_ansi_colors(output))

		result = [execute_test(func) for func in self.to_execute]
		logger.info(f"tests result: {result}")
		spinner.stop()
		return result

	def show_failed(self, output):

		def is_error(result):
			return result != "OK" and result != "No test yet"

		errors = []
		for func, res, lines in output:
			if (is_error(res)):
				errors.append(func)

		logger.warn(f"found errors for functions: {errors}")
		return errors
