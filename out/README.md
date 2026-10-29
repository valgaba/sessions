# Salida de Sessions

`bin/` contiene `sessions.exe`, Qt y sus plugins después de compilar con `build.ps1 -Deploy`.

Los archivos intermedios y resultados de pruebas se generan en `build/`; los ZIP de distribución, en `packages/`. Los ejecutables y DLL no se guardan en Git: se publican en Releases de GitHub. `qt67-binutils/` permite usar las herramientas locales de reemplazo, cuando son necesarias.
