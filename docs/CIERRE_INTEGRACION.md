# Verificación final de autenticación y control de acceso

Base integrada: baab949 (Isaac 6558316, Samir 82a8f31, Hans 757fda6).
Las correcciones de cierre agregan pruebas, evidencias y documentación; no
sustituyen los módulos ni alteran la autoría de sus implementaciones.

| Caso | Verificación real | Resultado |
| --- | --- | --- |
| P1 | login válido; whoami de Samir, Isaac y root | Aprobada |
| P2 | contraseña inválida; retorno a Usuario; sin eco de contraseña | Aprobada |
| P3 | creación propia, escritura, lectura y UID/modo | Aprobada |
| P4 | lectura y truncado ajenos denegados; evento de auditoría | Aprobada |
| P5 | dueño/root autorizados; ajeno sin chmod ni escritura; entradas inválidas | Aprobada |
| P6 | exec de /echo sin x rechazado; programa no iniciado; auditoría | Aprobada |
| P7 | padre privado y padre 0777; propiedad; w+x; archivo conservado; dueño elimina | Aprobada |
| P8a | UID conservado por fork/exec en sesiones reales | Aprobada |
| P8b | UID, modo y contenido tras reiniciar la misma imagen y nuevo login | Aprobada |

Comprobaciones adicionales: un proceso autenticado no puede escalar mediante
authenticate ni startsession; mknod restringido; auditoría solo para root;
descriptores abiertos conservan capacidades tras chmod; nuevas aperturas se
rechazan; 66 denegaciones dejan los 64 eventos del generador más reciente.

La primera aserción de startsession esperaba -1. La interfaz de Isaac devuelve
STARTSESSION_DENIED (-2), por lo que se corrigió la expectativa y se repitió la
suite. El registro inicial se conserva. No se agregó un bypass al kernel.

Pasaron 24 comprobaciones de aceptación y 21 etapas de la suite de Samir,
incluida usertests -q. Entorno: Debian 13, GCC RISC-V 14.2.0, QEMU 10.0.13,
3 harts. test_mkfs valida por separado el formato inicial de la imagen.

Reproducir según docs/MODULO_SAMIR.md. No ejecutar make clean entre los dos
arranques de persistencia. Las credenciales son las educativas de kernel/auth.c.

Límites: tres cuentas fijas, seis bits efectivos, sin grupos/chown/umask ni
migración de imágenes. Registro de auditoría volátil. Programas incorporados
para el gestor y root. El resultado es un prototipo educativo, no una auditoría
exhaustiva ni una garantía de seguridad de producción.

El Word compartido recoge implementación y resultados finales. Sus imágenes
modulares anteriores se sustituyen por las capturas autenticadas cuando
corresponde. Las capturas de la fase anterior se conservan en el historial.
