#!/usr/bin/env python3
"""Show real QEMU results in a terminal and pause for screenshots."""
from run_samir import VM, ROOT
import shutil
import subprocess

shutil.copyfile(ROOT / 'fs.img', '/tmp/samir-visual.img')
subprocess.run(['gcc', '-I.', 'tests/proc_layout.c', '-o', '/tmp/xv6-proc-layout'], cwd=ROOT, check=True)
layout = tuple(map(int, subprocess.check_output(['/tmp/xv6-proc-layout']).split()))
symbols = subprocess.check_output(['riscv64-linux-gnu-nm', 'kernel/kernel'], cwd=ROOT, text=True)
address = next(int(l.split()[0], 16) for l in symbols.splitlines() if l.split()[-1] == 'proc')
vm = VM('/tmp/samir-visual.img')
def show(title, uid, commands):
    print('\033[2J\033[H', end='')
    print('xv6-riscv | Modulo de Samir | Ejecucion REAL en QEMU')
    print(title)
    print('Identidad de la shell fijada por GDB del host; login pendiente.')
    vm.identity(uid, address, layout)
    for command in commands:
        print(vm.command(command), end='')
    input('\n[Capturar evidencia; Enter continua] ')
try:
    show('P3 - propietario: crear, escribir, leer y consultar metadatos',1002,
         ['permtest p3', 'ls -l /home/samir/p3.txt'])
    show('P5a - propietario: cambios validos y entradas rechazadas',1002,
         ['permtest p5-owner'])
    show('P5b - usuario ajeno: chmod rechazado; root verifica y cambia',1001,
         ['permtest p5-other'])
    vm.identity(0,address,layout)
    print(vm.command('permtest p5-root'),end='')
    input('\n[Capturar P5 root; Enter continua] ')
    show('P7a - el propietario puede eliminar su archivo',1002,
         ['permtest p7-setup','permtest p7-owner','permtest p7-setup'])
    show('P7b - FALLO INTEGRACION: falta autorizacion de Hans',1001,
         ['permtest p7-other'])
    vm.identity(0,address,layout)
    print(vm.command('sync'),end='')
    vm.close()
    vm = VM('/tmp/samir-visual.img')
    print('\033[2J\033[H',end='')
    print('P8b - reinicio REAL de QEMU, misma imagen; propietario y modo persistentes')
    print(vm.history,end='')
    print(vm.command('permtest persist'),end='')
    print(vm.command('ls -l /home/samir/p3.txt'),end='')
    input('\n[Capturar persistencia; Enter finaliza] ')
finally:
    vm.close()
