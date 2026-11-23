# xv6 control de acceso

Proyecto de Sistemas Operativos de Isaac, Samir y Hans: autenticación educativa,
UID en procesos, propietarios y permisos persistentes, chmod, autorización en
kernel y auditoría circular.

La versión integrada se validó con login real y tres harts. P1-P8 aprobadas,
45 comprobaciones satisfactorias entre las dos suites, incluida usertests -q.

- [Guía del módulo y reproducción](docs/MODULO_SAMIR.md)
- [Matriz de verificación final](docs/CIERRE_INTEGRACION.md)
- [Autorización y auditoría de Hans](docs/MODULO_HANS.md)
- [Evidencias finales](evidencias/integracion-final/)
- [README original de xv6](README)

```sh
make -j4 kernel/kernel fs.img
python3 tests/test_mkfs.py
python3 tests/run_integrated.py
python3 tests/run_samir.py --output evidencias/samir/integracion
make qemu
```

Requiere GCC RISC-V, binutils, GCC del host, QEMU >=7.2, Make, Perl, Python 3
y bc. Las cuentas y credenciales educativas están en kernel/auth.c. El informe
se mantiene en el Word compartido del equipo y no se versiona en este repositorio.
La imagen anterior al cambio de formato debe regenerarse; no ejecutar make
clean entre arranques usados para demostrar persistencia.
