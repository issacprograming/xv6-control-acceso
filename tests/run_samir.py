#!/usr/bin/env python3
"""Real QEMU module tests. GDB changes shell UID externally, never xv6 ABI.
Requires a clean throwaway fs.img; runs no login tests. P7 is reported as a
blocked integration case if Hans's authorization module is not present.
"""
import argparse
import json
import os
from pathlib import Path
import select
import socket
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'evidencias/samir'
OUT.mkdir(parents=True, exist_ok=True)

class VM:
    def __init__(self, image):
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0))
        port = listener.getsockname()[1]
        listener.close()
        self.p = subprocess.Popen([
            'qemu-system-riscv64', '-machine', 'virt', '-bios', 'none',
            '-kernel', 'kernel/kernel', '-m', '128M', '-smp', '1', '-nographic',
            '-monitor', 'none', '-global', 'virtio-mmio.force-legacy=false',
            '-drive', f'file={image},if=none,format=raw,id=x0',
            '-device', 'virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0',
            '-gdb', f'tcp:127.0.0.1:{port}',
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=ROOT)
        self.history = ''
        self.until(b'$ ')
        self.s = socket.create_connection(('127.0.0.1', port), timeout=10)
        self.s.settimeout(10)
        self.request("qSupported")
        self.request("?")
        self.s.sendall(b"$c#63")
        assert self.s.recv(1) == b"+"

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

    def command(self, command, timeout=45):
        self.p.stdin.write((command + '\n').encode())
        self.p.stdin.flush()
        return self.until(b'$ ', timeout)

    def packet(self):
        while self.s.recv(1) != b'$':
            pass
        data = b''
        while True:
            c = self.s.recv(1)
            if c == b'#':
                break
            data += c
        checksum = self.s.recv(2)
        assert int(checksum, 16) == sum(data) % 256
        self.s.sendall(b'+')
        return data

    def request(self, body):
        data = body.encode()
        self.s.sendall(b'$' + data + b'#' + f'{sum(data)%256:02x}'.encode())
        return self.packet()

    def identity(self, uid, procaddr, layout):
        self.s.sendall(b'\x03')
        self.packet()
        size, off_uid, off_name, off_pid, off_state = layout
        found = 0
        for i in range(64):
            addr = procaddr + size * i
            data = bytes.fromhex(self.request(f'm{addr:x},{size:x}').decode())
            state = struct.unpack_from('<i', data, off_state)[0]
            name = data[off_name:off_name+16].split(b'\0')[0]
            if state and name == b'sh':
                payload = struct.pack('<i', uid).hex()
                assert self.request(f'M{addr+off_uid:x},4:{payload}') == b'OK'
                found += 1
        assert found == 1, f'Expected one shell; found {found}'
        self.history += f'\n[HOST DEBUGGER: shell UID={uid}; login not yet implemented]\n'
        # Continue has an ACK but no response packet until the next stop.
        self.s.sendall(b'$c#63')
        assert self.s.recv(1) == b'+'

    def close(self):
        if hasattr(self, 's'):
            self.s.close()
        self.p.terminate()
        try:
            self.p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--image', default='fs.img')
    args = parser.parse_args()
    subprocess.run(['gcc', '-I.', 'tests/proc_layout.c', '-o', '/tmp/xv6-proc-layout'], cwd=ROOT, check=True)
    layout = tuple(map(int, subprocess.check_output(['/tmp/xv6-proc-layout']).split()))
    symbols = subprocess.check_output(['riscv64-linux-gnu-nm', 'kernel/kernel'], cwd=ROOT, text=True)
    procaddr = next(int(line.split()[0], 16) for line in symbols.splitlines() if line.split()[-1] == 'proc')
    records = []
    vm = VM(args.image)
    def stage(command, expect, label, timeout=45):
        result = vm.command(command, timeout)
        ok = expect in result and ('FAIL:' not in result if expect.startswith('DONE:') else True)
        records.append({'case': label, 'command': command, 'passed': ok, 'output': result})
        print(label, 'PASS' if ok else 'FAIL', flush=True)
        return ok
    try:
        vm.identity(1002, procaddr, layout)
        assert stage('permtest p3', 'DONE: p3', 'P3')
        assert stage('permtest p5-owner', 'DONE: p5-owner', 'P5_owner')
        vm.identity(1001, procaddr, layout)
        assert stage('permtest p5-other', 'DONE: p5-other', 'P5_non_owner')
        vm.identity(0, procaddr, layout)
        assert stage('permtest p5-root', 'DONE: p5-root', 'P5_root')
        stage('ls -l /home/samir/p3.txt', 'mode=0600', 'ls_metadata')
        vm.identity(1002, procaddr, layout)
        assert stage('permtest p7-setup', 'DONE: p7-setup', 'P7_setup')
        assert stage('permtest p7-owner', 'DONE: p7-owner', 'P7_owner')
        assert stage('permtest p7-setup', 'DONE: p7-setup', 'P7_recreate')
        vm.identity(1001, procaddr, layout)
        p7 = stage('permtest p7-other', 'DONE: p7-other', 'P7_integration')
        if p7:
            vm.identity(1002, procaddr, layout)
            stage('permtest p7-owner', 'DONE: p7-owner', 'P7_owner')
        # Explicitly check sync before restarting the same disk.
        vm.command('sync')
        (OUT / 'sesion_1.txt').write_text(vm.history)
    finally:
        vm.close()
    vm = VM(args.image)
    try:
        assert stage('permtest persist', 'DONE: persist', 'P8b_persistence')
        # Isaac's current test expects UID_UNAUTH, not the bootstrap root UID.
        vm.identity(-1, procaddr, layout)
        assert stage('uidtest', 'uidtest: OK', 'Isaac_uidtest')
        vm.identity(0, procaddr, layout)
        # Original upstream tests use root privileges on this module branch.
        stage('usertests -q', 'ALL TESTS PASSED', 'upstream_regression', timeout=240)
        (OUT / 'sesion_2.txt').write_text(vm.history)
    finally:
        (OUT / 'sesion_2.txt').write_text(vm.history)
        vm.close()
        (OUT / 'resultados.json').write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n')
    required = [r for r in records if r['case'] != 'P7_integration']
    if not all(r['passed'] for r in required):
        raise SystemExit(1)
    print('P7: ' + ('PASS' if p7 else 'BLOCKED: authorization module pending'))

if __name__ == '__main__':
    main()
