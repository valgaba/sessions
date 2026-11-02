Sessions 1.0.0 - Windows 64 bits

Versión de desarrollo en fase de pruebas.

Extrae la carpeta sessions_1_0_0 completa y ejecuta sessions.exe.
Conserva todas las DLL y subcarpetas junto al ejecutable.
Incluye Qt 6.7, LibVLC 3 y las fuentes Montserrat; no necesitas Qt Creator ni instalar VLC.
Las fuentes de la carpeta font/ se cargan solo para Sessions: no hace falta instalarlas.

Cue prepara un vídeo o imagen en composición. Play lo muestra en salida.
El último fotograma permanece al terminar un vídeo. Otro Play cambia
de contenido con fundido. Stop lo retira con fundido a negro.
Mix es una opción: al activarla no comienza la reproducción. Play con
Mix activado añade el contenido a la mezcla de salida. Loop repite;
Next arranca esa acción cuando termina la anterior y Purge la elimina
al terminar, sin borrar el archivo original. Cue de audio permite escucharlo.
Countdown permite asociar textos para la cuenta y para el tiempo total.
Los selectores Audio de Composición y Salida eligen el dispositivo de sonido.
Opciones > Tiempo de fundido permite configurar la duración.

Código fuente y compilación:
https://github.com/valgaba/sessions

Licencia GNU GPL v3 o posterior. Licencias y créditos de terceros:
carpeta licenses/.
