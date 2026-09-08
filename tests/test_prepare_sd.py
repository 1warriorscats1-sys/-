"""Public tests: synthesized FORM files only; no commercial fixtures or downloads."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import prepare_sd as prep
import audit_distribution as audit


def fixture(name='SANAE_Sylphid_Breeze', group=2):
    """Minimal validator fixture, NOT playable game data or a converter fixture."""
    sections = [(b'GEN8', bytearray(44)), (b'CODE', bytearray(4)),
                (b'FUNC', bytearray(4)), (b'VARI', bytearray(4)),
                (b'ROOM', bytearray(4+104*4)), (b'AGRP', bytearray(4+19*4)),
                (b'SPRT', bytearray(4+1568*4)), (b'SOND', bytearray(8)),
                (b'TGIN', bytearray(4)), (b'STRG', bytearray(4))]
    data = bytearray(b'FORM\0\0\0\0'); loc = {}
    for key, blob in sections:
        data += key + struct.pack('<I', len(blob))
        loc[key] = len(data)
        data += blob
    tail = len(data)
    data += b'TEST\0\0\0\0'
    def alloc(blob):
        offset = len(data); data.extend(blob); return offset
    def put(at, value):
        struct.pack_into('<I', data, at, value)
    text = name.encode()
    nameptr = alloc(struct.pack('<I', len(text))+text+b'\0')+4
    room = alloc(bytes(96))
    agrp = alloc(struct.pack('<I', nameptr))
    sound = alloc(bytes(40)); put(sound+28, group)
    data[loc[b'GEN8']+1] = 17
    put(loc[b'GEN8']+40, nameptr)
    for key, count, pointer in [(b'ROOM',104,room), (b'AGRP',19,agrp),
                                (b'SPRT',1568,0), (b'SOND',1,sound)]:
        put(loc[key], count)
        for i in range(count): put(loc[key]+4+4*i, pointer)
    put(tail+4, len(data)-tail-8)
    put(4, len(data)-8)
    return bytes(data)


def audio():
    return b'FORM'+struct.pack('<I', 12)+b'AUDO'+struct.pack('<II', 4, 0)


class ReaderTests(unittest.TestCase):
    def test_valid_identity_and_required_audio(self):
        self.assertEqual(prep.GameData(fixture()).validate_sanae(), {2})

    def test_bad_magic(self):
        with self.assertRaises(prep.InputError): prep.GameData(b'MZnot-a-game')

    def test_truncated_header(self):
        for d in [b'', b'FORM', b'FORM\0\0']:
            with self.assertRaises(prep.InputError): prep.GameData(d)

    def test_trailing_data(self):
        with self.assertRaises(prep.InputError): prep.GameData(fixture()+b'extra')

    def test_chunk_out_of_bounds(self):
        d = bytearray(fixture()); struct.pack_into('<I', d, 12, len(d)*2)
        with self.assertRaises(prep.InputError): prep.GameData(d)

    def test_duplicate_chunk(self):
        d = bytearray(fixture()); d += b'GEN8\0\0\0\0'; struct.pack_into('<I', d, 4, len(d)-8)
        with self.assertRaises(prep.InputError): prep.GameData(d)

    def test_wrong_game(self):
        with self.assertRaises(prep.InputError): prep.GameData(fixture('AnotherGame')).validate_sanae()

    def test_wrong_bytecode(self):
        d = bytearray(fixture()); d[17] = 16
        with self.assertRaises(prep.InputError): prep.GameData(d).validate_sanae()

    def test_bad_sound_group(self):
        with self.assertRaises(prep.InputError): prep.GameData(fixture(group=99)).validate_sanae()

    def test_bad_string_pointer(self):
        d = bytearray(fixture()); struct.pack_into('<I', d, 16+40, len(d)+20)
        with self.assertRaises(prep.InputError): prep.GameData(d).validate_sanae()

    def test_wrong_revision(self):
        d = bytearray(fixture()); g = prep.GameData(d)
        struct.pack_into('<I', d, g.chunks[b'ROOM'][0], 103)
        with self.assertRaises(prep.InputError): prep.GameData(d).validate_sanae()

    def test_pointer_count_out_of_chunk(self):
        d = bytearray(fixture()); g = prep.GameData(d)
        struct.pack_into('<I', d, g.chunks[b'ROOM'][0], 0xffffffff)
        with self.assertRaises(prep.InputError): prep.GameData(d).validate_sanae()


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root/'Steam folder with spaces и кириллица'; self.game.mkdir()
        (self.game/'data.win').write_bytes(fixture())
        (self.game/'audiogroup2.dat').write_bytes(audio())
        self.out = self.root/'new pack'
        self.steps = []

    def fake_step(self, script, *args):
        self.steps.append(script)
        if script in ['convert_data.py', 'patch_steam.py']:
            Path(args[1]).write_bytes(Path(args[0]).read_bytes())
        return 'synthetic pipeline step '+str(args[0])+'\n'

    def prepare(self):
        with patch.object(prep, 'run_step', side_effect=self.fake_step):
            return prep.prepare(self.game, self.out)

    def test_pipeline_order_layout_hashes_and_privacy(self):
        original = (self.game/'data.win').read_bytes()
        self.assertEqual(self.prepare(), self.out)
        self.assertEqual(self.steps, ['inspect_data.py','convert_data.py','patch_steam.py','verify_data.py'])
        manifest = json.loads((self.out/'manifest.json').read_text())
        self.assertFalse(manifest['runtime_included'])
        self.assertTrue(manifest['private_output_do_not_redistribute'])
        for name, digest in manifest['outputs_sha256'].items():
            self.assertEqual(prep.sha256(self.out/name), digest)
        self.assertEqual((self.game/'data.win').read_bytes(), original)
        self.assertNotIn(str(self.root), (self.out/'manifest.json').read_text())
        self.assertNotIn(str(self.root), (self.out/'prepare.log').read_text())
        self.assertFalse(list(self.out.rglob('main')))
        self.assertFalse(list(self.out.rglob('*.nsp')))
        self.assertFalse(list(self.out.rglob('audiogroup1.dat')))

    def test_case_insensitive_input_names(self):
        (self.game/'data.win').rename(self.game/'DATA.WIN')
        (self.game/'audiogroup2.dat').rename(self.game/'AUDIOGROUP2.DAT')
        self.prepare()
        self.assertEqual(len(list(self.out.rglob('game.win'))), 1)
        self.assertEqual(len(list(self.out.rglob('audiogroup2.dat'))), 1)

    def test_optional_audio_copied_when_present(self):
        (self.game/'audiogroup1.dat').write_bytes(audio())
        self.prepare()
        self.assertEqual(len(list(self.out.rglob('audiogroup1.dat'))), 1)

    def test_missing_audio_fails_before_output(self):
        (self.game/'audiogroup2.dat').unlink()
        with self.assertRaisesRegex(prep.InputError, 'Missing audiogroup2'):
            self.prepare()
        self.assertFalse(self.out.exists()); self.assertFalse(self.steps)

    def test_missing_data(self):
        (self.game/'data.win').unlink()
        with self.assertRaisesRegex(prep.InputError, 'Missing data.win'): self.prepare()

    def test_corrupt_audio(self):
        (self.game/'audiogroup2.dat').write_bytes(b'bad archive')
        with self.assertRaisesRegex(prep.InputError, 'audio archive'): self.prepare()

    def test_existing_output_untouched(self):
        self.out.mkdir(); (self.out/'keep').write_text('keep')
        with self.assertRaisesRegex(prep.InputError, 'already exists'): self.prepare()
        self.assertEqual((self.out/'keep').read_text(), 'keep')

    def test_overlapping_source_output(self):
        for out in [self.game, self.game/'result', self.root]:
            with self.assertRaisesRegex(prep.InputError, 'overlap'): prep.prepare(self.game, out)

    def test_failure_atomicity_no_unpatched_fallback(self):
        for failed_step in ['inspect_data.py','convert_data.py','patch_steam.py','verify_data.py']:
            def fail(script, *args):
                if script == failed_step: raise prep.InputError('injected failure')
                return self.fake_step(script, *args)
            with patch.object(prep, 'run_step', side_effect=fail):
                with self.assertRaisesRegex(prep.InputError, 'injected'): prep.prepare(self.game, self.out)
            self.assertFalse(self.out.exists())
            self.assertFalse(list(self.root.glob('.sanae-*')))

    def test_known_converted_is_verified_not_repatched(self):
        with patch.object(prep, 'KNOWN_CONVERTED', prep.sha256(self.game/'data.win')):
            self.prepare()
        self.assertEqual(self.steps, ['verify_data.py'])

    def test_input_mutation_aborts(self):
        def mutate(script, *args):
            result = self.fake_step(script, *args)
            if script == 'verify_data.py': (self.game/'data.win').write_bytes(b'changed')
            return result
        with patch.object(prep, 'run_step', side_effect=mutate):
            with self.assertRaisesRegex(prep.InputError, 'changed'): prep.prepare(self.game, self.out)
        self.assertFalse(self.out.exists())

    def test_subprocess_failure_surfaces_error(self):
        fake = prep.subprocess.CompletedProcess([], 1, 'bad file', 'details')
        with patch.object(prep.subprocess, 'run', return_value=fake):
            with self.assertRaisesRegex(prep.InputError, 'bad file'): prep.run_step('convert_data.py', 'input', 'output')

    def test_cli_bad_folder_exit_code(self):
        with patch('sys.stderr'):
            self.assertEqual(prep.main([str(self.root/'absent')]), 1)


class DistributionTests(unittest.TestCase):
    def test_forbidden_filenames(self):
        for name in ['main', 'sdk', 'subsdk0', 'a/game.win', 'a/TH08.EXE',
                     'output.nsp', 'secret/prod.keys', 'SANAE_exit_fix.zip', 'local-inputs/x.py']:
            self.assertTrue(audit.violations(name, b'text'), name)

    def test_signature_without_extension(self):
        for magic in audit.MAGICS:
            self.assertTrue(audit.violations('innocent.txt', magic+b'payload'))

    def test_text_sources_allowed(self):
        for name in ['README.md', 'scripts/prepare_sd.py', 'tests/test_prepare_sd.py']:
            self.assertFalse(audit.violations(name, b'# source\n'))


if __name__ == '__main__':
    unittest.main()
