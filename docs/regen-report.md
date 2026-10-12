# H1: regeneración del código traducido

- **ELF usado:** `PSP_DATA/EBOOT.BIN` (sha256 85c5e172…3d17f).
- **Comando:** `tools/regen_generated.sh ~/VCSNative/PSP_DATA/EBOOT.BIN`.
  - Con el traductor del fork (`build/mac/bin/vcs_recomp`, b5a861d).
  - Con el del original (`VCSNX_RECOMP=.cache/bin/vcs_recomp_upstream`, f6e7d41, compilado con `tools/build_upstream_generator.sh`).
- **Resultado directo:** no hay línea `identical=…`, porque ninguno de los dos traductores termina.
  - Los dos escriben `generated_units.hpp` y `generated_unit_0000.cpp`, y luego se atascan en la unidad 0001.
  - La memoria virtual llega a ~496 GB y la residente oscila entre 1 y 4,5 GB. macOS mata el proceso (`Killed: 9`) a los 5–7 minutos.
  - `sample` muestra todo el tiempo dentro de `emit_function_source`, haciendo crecer un `std::string` sin fin.
- **Lo que sí coincide:**
  - `generated_units.hpp` es idéntico al publicado. El descubrimiento de funciones y la partición en unidades reproducen el original a partir del ELF del usuario.
  - `generated_unit_0000.cpp` solo difiere en 111 líneas, siempre la misma constante: el límite de saltos locales `if (++local_transfers < 256u)` publicado frente a `< 2048u` que emite el traductor actual. El código publicado se generó con una versión anterior del traductor, que no está en el commit fijado.
- **Pasadas de optimización:**
  - cross_unit_hot_regs: no aplicable, porque la generación no termina.
  - gpr_blocks: no aplicable, por el mismo motivo.
  - fpr_blocks: no aplicable, por el mismo motivo.
- **Hipótesis descartada:** que el atasco viniera del cambio del fork en `break` (`docs/fork-review.md`). El traductor del original también se atasca.
- **Conclusión:** se usa el `generated/` publicado del original (spec §13.2), copiado en local y fuera del repo: `identical=237 different=0 missing=0 extra=0`.
- **Pendiente para el conversor futuro (spec §13.5):** encontrar la versión del traductor que produjo el código publicado (constante 256) y la causa del atasco en la unidad 0001. Sin eso, un usuario no puede regenerar el código desde su propio ELF.
- **Archivos distintos frente al publicado (en la prueba parcial):**
```
different: generated_unit_0000.cpp   (111 líneas: local_transfers 256u -> 2048u)
missing:   las 235 unidades restantes y auto_codegen_report.json (la generación no terminó)
```
