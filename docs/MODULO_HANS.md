# Módulo de Hans: autorización y auditoría

## Implementación

- `kernel/access.c`: `check_access` aplica bits rwx para propietario y otros, y permite al UID 0 superar los controles ordinarios.
- `kernel/fs.c`: exige permiso de búsqueda (`x`) en cada directorio recorrido al resolver rutas.
- `kernel/sysfile.c`: valida lectura/escritura en `open`, escritura y búsqueda al crear, permisos del directorio y propiedad al eliminar, y ofrece `auditread` solo a root. Los permisos de un descriptor quedan fijados al abrirlo.
- `kernel/exec.c`: rechaza la ejecución sin `x` y registra el rechazo.
- `kernel/audit.c`: conserva hasta 64 rechazos en memoria, en orden de más antiguo a más reciente, con PID, UID, operación e inode. No registra credenciales; el búfer es circular y protegido con spinlock.
- `user/audit.c`: consulta e imprime el registro mediante la syscall `auditread`.

Las operaciones de consola y tuberías conservan su comportamiento. La auditoría se pierde al reiniciar xv6, según el contrato.

## P4: lectura denegada a otro usuario

**Precondición:** integrar autenticación, UID y metadatos de archivos; iniciar xv6 y poder entrar con las cuentas de prueba. Usar las credenciales acordadas por el equipo; no se guardan aquí.

1. Como `root`, preparar un directorio temporal compartido: `mkdir /tmp` y `chmod 0777 /tmp`.
2. Como `isaac`, crear `/tmp/privado` con `echo secreto > /tmp/privado` y fijar modo `0600` con `chmod 0600 /tmp/privado`.
3. Cerrar sesión e iniciar como `samir`.
4. Intentar `cat /tmp/privado`. La apertura debe fallar en el permiso de lectura del archivo.
5. Iniciar como `root` y ejecutar `audit`. Debe aparecer un evento con el UID de Samir, operación `read` e inode del archivo.

**Esperado:** apertura denegada por el kernel y evento en auditoría.
**Observado:** pendiente de ejecutar sobre la versión integrada. Guardar captura real como `evidencias/hans/P4_lectura_denegada.png`.

## P6: ejecución sin permiso `x`

**Precondición:** integración de UID/login y permisos; usar un binario de prueba accesible por ruta para el usuario y retirar `x` al archivo.

1. Como `root`, ejecutar `chmod 0644 /echo` para retirar los bits `x` al binario de prueba.
2. Iniciar sesión como `isaac` y ejecutar `/echo`. El kernel debe rechazar `exec`.
3. Iniciar sesión como `root` y ejecutar `audit`.

**Esperado:** `exec` falla antes de cargar el ELF y aparece un evento con operación `exec`, UID y inode correspondientes.
**Observado:** pendiente de ejecutar sobre la versión integrada. Guardar captura real como `evidencias/hans/P6_exec_sin_x.png`.

## Estado de verificación

El código está preparado sobre `feature/permisos-archivos`, que ya contiene UID e inodos persistentes. Aún falta integrar la rama de autenticación para hacer la demostración multiusuario. No se afirma que P4/P6 hayan sido aprobadas hasta ejecutar xv6, verificar el rechazo y guardar las capturas.
