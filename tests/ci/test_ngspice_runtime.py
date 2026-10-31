"""Regression tests for the pinned package's raw and post-install spinit forms."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'configure_ngspice', Path(__file__).resolve().parents[2] / 'scripts/ci/configure_ngspice.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
normalize = module.normalize_spinit


class SpinitPaths(unittest.TestCase):
    prefix = 'D:/a/_temp/msys64/ucrt64'

    def test_raw_msys_paths_and_semantics(self):
        text = '* comment /ucrt64/ stays\nif $?xspice_enabled\n codemodel /ucrt64/lib/ngspice/analog.cm\nend\n'
        after, paths = normalize(text, self.prefix)
        self.assertEqual(after, text.replace('codemodel /ucrt64/', 'codemodel ' + self.prefix + '/'))
        self.assertEqual(len(paths), 1)
        self.assertTrue(paths[0]['converted'])

    def test_already_native_is_byte_unchanged(self):
        for path in ('D:/a/_temp/msys64/ucrt64/lib/ngspice/analog.cm',
                     r'C:\msys64\ucrt64\lib\ngspice\analog.cm'):
            text = ' codemodel "' + path + '"\r\n'
            after, paths = normalize(text, self.prefix)
            self.assertEqual(after, text)
            self.assertFalse(paths[0]['converted'])

    def test_mixed_quoted_paths_spaces_and_idempotence(self):
        text = ' codemodel "/ucrt64/lib/ngspice/analog.cm"\r\n osdi /ucrt64/lib/ngspice/test.osdi\r\n'
        once, _ = normalize(text, 'C:/path with spaces/ucrt64')
        twice, paths = normalize(once, 'C:/path with spaces/ucrt64')
        self.assertEqual(once, twice)
        self.assertIn('osdi "C:/path with spaces/ucrt64/lib/ngspice/test.osdi"', once)
        self.assertFalse(any(item['converted'] for item in paths))

    def test_known_double_prefix_rejected(self):
        valid = ' codemodel D:/a/_temp/msys64/ucrt64/lib/ngspice/analog.cm\n'
        broken = valid.replace('/ucrt64/', self.prefix + '/')
        self.assertIn('msys64D:', broken)
        with self.assertRaises(ValueError):
            normalize(broken, self.prefix)
        self.assertEqual(normalize(valid, self.prefix)[0], valid)

    def test_unknown_format_and_bad_prefix_rejected(self):
        with self.assertRaises(ValueError):
            normalize('codemodel /other/analog.cm\n', self.prefix)
        with self.assertRaises(ValueError):
            normalize('codemodel /ucrt64/lib/analog.cm\n', '/ucrt64')


if __name__ == '__main__':
    unittest.main()
