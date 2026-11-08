"""Local, network-free regression checks for the dependency checkout boundary."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    'fetch_dependencies', Path(__file__).resolve().parents[1] / 'scripts/fetch-dependencies.py')
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class CheckoutSafetyTests(unittest.TestCase):
    def test_checkout_uses_pin_instead_of_remote_head(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            origin = root / 'origin'
            origin.mkdir()
            subprocess.run(['git', 'init', str(origin)], check=True, capture_output=True)
            for value in ('pinned', 'newer'):
                (origin / 'version.txt').write_text(value, encoding='utf-8')
                subprocess.run(['git', '-C', str(origin), 'add', 'version.txt'], check=True)
                subprocess.run(['git', '-C', str(origin), '-c', 'user.name=Fixture',
                                '-c', 'user.email=fixture@example.invalid', 'commit',
                                '--no-gpg-sign', '-m', value], check=True, capture_output=True)
                if value == 'pinned':
                    pin = subprocess.check_output(
                        ['git', '-C', str(origin), 'rev-parse', 'HEAD'], text=True).strip()
            dependency = root / 'dependency'
            definition = {'repository': origin.as_uri(), 'commit': pin}
            fetch.checkout(dependency, definition)
            self.assertEqual((dependency / 'version.txt').read_text(encoding='utf-8'), 'pinned')
            fetch.checkout(dependency, definition)
            self.assertEqual((dependency / 'version.txt').read_text(encoding='utf-8'), 'pinned')

    def test_existing_non_repository_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'keep.txt'
            data.write_text('keep', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Not a dedicated'):
                fetch.checkout(root, {'repository': 'https://example.invalid/repo.git'})
            self.assertEqual(data.read_text(encoding='utf-8'), 'keep')

    def test_wrong_remote_is_not_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(['git', 'init', str(root)], check=True, capture_output=True)
            subprocess.run(['git', '-C', str(root), 'remote', 'add', 'origin', 'local-original'],
                           check=True)
            with self.assertRaisesRegex(ValueError, 'Unexpected repository'):
                fetch.checkout(root, {'repository': 'https://example.invalid/repo.git'})
            result = subprocess.check_output(
                ['git', '-C', str(root), 'remote', 'get-url', 'origin'], text=True).strip()
            self.assertEqual(result, 'local-original')

    def test_local_edits_are_not_reset_or_fetched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            remote = 'https://example.invalid/repo.git'
            subprocess.run(['git', 'init', str(root)], check=True, capture_output=True)
            subprocess.run(['git', '-C', str(root), 'remote', 'add', 'origin', remote], check=True)
            data = root / 'local.txt'
            data.write_text('local edit', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'Modified dependency checkout'):
                fetch.checkout(root, {'repository': remote})
            self.assertEqual(data.read_text(encoding='utf-8'), 'local edit')


if __name__ == '__main__':
    unittest.main()
