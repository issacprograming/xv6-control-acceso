# Propietarios y permisos persistentes con chmod y ls -l

Añade UID/modo a los inodos, sincroniza disco/memoria/stat/mkfs y permite
cambiar permisos solo al dueño o root. Incluye chmod, ls -l y pruebas
reproducibles con capturas reales. Depende de la rama de identidad e98304b.

Validación: compilación GCC RISC-V 14.2; QEMU 10.0.13; P3 y P5 aprobadas;
persistencia tras reinicio aprobada; usertests -q: ALL TESTS PASSED.
El UID de pruebas se fija por el depurador externo; no se declara login probado.

P7 negativa falla por falta del control de unlink/directorios de Hans;
la eliminación por el dueño funciona. No fusionar como integración terminada.
Revisar especialmente inodos de 128 bytes, cambio de FSMAGIC, bootstrap UID 0,
convención del dígito central y disponibilidad del syscall 24.

Revisión de otro integrante pendiente. El Word compartido se actualiza aparte.
