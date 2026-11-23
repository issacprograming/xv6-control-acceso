#!/usr/bin/env python3
"""Run the module tests in real QEMU sessions on a throwaway disk image."""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
ACCOUNTS = {0: 'root', 1001: 'isaac', 1002: 'samir'}


class VM:
    def __init__(self, image):
        self.p = subprocess.Popen([
            'qemu-system-riscv64', '-machine', 'virt', '-bios', 'none',
            '-kernel', 'kernel/kernel', '-m', '128M', '-smp', os.environ.get('XV6_TEST_CPUS', '3'), '-nographic',
            '-monitor', 'none', '-global', 'virtio-mmio.force-legacy=false',
            '-drive', f'file={image},if=none,format=raw,id=x0',
            '-device', 'virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0',
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=ROOT)
        self.history = ''
        self.uid = None
        try:
            self.until(b'Usuario: ')
        except Exception:
            self.close()
            raise

    def until(self, marker, timeout=45):
        data = b''
        deadline = time.monotonic() + timeout
        while marker not in data:
            if time.monotonic() > deadline:
                raise RuntimeError(f'Timeout waiting for {marker!r}: {data[-2000:]!r}')
            if select.select([self.p.stdout], [], [], 0.1)[0]:
                b = os.read(self.p.stdout.fileno(), 65536)
                if not b:
                    raise RuntimeError(data.decode(errors='replace'))
                data += b
        text = data.decode(errors='replace').replace('\r', '')
        self.history += text
        return text

    def send(self, line):
        self.p.stdin.write((line + '\n').encode())
        self.p.stdin.flush()

    def command(self, command, timeout=45):
        self.send(command)
        return self.until(b'$ ', timeout)

    def login(self, uid, reject=False):
        if self.uid is not None:
            self.p.stdin.write(b'\x04')  # EOF at the shell prompt
            self.p.stdin.flush()
            self.until(b'Usuario: ')
            self.uid = None
        name = ACCOUNTS[uid]
        self.send(name)
        self.until(b'Contrasena: ')
        password = 'wrong-test-password' if reject else f'{name}-xv6-test'
        self.send(password)
        if reject:
            result = self.until(b'Usuario: ')
            assert 'Credenciales invalidas' in result, result
        else:
            result = self.until(b'$ ')
            self.uid = uid
        assert password not in result, 'Password echoed on the console'

    def close(self):
        self.p.terminate()
        try:
            self.p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', default='fs.img')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    records = []
    vm = VM(args.image)
    def stage(command, expect, label, timeout=45):
        result = vm.command(command, timeout)
        ok = expect in result and ('FAIL:' not in result if expect.startswith('DONE:') else True)
        records.append({'case': label, 'command': command, 'passed': ok, 'output': result})
        print(label, 'PASS' if ok else 'FAIL', flush=True)
        return ok
    try:
        vm.login(0, reject=True)
        vm.login(1002)
        assert stage('whoami', 'UID 1002', 'Samir_whoami')
        assert stage('uidtest', 'uidtest: OK', 'Samir_uidtest')
        assert stage('permtest p3', 'DONE: p3', 'P3')
        assert stage('permtest p5-owner', 'DONE: p5-owner', 'P5_owner')
        vm.login(1001)
        assert stage('whoami', 'UID 1001', 'Isaac_whoami')
        assert stage('uidtest', 'uidtest: OK', 'Isaac_uidtest')
        assert stage('permtest p5-other', 'DONE: p5-other', 'P5_non_owner')
        vm.login(0)
        assert stage('whoami', 'UID 0', 'root_whoami')
        assert stage('uidtest', 'uidtest: OK', 'root_uidtest')
        assert stage('ls -l /chmod', 'mode=0755', 'executable_metadata')
        assert stage('permtest p5-root', 'DONE: p5-root', 'P5_root')
        assert stage('chmod 644 /home/samir/p3.txt', '$ ', 'chmod_command')
        assert stage('ls -l /home/samir/p3.txt', 'mode=0644', 'ls_metadata')
        vm.command('chmod 600 /home/samir/p3.txt')
        assert stage('ls -l /home/samir/p3.txt', 'mode=0600', 'ls_private')
        vm.login(1002)
        assert stage('permtest p7-setup', 'DONE: p7-setup', 'P7_setup')
        assert stage('permtest p7-owner', 'DONE: p7-owner', 'P7_owner')
        assert stage('permtest p7-setup', 'DONE: p7-setup', 'P7_recreate')
        vm.login(1001)
        p7 = stage('permtest p7-other', 'DONE: p7-other', 'P7_integration')
        if p7:
            vm.login(1002)
            stage('permtest p7-owner', 'DONE: p7-owner', 'P7_owner')
        # Explicitly check sync before restarting the same disk.
        vm.command('sync')
        (args.output / 'sesion_1.txt').write_text(vm.history)
    finally:
        vm.close()
    vm = VM(args.image)
    try:
        vm.login(1002)
        assert stage('permtest persist', 'DONE: persist', 'P8b_persistence')
        vm.login(0)
        stage('usertests -q', 'ALL TESTS PASSED', 'upstream_regression', timeout=240)
        (args.output / 'sesion_2.txt').write_text(vm.history)
    finally:
        (args.output / 'sesion_2.txt').write_text(vm.history)
        vm.close()
        (args.output / 'resultados.json').write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n')
    required = records
    if not all(r['passed'] for r in required):
        raise SystemExit(1)
    print('P7: ' + ('PASS' if p7 else 'FAIL'))

if __name__ == '__main__':
    main()
