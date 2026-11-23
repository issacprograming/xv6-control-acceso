#!/usr/bin/env python3
"""Show real QEMU results in a terminal and pause for screenshots."""
from run_samir import VM, ROOT
import shutil

shutil.copyfile(ROOT / 'fs.img', '/tmp/samir-visual.img')
vm = VM('/tmp/samir-visual.img')
def show(title, uid, commands):
    print('\033[2J\033[H', end='')
    print('xv6-riscv | Modulo de Samir | Ejecucion REAL en QEMU')
    print(title)
    print('Sesion autenticada en xv6.')
    vm.login(uid)
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
    vm.login(0)
    print(vm.command('permtest p5-root'),end='')
    input('\n[Capturar P5 root; Enter continua] ')
    show('P7a - el propietario puede eliminar su archivo',1002,
         ['permtest p7-setup','permtest p7-owner','permtest p7-setup'])
    show('P7b - integracion: eliminacion ajena denegada',1001,
         ['permtest p7-other'])
    vm.login(0)
    print(vm.command('sync'),end='')
    vm.close()
    vm = VM('/tmp/samir-visual.img')
    print('\033[2J\033[H',end='')
    print('P8b - reinicio REAL de QEMU, misma imagen; propietario y modo persistentes')
    vm.login(1002)
    print(vm.history,end='')
    print(vm.command('permtest persist'),end='')
    print(vm.command('ls -l /home/samir/p3.txt'),end='')
    input('\n[Capturar persistencia; Enter finaliza] ')
finally:
    vm.close()
