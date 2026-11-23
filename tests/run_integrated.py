#!/usr/bin/env python3
"""P1-P8 via authenticated sessions, audit and descriptor semantics, 3 harts."""
from run_samir import VM, ROOT
from pathlib import Path
import json
import re
import shutil

OUT = ROOT / 'evidencias/integracion-final'
OUT.mkdir(parents=True, exist_ok=True)
shutil.copyfile(ROOT / 'fs.img', '/tmp/xv6-acceptance.img')
records = []
vm = VM('/tmp/xv6-acceptance.img')

def record(case, output, expected):
    passed = expected in output and 'FAIL:' not in output
    records.append(dict(case=case, passed=passed, output=output))
    print(case, 'PASS' if passed else 'FAIL', flush=True)
    assert passed, output
    return output

def command(case, cmd, expected):
    return record(case, vm.command(cmd), expected)

try:
    start = len(vm.history)
    vm.login(0, reject=True)
    record('P2_login_invalido', vm.history[start:], 'Credenciales invalidas')
    vm.login(1002)
    command('P1_login_samir', 'whoami', 'UID 1002')
    command('P8a_uid_samir', 'uidtest', 'uidtest: OK')
    command('P3_propietario', 'permtest p3', 'DONE: p3')
    command('descriptor_abierto', 'accesstest fd', 'DONE: fd')
    command('sin_escalada', 'accesstest auth', 'DONE: auth')
    command('P5_propietario', 'permtest p5-owner', 'DONE: p5-owner')
    vm.login(1001)
    command('P5_ajeno', 'permtest p5-other', 'DONE: p5-other')
    vm.login(0)
    command('P5_root', 'permtest p5-root', 'DONE: p5-root')
    vm.command('mkdir /tmp')
    vm.command('chmod 777 /tmp')
    vm.login(1001)
    command('preparar_P4_P7', 'accesstest setup', 'DONE: setup')
    vm.login(0)
    vm.command('chmod 644 /echo')
    vm.login(1002)
    command('P4_lectura_denegada', 'accesstest p4', 'DONE: p4')
    command('P6_exec_denegado', 'accesstest p6', 'DONE: p6')
    command('P7_propiedad_en_directorio_compartido', 'accesstest p7-other', 'DONE: p7-other')
    vm.login(1001)
    command('P7_archivo_conservado_y_eliminacion_propia', 'accesstest p7-owner', 'DONE: p7-owner')
    vm.login(1002)
    command('P7_creacion', 'permtest p7-setup', 'DONE: p7-setup')
    command('P7_reglas_directorio', 'accesstest p7-parent', 'DONE: p7-parent')
    vm.login(1001)
    command('P7_directorio_privado', 'permtest p7-other', 'DONE: p7-other')
    vm.login(1002)
    command('P7_eliminacion_dueno', 'permtest p7-owner', 'DONE: p7-owner')
    vm.login(0)
    command('auditoria_P4_P6_P7', 'accesstest audit', 'DONE: audit')
    command('consulta_audit', 'audit', 'op=exec')
    vm.command('chmod 755 /echo')
    vm.login(1002)
    output = command('llenado_auditoria', 'accesstest ring-fill', 'DONE: ring-fill')
    pid = re.search(r'RING_PID=(\d+)', output).group(1)
    vm.login(0)
    command('auditoria_circular_64', 'accesstest ring-check ' + pid, 'DONE: ring-check')
    vm.command('sync')
finally:
    (OUT / 'sesion_1.txt').write_text(vm.history)
    vm.close()
    (OUT / 'resultados.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')

vm = VM('/tmp/xv6-acceptance.img')
try:
    vm.login(1002)
    command('P8b_persistencia_metadatos_y_contenido', 'permtest persist', 'DONE: persist')
    command('P8a_uid_tras_reinicio', 'uidtest', 'uidtest: OK')
finally:
    (OUT / 'sesion_2.txt').write_text(vm.history)
    vm.close()
    (OUT / 'resultados.json').write_text(json.dumps(records, ensure_ascii=False, indent=2)+'\n')
print('ALL INTEGRATED ACCEPTANCE TESTS PASSED', flush=True)
