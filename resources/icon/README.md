# NRO icon

The build pipeline embeds `icon.jpg` (128x128 JPEG) into the .nro
metadata so hbmenu shows a custom icon.

Drop a 128x128 (or 256x256) JPEG here named `icon.jpg` and rebuild.
Without it, hbmenu uses a generic icon.

A placeholder so the directory exists:

```
# Generate a placeholder icon (requires ImageMagick)
convert -size 128x128 xc:'#f09199' -gravity center \
        -pointsize 72 -fill white -annotate +0+0 'A' \
        resources/icon/icon.jpg
```
