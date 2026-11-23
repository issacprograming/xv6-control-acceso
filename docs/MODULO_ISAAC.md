# Módulo de autenticación e identidad de procesos

**Responsable:** Ismael Isaac Chuchon Tucno (`issacprograming`).  
**Rama de desarrollo:** `feature/autenticacion-uid`.  
**Commit principal:** `6558316` — `feat: implementar autenticacion y sesiones seguras en xv6`.  
**Rama de integración final:** `integration/proyecto-final`.  
**Commit de integración:** `baab949`.  
**Sistema operativo:** xv6-riscv.

## 1. Descripción general

Este módulo incorpora un mecanismo básico de autenticación de usuarios e identificación de procesos en xv6-riscv, un sistema operativo educativo que originalmente no distingue a los usuarios mediante identificadores UID.

La implementación permite iniciar sesión con credenciales predefinidas, asignar una identidad a cada proceso y conservarla durante la creación y ejecución de programas. También incorpora mecanismos de protección para impedir cambios de identidad no autorizados y evitar que una sesión anterior acceda a la consola de una sesión nueva.

El módulo proporciona la base de identidad que utilizan los mecanismos de propietarios y permisos desarrollados por Samir y las comprobaciones de autorización y auditoría desarrolladas por Hans.

## 2. Usuarios e identificadores UID

Se definieron tres cuentas educativas y un estado para los procesos que todavía no están autenticados.

| Usuario | UID | Descripción |
|---|---|---|
| Sin autenticar | -1 | Estado inicial del gestor de sesiones |
| root | 0 | Usuario administrador |
| isaac | 1001 | Usuario ordinario |
| samir | 1002 | Usuario ordinario |

Las constantes se definieron en `kernel/uid.h`.

Las credenciales de demostración se encuentran en `kernel/auth.c` y son verificadas por el kernel. No se implementó administración dinámica de cuentas.

## 3. Archivos y funciones implementados

| Archivo | Funcionalidad |
|---|---|
| `kernel/uid.h` | Constantes de UID |
| `kernel/proc.h` y `kernel/proc.c` | Identidad, herencia del UID y estado de sesión |
| `kernel/auth.c` y `kernel/auth.h` | Validación de credenciales y mecanismos de autenticación |
| `kernel/console.c` | Protección de consola y lectura de contraseñas sin eco |
| `kernel/exec.c` | Conservación del UID y ejecución de programas de confianza |
| `kernel/sysproc.c` | Implementación de la llamada `getuid` |
| `kernel/syscall.c` y `kernel/syscall.h` | Registro de llamadas al sistema |
| `user/init.c` | Inicio y renovación de sesiones |
| `user/login.c` | Interfaz de autenticación |
| `user/whoami.c` | Consulta de identidad |
| `user/uidtest.c` | Verificación de UID mediante `fork()` y `exec()` |
| `user/user.h` y `user/usys.pl` | Interfaces de usuario |
| `user/usys.S` | Stubs de llamadas al sistema generados |
| `Makefile` | Incorporación de programas y componentes de autenticación |

## 4. Identidad y herencia de procesos

Se agregó el campo `uid` a la estructura `struct proc`, permitiendo que el kernel mantenga la identidad asociada a cada proceso.

Durante la creación de procesos, `fork()` transmite el UID del padre al hijo. Cuando un proceso utiliza `exec()` para cargar un programa diferente, mantiene su identidad.

Se implementó además la llamada al sistema `getuid()`, que devuelve el UID del proceso actual.

El programa `whoami` permite consultar la identidad activa, mientras que `uidtest` verifica que el UID se conserve durante la ejecución de procesos.

Estas funciones permiten que los mecanismos de autorización identifiquen al usuario que solicita una operación.

## 5. Autenticación y gestión de sesiones

El sistema incorpora un programa `login` que solicita el nombre de usuario y la contraseña.

La comprobación de las credenciales se realiza desde el kernel mediante `authenticate()`. Si los datos son correctos, se asigna el UID correspondiente y se inicia una shell con esa identidad.

Si las credenciales son incorrectas, el acceso se rechaza y el programa permite realizar otro intento.

El gestor `init` comienza sin una identidad autenticada (`UID -1`) y utiliza `startsession()` para iniciar una nueva sesión. Al cerrar la shell, el sistema vuelve a presentar el inicio de sesión.

La lectura de contraseñas utiliza `readsecret()`, que evita mostrar los caracteres introducidos en la consola.

### Protección de UID 0

El kernel administra un permiso de autenticación de un solo uso para el proceso autorizado a iniciar sesión.

Ese permiso no se hereda mediante `fork()` y se consume durante la autenticación. Un proceso ordinario no puede utilizar libremente `authenticate()` para cambiar su identidad o conseguir privilegios de root.

Se incorporaron ejecutables de confianza al kernel para proteger el arranque y determinados programas utilizados durante sesiones privilegiadas.

Esta solución corresponde a un diseño educativo y no sustituye un mecanismo completo de seguridad de producción.

## 6. Llamadas al sistema

El módulo incorporó cuatro llamadas al sistema.

| Número | Llamada | Función |
|---|---|---|
| 23 | `getuid` | Consultar el UID del proceso |
| 24 | `startsession` | Iniciar una sesión autorizada |
| 25 | `readsecret` | Leer una contraseña sin eco |
| 26 | `authenticate` | Validar credenciales y establecer identidad |

Durante la integración con los demás módulos se reservaron los números 27 para `chmod`, implementado por Samir, y 28 para `auditread`, implementado por Hans.

Las interfaces de usuario se mantuvieron sincronizadas mediante `user/user.h`, `user/usys.pl` y el ensamblador generado `user/usys.S`.

## 7. Protección de la consola

Se incorporaron controles para evitar que un proceso perteneciente a una sesión anterior continúe utilizando la consola durante una nueva sesión.

El kernel verifica la sesión activa al realizar operaciones de lectura y escritura sobre el dispositivo.

La implementación también contempla descriptores heredados o conservados por procesos que permanecen activos después de cerrar la shell.

Esta protección limita el acceso entre sesiones, aunque no elimina todas las posibilidades de suplantación mediante programas ejecutados dentro de la propia sesión activa.

## 8. Compilación y ejecución

El desarrollo se realizó utilizando Ubuntu, GCC para RISC-V, Make y QEMU.

Para compilar el sistema:

```bash
make -j4 kernel/kernel fs.img
```

Para ejecutar xv6:

```bash
make qemu
```

Después del arranque aparece el inicio de sesión:

```text
xv6 kernel is booting

Usuario:
Contrasena:
```

Una vez autenticado, el usuario puede consultar su identidad:

```text
$ whoami
isaac (UID 1001)
```

Para comprobar la conservación del UID:

```text
$ uidtest
uidtest: fork inherited UID 1001
uidtest: exec preserved UID 1001
uidtest: OK
```

## 9. Pruebas realizadas

El módulo se verificó inicialmente en su rama de desarrollo y posteriormente dentro de la integración final.

| Caso | Comprobación | Resultado |
|---|---|---|
| P01 | Inicio de sesión con credenciales válidas | Aprobada |
| P02 | Rechazo de contraseñas incorrectas | Aprobada |
| P08a | Herencia del UID mediante `fork()` | Aprobada |
| P08a | Conservación del UID mediante `exec()` | Aprobada |
| Seguridad adicional | Rechazo de cambios de UID no autorizados | Aprobada |
| Sesiones | Retorno al login después de cerrar la shell | Aprobada |
| Consola | Aislamiento entre sesiones consecutivas | Aprobada |

En la validación se comprobaron las identidades de root (UID 0), Isaac (UID 1001) y Samir (UID 1002).

También se verificó que la contraseña no apareciera en la consola y que las solicitudes directas de autenticación desde procesos ordinarios no permitieran modificar indebidamente su UID.

La versión integrada superó las pruebas funcionales P01–P08 y la suite general de xv6, según los registros de validación del proyecto.

## 10. Integración con los módulos de Samir y Hans

### Integración con Samir

El módulo de propietarios y permisos utiliza el UID de los procesos para asignar la propiedad de los archivos.

Durante la integración fue necesario conservar el inicio de sesión con `UID -1` y adaptar la creación inicial de `/console` mediante `mkfs`.

También se resolvió la numeración de las llamadas al sistema para evitar conflictos entre autenticación y `chmod`.

La integración quedó registrada en el commit `2d6752d`.

### Integración con Hans

El módulo de autorización utiliza la identidad mantenida por el kernel para comprobar los permisos de lectura, escritura y ejecución.

Se conservaron el arranque autenticado, la protección de la consola y los programas de confianza al incorporar las verificaciones de autorización y auditoría.

Las operaciones denegadas pueden registrarse en el módulo de auditoría, permitiendo relacionar un intento de acceso con el UID del proceso solicitante.

La integración completa quedó registrada en el commit `baab949`, dentro de la rama `integration/proyecto-final`.

## 11. Limitaciones identificadas

La implementación es un prototipo educativo y presenta las siguientes limitaciones:

- Las cuentas y contraseñas de demostración están definidas en el código fuente.
- No existe creación, eliminación ni administración dinámica de usuarios.
- No se implementan grupos de usuarios ni un sistema completo de privilegios de UNIX/Linux.
- Los ejecutables incorporados al kernel siguen un tratamiento diferente al de los archivos ejecutables almacenados en disco.
- El aislamiento de consola no impide que un programa de una sesión activa intente imitar una pantalla de autenticación.
- La solución no debe utilizarse como sistema de autenticación de producción.

Estas limitaciones no impiden estudiar los principios fundamentales de identidad, autenticación y control de acceso desde el kernel.

## 12. Conclusión

El módulo permitió incorporar una identidad de usuario a los procesos de xv6-riscv y desarrollar un mecanismo básico de autenticación mediante inicio de sesión.

Las pruebas confirmaron la asignación correcta del UID, su herencia mediante `fork()`, su conservación durante `exec()` y el rechazo de cambios de identidad no autorizados.

Su integración con los módulos de Samir y Hans permitió relacionar la identidad del proceso con los propietarios, permisos y controles de autorización del sistema de archivos.

De esta manera, el módulo constituyó la base para desarrollar un sistema educativo de autenticación y control de acceso dentro de xv6-riscv.

## 13. Referencias

Cox, R., Kaashoek, F., & Morris, R. (2022). *xv6: A simple, Unix-like teaching operating system* (Rev. 3). Massachusetts Institute of Technology. https://pdos.csail.mit.edu/6.S081/2022/xv6/book-riscv-rev3.pdf

MIT PDOS. (s. f.). *xv6-riscv* [Código fuente]. GitHub. https://github.com/mit-pdos/xv6-riscv
