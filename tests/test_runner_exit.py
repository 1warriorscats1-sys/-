from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import patch_nso as nso
import runner_exit as ex
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_INTR
from unicorn.arm64_const import *

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT/'build-inputs/runtime-2024.14.3.260/bin/main'


class ExitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.h, cls.f, cls.s, cls.r = nso.load_nso(SRC)
        # Save stub emitted by the original patcher; exit install cannot modify it.
        cls.text = bytearray(cls.r[0])
        cls.commit = len(cls.text)
        from collections import namedtuple
        import re
        image = nso.build_image(cls.s, cls.r)
        save = re.search(rb'(?<=\x00)save\x00', image).start()
        md = nso.capstone.Cs(nso.capstone.CS_ARCH_ARM64, nso.capstone.CS_MODE_ARM)
        I = namedtuple('I', 'address size mnemonic op_str')
        insns = [I(*i) for i in md.disasm_lite(cls.r[0][0x5a000:0x5c004], 0x5a000)]
        nso.add_save_hooks(cls.text, insns, (0x569a10, 0x569970, save, 0x569600, 0xe0d98c))
        cls.saved_text = bytes(cls.text)
        cls.header = bytearray(cls.h)
        cls.raw = list(cls.r)
        cls.meta = ex.install(cls.text, cls.raw, cls.s, cls.header, cls.commit)

    def vm(self, base=0):
        u = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        u.mem_map(base, 0x1200000)
        for i, (_, addr, _) in enumerate(self.s):
            u.mem_write(base+addr, bytes(self.text) if i == 0 else self.raw[i])
        stack = base+0x1300000
        u.mem_map(stack, 0x10000)
        sp = stack+0x8000
        u.reg_write(UC_ARM64_REG_SP, sp)
        return u, sp

    def run_poll(self, pending, result=0, pointer=True, base=0):
        u, sp = self.vm(base)
        u.mem_write(base+self.meta['flag'], bytes([pending]))
        regs = [UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_X2,
                UC_ARM64_REG_X19, UC_ARM64_REG_X29, UC_ARM64_REG_X30]
        values = [71, 72, 73, 74, 75, base+0x1200100]
        for reg, val in zip(regs, values):
            u.reg_write(reg, val)
        events = []
        def hook(uc, addr, size, data):
            if addr == base+0xaf0:
                events.append('normal'); uc.emu_stop()
            elif addr == base+self.commit:
                events.append('commit')
                uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
            elif addr == base+0x5693f0:
                events.append('lookup')
                self.assertEqual(uc.reg_read(UC_ARM64_REG_SP) % 16, 0)
                self.assertEqual(bytes(uc.mem_read(uc.reg_read(UC_ARM64_REG_X1), len(ex.EXIT_NAME))), ex.EXIT_NAME)
                if pointer:
                    uc.mem_write(uc.reg_read(UC_ARM64_REG_X0), struct.pack('<Q', base+0x120))
                uc.reg_write(UC_ARM64_REG_X0, result)
                uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
            elif addr == base+0x120:
                events.append('exit'); uc.emu_stop()
        def intr(uc, number, data):
            pc = uc.reg_read(UC_ARM64_REG_PC)
            self.assertEqual(bytes(uc.mem_read(pc-4, 4)), struct.pack('<I', 0xd40000e1))
            events.append('fallback'); uc.emu_stop()
        u.hook_add(UC_HOOK_CODE, hook)
        u.hook_add(UC_HOOK_INTR, intr)
        u.emu_start(base+self.meta['poll'], base+0x1200100, count=100)
        if not pending:
            self.assertEqual([u.reg_read(r) for r in regs], values)
            self.assertEqual(u.reg_read(UC_ARM64_REG_SP), sp)
        return events

    def test_normal_both_load_bases(self):
        for base in [0, 0x7100000000]:
            self.assertEqual(self.run_poll(0, base=base), ['normal'])

    def test_deferred_exit_both_load_bases(self):
        for base in [0, 0x7100000000]:
            self.assertEqual(self.run_poll(1, base=base), ['commit', 'lookup', 'exit'])

    def test_lookup_failure_exits_without_libc(self):
        self.assertEqual(self.run_poll(1, result=1), ['commit', 'lookup', 'fallback'])

    def test_null_symbol_exits_without_libc(self):
        self.assertEqual(self.run_poll(1, pointer=False), ['commit', 'lookup', 'fallback'])

    def test_request_restores_original_frame(self):
        for base in [0, 0x7100000000]:
            u, sp = self.vm(base)
            end = base+0x1200100
            u.mem_write(sp+0x30, struct.pack('<QQQ', 0x112233, end, 0x445566))
            u.reg_write(UC_ARM64_REG_X19, 123)
            u.emu_start(base+0x375214, end, count=30)
            self.assertEqual(u.reg_read(UC_ARM64_REG_PC), end)
            self.assertEqual(u.reg_read(UC_ARM64_REG_X19), 0x445566)
            self.assertEqual(u.reg_read(UC_ARM64_REG_X29), 0x112233)
            self.assertEqual(u.reg_read(UC_ARM64_REG_SP), sp+0x50)
            self.assertEqual(bytes(u.mem_read(base+self.meta['flag'], 1)), b'\1')

    def test_only_expected_original_text_changed(self):
        changed = {i for i in range(0, len(self.r[0]), 4)
                   if self.text[i:i+4] != self.saved_text[i:i+4]}
        self.assertEqual(changed, {0x375214, 0x1740, 0x1954})
        # game_restart and bookkeeping are untouched.
        self.assertEqual(self.text[0x375230:0x375240], self.r[0][0x375230:0x375240])

    def test_layout(self):
        self.assertLess(len(self.text), self.s[1][1])
        self.assertEqual(self.raw[2], self.r[2])
        self.assertEqual(ex.word(self.header, 0x3c), ex.word(self.h, 0x3c)+4096)
        self.assertEqual(self.meta['flag'] % 4096, 0)
        mod = ex.word(self.r[0], 4)
        self.assertEqual(mod + struct.unpack_from('<i', self.raw[1], mod+12-self.s[1][1])[0], self.meta['flag']+4096)

    def test_bad_instruction_rejected(self):
        for site in [0x1740, 0x1954, 0x375214, 0x5693f4]:
            text = bytearray(self.r[0]); text[site] ^= 1
            with self.assertRaises(ValueError):
                ex.install(text, list(self.r), self.s, bytearray(self.h), self.commit)

    def test_bad_layout_rejected(self):
        header = bytearray(self.h)
        struct.pack_into('<I', header, 0x3c, 0)
        with self.assertRaises(ValueError):
            ex.install(bytearray(self.r[0]), list(self.r), self.s, header, self.commit)

    def test_guarded_commit_mounted_and_unmounted(self):
        for mounted in [0, 1]:
            u, sp = self.vm()
            u.mem_write(0xe0d98c, bytes([mounted]))
            end = 0x1200100
            u.reg_write(UC_ARM64_REG_LR, end)
            events = []
            def hook(uc, addr, size, data):
                if addr == 0x569970:
                    events.append('commit')
                    self.assertEqual(bytes(uc.mem_read(uc.reg_read(UC_ARM64_REG_X0), 5)), b'save\0')
                    uc.reg_write(UC_ARM64_REG_PC, uc.reg_read(UC_ARM64_REG_LR))
            u.hook_add(UC_HOOK_CODE, hook)
            u.emu_start(self.commit, end, count=30)
            self.assertEqual(events, ['commit'] if mounted else [])
            self.assertEqual(u.reg_read(UC_ARM64_REG_PC), end)
            self.assertEqual(u.reg_read(UC_ARM64_REG_SP), sp)
            self.assertEqual(bytes(u.mem_read(0xe0d98c, 1)), bytes([mounted]))

    def test_branch_checks(self):
        for target in [3, 1 << 27, -(1 << 27)-4]:
            with self.assertRaises(ValueError): ex.branch(0, target)


if __name__ == '__main__':
    unittest.main()
