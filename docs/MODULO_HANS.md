# Módulo de Hans: autorización y auditoría

**Responsable:** Hans  
**Área:** control de acceso, validación de permisos y registro de denegaciones  
**Rama documentada:** `integration/proyecto-final`

## 1. Descripción general

Este módulo aplica las reglas de autorización del sistema de archivos de xv6 y registra los accesos denegados. El kernel valida los permisos al recorrer rutas, abrir o modificar archivos y ejecutar programas. También mantiene un registro temporal de denegaciones que puede consultar únicamente root.

El módulo se integra con la autenticación y los metadatos de propietario y permisos desarrollados por los demás integrantes. La política descrita aquí corresponde a la integración final del proyecto.

## 2. Objetivos

- Aplicar permisos de lectura, escritura y búsqueda/ejecución (`rwx`) según el propietario y el UID del proceso.
- Restringir operaciones sobre archivos, directorios, dispositivos y programas ejecutables.
- Registrar las operaciones denegadas sin almacenar contraseñas ni credenciales.
- Permitir que root consulte los eventos para revisar los rechazos.

## 3. Reglas de autorización

| Operación | Regla aplicada |
|---|---|
| Recorrer una ruta | Se exige permiso `x` en cada directorio recorrido. El permiso `r` para listar entradas es independiente. |
| Abrir para lectura o escritura | Se valida el permiso correspondiente al abrir. Los descriptores ordinarios conservan las capacidades obtenidas en ese momento. |
| Crear con `O_CREATE` | Se comprueba `x` en el directorio padre antes de buscar el nombre; para una entrada nueva también se exige `w` en el padre. |
| Truncar con `O_TRUNC` | Se exige permiso `w` sobre el archivo. |
| Eliminar | Se exige `w+x` en el directorio padre y ser propietario del objetivo o UID 0. |
| Crear un enlace con `link` | Se exige lectura sobre el archivo de origen y `w+x` en el directorio de destino. |
| Ejecutar un archivo desde disco | Se exige que sea un archivo regular y que tenga permiso `x` antes de cargar el ELF. |
| Crear un dispositivo | `mknod` requiere UID 0. Los dispositivos pasan por los controles ordinarios de apertura. |
| Consultar auditoría | `auditread` está disponible únicamente para UID 0. |

El UID 0 supera los controles ordinarios de permisos. Las comprobaciones especiales del gestor de sesiones para la consola se describen en la sección de integración.

## 4. Archivos y funciones principales

| Archivo | Función en el módulo |
|---|---|
| `kernel/access.c` | Implementa `check_access`, que aplica los bits `rwx` para el propietario y los demás usuarios, con la excepción de UID 0. |
| `kernel/fs.c` | Exige permiso `x` en cada directorio al resolver una ruta. |
| `kernel/sysfile.c` | Valida permisos en apertura, creación, truncado, enlaces, eliminación y cambio de directorio; implementa la consulta de auditoría. |
| `kernel/exec.c` | Rechaza la ejecución desde disco si el archivo no es regular o carece de `x`; registra el rechazo. |
| `kernel/audit.c` | Mantiene el búfer circular de eventos de denegación protegido por un spinlock. |
| `kernel/audit.h` | Comparte las definiciones de eventos y la interfaz de auditoría entre componentes del kernel. |
| `user/audit.c` | Invoca `auditread` e imprime los eventos disponibles. |

## 5. Registro de auditoría

El búfer conserva como máximo 64 denegaciones en memoria. Cada evento contiene PID, UID, operación e inode cuando ya se resolvió uno. El búfer es circular: al superar su capacidad, conserva los 64 eventos más recientes y los devuelve en orden cronológico. Si la operación se rechaza antes de resolver un inode —por ejemplo, en `mknod` o una consulta de auditoría— el campo inode puede ser 0.

El registro se inicializa durante el arranque y se pierde al reiniciar xv6. La consulta está restringida a root. No se guardan contraseñas ni credenciales. La auditoría de archivos es independiente del control de sesión aplicado a la consola.

## 6. Interfaz del sistema

La numeración final de las llamadas relacionadas con identidad, sesión y permisos en la integración es:

| Llamada | Número |
|---|---:|
| `getuid` | 23 |
| `startsession` | 24 |
| `readsecret` | 25 |
| `authenticate` | 26 |
| `chmod` | 27 |
| `auditread` | 28 |

La interfaz de auditoría se define de forma compartida en `kernel/audit.h`.

## 7. Integración con autenticación, ejecución y consola

Los procesos ordinarios cargan programas desde disco. Antes de leer el ELF, el kernel verifica que el inode corresponda a un archivo regular y que el usuario tenga permiso `x`. El arranque usa el `init` incorporado en el kernel; el proceso de login verificado carga la shell incorporada. Root mantiene la lista de programas incluidos durante la compilación, entre ellos `audit`. Los modos almacenados en disco se aplican a la carga desde disco.

Los dispositivos están sujetos a las validaciones ordinarias de apertura y solo root puede crearlos con `mknod`. El gestor de sesiones de confianza tiene una excepción limitada para buscar y abrir `/console` durante el arranque. Esta apertura exige `O_RDWR` y el dispositivo de consola de root con menor 0. Los descriptores heredados de consola se vuelven a validar en cada lectura y escritura para impedir que una sesión anterior los use durante la siguiente. Los descriptores ordinarios conservan los permisos obtenidos al abrirlos y las tuberías mantienen su comportamiento.

## 8. Pruebas realizadas

### 8.1 Lectura denegada entre usuarios (P4)

**Procedimiento:** root prepara un directorio compartido con `chmod 0777 /tmp`; Isaac crea `/tmp/privado` con modo `0600`; luego Samir intenta leerlo con `cat`. Finalmente, root ejecuta `audit`.

**Resultado esperado:** la apertura de Samir es rechazada y el registro muestra su UID, la operación `read` y el inode del archivo.

**Resultado observado en la integración:** Samir recibió el rechazo al leer el archivo de Isaac desde el directorio compartido. Root encontró el evento `read` en la auditoría.

### 8.2 Ejecución sin permiso (P6)

**Procedimiento:** root fija el modo de `/echo` en `0644`; Samir intenta ejecutar `/echo`; luego root consulta `audit`.

**Resultado esperado:** `exec` falla antes de cargar el ELF y se registra la operación junto con el UID y el inode.

**Resultado observado en la integración:** se rechazó la ejecución de `/echo` con modo `0644`. Root consultó el evento `exec` y pudo ejecutar la copia incorporada.

### 8.3 Capacidad del búfer y aislamiento de consola

La integración se ejecutó en QEMU con sesiones autenticadas. Después de 66 denegaciones consecutivas, root recibió los 64 eventos más recientes en orden cronológico. También se comprobó que un proceso de una sesión anterior, aunque conservara descriptores de consola, recibía `-1` al intentar leer o escribir durante la sesión siguiente.

## 9. Compilación y ejecución

Desde la raíz del repositorio, compilar la imagen y arrancar xv6 en QEMU:

```sh
make -j4 kernel/kernel fs.img
make qemu
```

Una vez iniciada una sesión de root, se puede consultar el registro ejecutando `audit`.

## 10. Limitaciones

- El registro es volátil: no sobrevive al reinicio.
- La capacidad está fijada en 64 eventos; los eventos más antiguos se descartan al llenarse el búfer.
- El inode puede aparecer como 0 cuando el rechazo ocurre antes de resolver el archivo.
- La comprobación de permisos de los descriptores ordinarios se realiza al abrir; la validación por sesión se mantiene específicamente para descriptores de consola heredados.

## 11. Conclusión

El módulo aplica las verificaciones de autorización en los puntos de acceso del kernel y deja evidencia de las denegaciones para su consulta por root. Las pruebas de lectura, ejecución, capacidad del registro y aislamiento de la consola se observaron en la rama de integración del proyecto.

## 12. Referencias

- Código fuente del proyecto: directorios `kernel/`, `user/` y `mkfs/` en la rama `integration/proyecto-final`.
- MIT, xv6: A simple, Unix-like teaching operating system. <https://github.com/mit-pdos/xv6-riscv>
