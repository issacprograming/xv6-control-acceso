# Módulo de Hans: autorización y auditoría

## Implementación

- `kernel/access.c`: `check_access` aplica bits rwx para propietario y otros, y permite al UID 0 superar los controles ordinarios.
- `kernel/fs.c`: exige permiso de búsqueda (`x`) en cada directorio recorrido al resolver rutas.
- `kernel/sysfile.c`: valida lectura/escritura en `open`, escritura y búsqueda al crear, permisos del directorio y propiedad al eliminar, y ofrece `auditread` solo a root. Los permisos de un descriptor quedan fijados al abrirlo.
- `kernel/exec.c`: rechaza la ejecución sin `x` y registra el rechazo.
- `kernel/audit.c`: conserva hasta 64 rechazos en memoria, en orden de más antiguo a más reciente, con PID, UID, operación e inode. No registra credenciales; el búfer es circular y protegido con spinlock.
- `user/audit.c`: consulta e imprime el registro mediante la syscall `auditread`.

## Política de la integración

- Los procesos ordinarios cargan programas desde disco. El kernel comprueba que el inodo sea un archivo regular y exige permiso `x` antes de leer el ELF. El arranque carga el `init` integrado en el kernel y el proceso de login verificado carga únicamente la shell integrada. Root conserva la lista de programas incorporados durante la compilación, incluido `audit`. Sus ejecutables provienen del kernel y sus modos en disco se aplican solo a la carga desde disco.
- La búsqueda (`x`) y el listado (`r`) son permisos independientes. Al abrir con `O_CREATE`, se vuelve a verificar `x` con el padre bloqueado antes de buscar el nombre. Crear una entrada nueva también exige `w`. `O_TRUNC` exige `w` sobre el archivo. Eliminar exige `w+x` sobre el padre y propiedad del objetivo o UID 0. `link` exige lectura sobre el origen y `w+x` sobre el padre de destino.
- Los dispositivos pasan por los controles ordinarios de apertura y `mknod` exige UID 0. El gestor de sesiones de confianza tiene una excepción para buscar y abrir `/console` durante el arranque. La apertura sin permisos exige `O_RDWR` y el dispositivo de consola de root con menor 0. Los descriptores heredados de consola siguen sujetos a la comprobación de sesión en cada lectura y escritura. Los descriptores ordinarios conservan los permisos obtenidos al abrirlos, y las tuberías conservan su comportamiento.
- El registro se inicializa una sola vez durante el arranque y se pierde al reiniciar. El campo inodo vale 0 cuando se rechaza `mknod` o una consulta de auditoría antes de resolver un inodo. La política de sesiones de la consola se comprueba en el controlador y es independiente de la auditoría de archivos.
- La ABI de auditoría se comparte mediante `kernel/audit.h`. La numeración final es `getuid=23`, `startsession=24`, `readsecret=25`, `authenticate=26`, `chmod=27` y `auditread=28`.

## P4: lectura denegada a otro usuario

**Precondición:** integrar autenticación, UID y metadatos de archivos; iniciar xv6 y poder entrar con las cuentas de prueba. Usar las credenciales acordadas por el equipo; no se guardan aquí.

1. Como `root`, preparar un directorio temporal compartido: `mkdir /tmp` y `chmod 0777 /tmp`.
2. Como `isaac`, crear `/tmp/privado` con `echo secreto > /tmp/privado` y fijar modo `0600` con `chmod 0600 /tmp/privado`.
3. Cerrar sesión e iniciar como `samir`.
4. Intentar `cat /tmp/privado`. La apertura debe fallar en el permiso de lectura del archivo.
5. Iniciar como `root` y ejecutar `audit`. Debe aparecer un evento con el UID de Samir, operación `read` e inode del archivo.

**Esperado:** apertura denegada por el kernel y evento en auditoría.
**Observado en la integración:** Samir recibió un rechazo de lectura sobre un archivo de Isaac con modo `0600` en un directorio compartido. El evento `read` apareció al consultar el registro como root.

## P6: ejecución sin permiso `x`

**Precondición:** integración de UID/login y permisos; usar un binario de prueba accesible por ruta para el usuario y retirar `x` al archivo.

1. Como `root`, ejecutar `chmod 0644 /echo` para retirar los bits `x` al binario de prueba.
2. Iniciar sesión como `isaac` y ejecutar `/echo`. El kernel debe rechazar `exec`.
3. Iniciar sesión como `root` y ejecutar `audit`.

**Esperado:** `exec` falla antes de cargar el ELF y aparece un evento con operación `exec`, UID y inode correspondientes.
**Observado en la integración:** Samir recibió un rechazo de ejecución de `/echo` con modo `0644`. Root consultó el evento `exec` y pudo ejecutar su copia incorporada.

## Estado de verificación

La integración se probó en QEMU con sesiones autenticadas. Tras 66 denegaciones consecutivas, la consulta de root devolvió los 64 eventos más recientes en orden cronológico. Un proceso de la sesión anterior conservó sus descriptores de consola y recibió `-1` al intentar leer y escribir durante la sesión siguiente.
