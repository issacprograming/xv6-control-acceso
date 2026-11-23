# Propietarios y permisos de archivos

Responsable del módulo: Samir Bryan Cordero Garcia (`sam-dx`).
Los cuatro commits originales de `feature/permisos-archivos` se conservaron.
La validación final parte de `baab949` de `integration/proyecto-final`, que
integra la autenticación de Isaac y la autorización y auditoría de Hans.
La rama de cierre es `integration/cierre-samir`.

## Resultado final

P1-P8 están aprobadas en QEMU con sesiones autenticadas. Pasaron las 24
comprobaciones de `run_integrated.py` y las 21 etapas de `run_samir.py`, incluida
la suite original `usertests -q` con `ALL TESTS PASSED`. Se usaron tres harts,
GCC RISC-V 14.2.0 y QEMU 10.0.13 en Debian 13.

P7 ya no depende de una integración pendiente: el kernel rechaza la eliminación
ajena tanto por los permisos de un directorio privado como por la propiedad del
objetivo bajo un padre 0777. La prueba verifica que el archivo continúa intacto
y que el propietario sí puede eliminarlo. También se probaron padres sin w y
sin x. El fallo de la primera rama modular se conserva como antecedente.

## Implementación

- `struct dinode`, `struct inode` y `struct stat`: `short uid` y `short mode`.
- `ialloc`: asigna el UID del creador y modo 0644 a archivos, 0755 a directorios.
- `ilock`, `iupdate` y `stati`: cargan, persisten y exponen los metadatos.
- `sys_chmod`: valida modo y ruta; bloquea el inodo; autoriza dueño o root;
  actualiza mediante el journal. Los rechazos no cambian modo ni propietario.
- `user/chmod.c`: tres dígitos octales o cuatro con cero inicial.
- `ls -l`: seis bits rwx, UID, octal, número de inodo y tamaño; ls sin opciones
  conserva su salida anterior.

El inodo original de 64 bytes pasaría a 68 al agregar los campos, sin dividir
el bloque de 1024. Se usa un inodo de 128 bytes con quince enteros reservados:
ocho inodos por bloque, manteniendo NDIRECT=12 y el tamaño máximo de archivo.
FSMAGIC cambió de 0x10203040 a 0x10203041 para rechazar imágenes antiguas.
No existe migración automática. NINODES=200 requiere 26 bloques de inodos.

`mkfs` crea / y /home con UID 0 y 0755; los hogares de Isaac y Samir con UID
1001/1002 y 0700; los programas con 0755 y README con 0644. La inicialización
especial de /console para el gestor de sesiones forma parte de la integración.

## Convención de modos y ABI final

Se conservan seis bits efectivos: `(mode >> 6) & 7` para dueño y `mode & 7`
para otros. No hay grupos. El contrato incluye 0644, 0755 y 0777 pese a pedir
el dígito central cero. Se admite que ese dígito sea cero o repita el de otros;
no interviene en la autorización. Se rechazan 0670, negativos y bits especiales.

| Llamada | Número final | Responsable |
| --- | --- | --- |
| getuid | 23 | Isaac |
| startsession | 24 | Isaac |
| readsecret | 25 | Isaac |
| authenticate | 26 | Isaac |
| chmod | 27 | Samir |
| auditread | 28 | Hans |

chmod era 24 en la primera rama modular. Se renumeró al integrar las llamadas
de autenticación. `startsession` es una interfaz interna del gestor: retorna
un PID/0 al crear la sesión y `STARTSESSION_DENIED` (-2) al rechazar un proceso
ordinario. La primera aserción adicional esperaba -1; se corrigió el test para
usar la constante real, sin modificar ni debilitar el kernel.

## Identidad y autorización integradas

La validación final usa login y contraseñas educativas predefinidas de
`kernel/auth.c`. No se fijan UID por GDB. El gestor de sesiones es un proceso
sin identidad autenticada; solo su flujo restringido crea el login. fork hereda
UID/sesión; exec conserva identidad. Un proceso autenticado no puede usar
`authenticate` para convertirse en root ni iniciar otra sesión privilegiada.

Hans aplica r/w/x a open, ejecución, recorrido y modificación de directorios.
La eliminación exige w+x del padre y propiedad del objetivo salvo root. Los
descriptores ya abiertos conservan capacidades tras chmod; las nuevas aperturas
sí observan el modo nuevo. La auditoría registra PID, UID, operación e inodo,
se limita a 64 eventos y solo root la consulta.

El gestor y root tienen programas incorporados al kernel. Los programas de
usuarios normales se cargan del disco y pasan los controles de ejecución.
Es una decisión educativa particular, no una infraestructura de producción.

## Reproducir desde una imagen nueva de prueba

Dependencias: GCC RISC-V y binutils, GCC del host, QEMU >=7.2, Make, Perl,
Python 3 y bc. `make clean` borra la imagen: usarlo al preparar una nueva prueba,
no entre los arranques que deben demostrar persistencia.

```sh
make clean
make -j4 kernel/kernel fs.img
python3 tests/test_mkfs.py
python3 tests/run_integrated.py
python3 tests/run_samir.py --output evidencias/samir/integracion
```

`run_integrated.py` copia fs.img a /tmp antes de probar. `run_samir.py` utiliza
la imagen indicada por `--image` (fs.img por defecto). Ambos reinician QEMU sin
regenerar la imagen para P8b. P7 negativa forma parte de las comprobaciones
obligatorias: un fallo de P7 hace fallar la suite. `XV6_TEST_CPUS` permite elegir
los harts; el valor por defecto verificado es 3.

```sh
make qemu
python3 tests/visual_integrated.py
```

La demostración visual ejecuta de nuevo los casos reales y pausa para capturas.
P1-P2, P3, P4, P5, P6, P7, auditoría y persistencia tienen imágenes finales.

## Evidencias

- `evidencias/integracion-final/`: 24 comprobaciones, dos sesiones, PNG finales,
  datos del entorno e incidencia de la primera expectativa del test.
- `evidencias/samir/integracion/`: 21 etapas y regresión original.
- `evidencias/samir/ejecucion_inicial/`: antecedentes de la rama modular.
- `docs/CIERRE_INTEGRACION.md`: matriz final y alcance.

El informe se actualiza en el Word compartido y no se incluye en Git, de
acuerdo con el contrato del equipo. La revisión y publicación de la rama de
cierre deben conservar los commits de los tres integrantes.

## Referencias

Cox, R., Kaashoek, F., & Morris, R. (2022). *xv6: A simple, Unix-like teaching
operating system* (Rev. 3). MIT.
https://pdos.csail.mit.edu/6.S081/2022/xv6/book-riscv-rev3.pdf

MIT PDOS. (s. f.). *xv6-riscv* [Código fuente]. GitHub.
https://github.com/mit-pdos/xv6-riscv
