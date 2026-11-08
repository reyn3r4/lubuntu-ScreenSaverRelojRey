# Reloj Rey

Salvapantallas analogico para X11/XScreenSaver, escrito en C++ con Qt 6 Widgets.
La fecha en espanol aparece dentro de la mitad superior de la esfera y la hora
digital con segundos debajo del centro. La esfera es semitransparente; se puede ajustar el color de
las manecillas y del texto, la opacidad, el segundero y el formato de 12/24
horas. Al iniciarse, lee la imagen de fondo configurada en LXQt, la ajusta a
pantalla sin deformarla y aplica un oscurecimiento suave para mantener legible
el reloj. La imagen se carga una sola vez; si no esta disponible, se usa un
fondo oscuro.

## Requisitos

- Lubuntu/X11 con las bibliotecas de ejecucion de Qt 6 Widgets y X11.
- Para compilar: `build-essential`, `cmake`, `qt6-base-dev` y `libx11-dev`.

No se necesita instalar una dependencia adicional para ejecutar el binario en
un sistema que ya tenga Qt 6 Widgets y X11.

## Compilar

Desde esta carpeta:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

El ejecutable queda en `build/reloj-rey`.

## Probar

Para probar el reloj a pantalla completa:

```sh
./build/reloj-rey --root
```

Pulsa `Esc` para cerrar la prueba. Para cambiar colores y opacidad:

```sh
./build/reloj-rey --root --accent '#38bdf8' --text '#ffffff' --face-opacity 60
```

El fondo se detecta desde `~/.config/pcmanfm-qt/lxqt/settings.conf`. Puedes
indicar otra imagen o desactivar la lectura automatica:

```sh
./build/reloj-rey --root --background /ruta/a/imagen.jpg
./build/reloj-rey --root --no-background
```

XScreenSaver tambien puede incrustarlo en una ventana X11:

```sh
./build/reloj-rey --window-id 0xID_DE_VENTANA
```

## Integrar con XScreenSaver

El descriptor de opciones esta en `xscreensaver/reloj-rey.xml`. Para instalarlo
como un salvapantallas seleccionable para todos los usuarios, instala el binario
y el descriptor de XScreenSaver:

```sh
sudo install -Dm755 build/reloj-rey /usr/libexec/xscreensaver/reloj-rey
sudo install -Dm644 xscreensaver/reloj-rey.xml \
  /usr/share/xscreensaver/config/reloj-rey.xml
```

Luego reinicia XScreenSaver y vuelve a abrir su panel de configuracion:

```sh
xscreensaver-command -restart
xscreensaver-settings
```

Despues de recompilar una version nueva, vuelve a copiar el binario a
`/usr/libexec/xscreensaver/reloj-rey` y reinicia XScreenSaver. En modo
XScreenSaver, el programa dibuja directamente en la ventana indicada por
`XSCREENSAVER_WINDOW`.

XScreenSaver combina la lista de programas del sistema con `~/.xscreensaver`.
Si el protector no aparece automaticamente, agrega `reloj-rey --root` al final
de la lista `programs:` de ese archivo y vuelve a abrir el panel.

Los colores personalizados tambien se pueden pasar directamente:

```sh
reloj-rey --root --accent '#dc2626' --text '#e2e8f0' --face '#280505' \
  --face-opacity 60 --24-hour
```

La transparencia se renderiza con alfa. Su aspecto sobre el fondo del escritorio
depende del compositor X11; sin compositor, el reloj sigue funcionando, pero
la mezcla con el fondo puede variar.
