# Sonda Vulkan en la Switch (Plan 2, Tarea 3)

- **Fecha:** 2026-10-11.
- **Build:** `vkprobe.nro` del commit aaf08c4, `mesa-switch` 606441c.
- **Cómo:** abierto desde hbmenu con R + juego. Primero estaba en `switch/vcsnx/` y hbmenu no lo mostró, porque lista una sola app por carpeta; después se movió a `switch/vkprobe.nro`.
- **Resultado:** `PASS`. NVK crea el dispositivo, limpia una imagen de 256×256 en la GPU, la copia a un buffer visible por la CPU y los píxeles leídos coinciden con lo esperado (±1).

```text
VCSNX Vulkan probe (mesa-switch NVK)
device: NVIDIA Tegra X1 (GM20B) (NVK GM20B)
api 1.4.354  driver 0x06802004  vendor 0x10de  device 0x0fe0
maxImageDimension2D 16384  maxSamplerAnisotropy 16.0
heap 0: 2391 MB flags 0x1
queue family 0: flags 0x7 count 1
GPU clear + copy finished in 657 us
pixel (0,0)     = 64 127 191 255
pixel (255,255) = 64 127 191 255
RESULT: PASS
Press + to exit.
```

El juego confirmó lo mismo en su primera corrida: `[vulkan] GPU: NVIDIA Tegra X1 (GM20B) (NVK GM20B)` y `ge backend requested=vulkan active=vulkan`.
