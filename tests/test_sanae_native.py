"""Public native SANAE helper tests; no game or closed runtime required."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SanaeNativeTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('cc'), 'C compiler not installed')
    def test_helpers_with_failure_injection(self):
        with tempfile.TemporaryDirectory(prefix='sanae-test-') as directory:
            binary = Path(directory) / 'helpers'
            flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.getenv('SANAE_SANITIZE') == '1' else []
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-g', *flags,
                            '-I', str(ROOT/'open-runner/sanae'), str(ROOT/'tests/native/sanae_helpers.c'),
                            '-lm', '-o', str(binary)], check=True)
            subprocess.run([str(binary)], cwd=directory, check=True)


if __name__ == '__main__':
    unittest.main()
