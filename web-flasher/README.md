# Flasher web (ESP Web Tools)

Página que flashea el firmware en la placa **desde Chrome/Edge** vía Web Serial —
sin PlatformIO ni cables de toolchain. Pensada para que cualquiera instale el
firmware con un clic.

## Cómo se publica

El workflow [`.github/workflows/release.yml`](../.github/workflows/release.yml)
hace todo al pushear un tag `v*`:

1. Compila el firmware.
2. Genera la imagen única `firmware.factory.bin` (merge de bootloader +
   particiones + boot_app0 + app, offsets de `partitions_custom.csv`).
3. La adjunta al **GitHub Release**.
4. Despliega esta carpeta + el `.bin` + un `manifest.json` con la versión del tag
   a **GitHub Pages**.

Resultado: `https://wilmercardozo.github.io/nerd-dashboard/` flashea la última versión.

## Requisito una sola vez

Habilitar Pages: **Settings → Pages → Source: GitHub Actions**. Si no, el job
`deploy-pages` falla (pero el Release con los binarios igual se publica).

## Probar local

```bash
cd web-flasher
# pon un firmware.factory.bin aquí (de un Release o build local)
python -m http.server 8000
```

Abre `http://localhost:8000` (Web Serial permite localhost sin HTTPS). En
producción **debe** ser HTTPS — Pages ya lo es.

## Archivos

- `index.html` — página con `<esp-web-install-button>` (esp-web-tools por CDN).
- `manifest.json` — referencia; el workflow lo regenera con la versión del tag.
  En Pages, `firmware.factory.bin` queda junto al manifest (mismo origen, sin CORS).

## Generar la factory bin a mano

```bash
pio run -e nerd-dashboard
BUILD=.pio/build/nerd-dashboard
BOOT_APP0=$(find ~/.platformio/packages/framework-arduinoespressif32 -name boot_app0.bin | head -1)
python -m esptool --chip esp32 merge_bin --flash_mode dio --flash_freq 40m --flash_size 4MB \
  -o firmware.factory.bin \
  0x1000 "$BUILD/bootloader.bin" 0x8000 "$BUILD/partitions.bin" \
  0xe000 "$BOOT_APP0" 0x10000 "$BUILD/firmware.bin"
```
