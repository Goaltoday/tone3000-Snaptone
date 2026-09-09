# NAM → CLO en el plugin TONE3000

Esta integración incorpora el conversor NamToClo v2.10.1 al plugin oficial sin
duplicar la descarga de modelos. La pestaña usa los bytes del NAM que ya están
en `ChainBlock::modelCache` y ejecuta la conversión en un `juce::ThreadPool`
separado del audio.

## Flujo que se conserva

1. El NAM se carga normalmente desde Tone3000 o desde un fichero local.
2. La pestaña selecciona ese NAM y el destino GP-200 (B1024) o GP-5/GP-50
   (B512).
3. La conversión nativa genera A128/B2048.
4. `Corrective IR`, si se activa, se aplica sobre B2048.
5. El CLO se reduce al tamaño final seleccionado y se aplica un único Tone
   Match directo sobre B1024/B512.
6. Se exporta el candidato final sin confianza espectral, sin selección de
   candidatos y sin suavizado del 5 %.

## Fuentes integradas de audio

- Al activar Corrective IR se puede elegir un bloque IR cargado en cualquiera
  de las dos cadenas. Se usa el WAV original del modelo activo, sin ganancias,
  mezcla, EQ ni otros bloques; también permanece disponible un WAV externo.
- La referencia de Tone Match ofrece `Original de conversión` o los WAV del
  primer nivel de `Documentos/TONE3000 CLO`. El botón `Actualizar` vuelve a
  leer la carpeta sin cerrar la pestaña.
- Los bytes de un IR cargado se mantienen mediante una referencia inmutable y
  el worker crea su copia temporal, por lo que no hay descarga adicional.

El estímulo oficial `nam_input_wav.wav` se compila como `BinaryData`. El worker
crea una copia privada dentro de la carpeta temporal de cada trabajo; así no
se bloquea la interfaz escribiendo el WAV ni se reutiliza una caché dañada.

## Puntos de integración

- `plugin/include/ConversionManager.h` y `plugin/src/ConversionManager.cpp`:
  cola de conversión, estado consultable y serialización del modelo temporal.
- `plugin/src/ProcessorChain.cpp`: referencia inmutable O(1) a los bytes del
  modelo, opciones y acceso al estímulo embebido.
- `plugin/src/EditorWebViewSetup.cpp`: funciones nativas
  `startNamToClo`, `getNamToCloStatus`, `listNamToCloReferences` y
  `pickConversionFile`.
- `plugin/src/Editor.cpp` / `plugin/include/Editor.h`: selectores nativos de
  WAV y carpeta de salida.
- `ui/src/components/ConversionPanel.tsx`: pestaña, polling que termina al
  completar y reconexión al trabajo si la pestaña se cierra y se reabre.
- `ui/src/components/Plugin.tsx` y `PluginHeader.tsx`: icono y takeover de la
  pestaña.

## Dependencias y compilación

El CMake raíz fija `r8brain-free-src` en `version-3.7` y SoXR en `0.1.3`.
`r8bbase.cpp` y `src/fft4g.c` se enlazan dentro del plugin junto con el
NeuralAmpModelerCore que ya usa TONE3000. El WAV se añade a `WEB_ASSETS`.

La acción manual `.github/workflows/build-gp-edition.yml` configura Windows
x64, compila la UI, reconfigura CMake para incluir los recursos generados,
construye VST3 y Standalone, ejecuta las pruebas DSP y las regresiones de
exportación B1024/B512 mediante `ctest`, y publica un ZIP sin firma. La
firma de código, notarización y PACE deben añadirse en un job posterior con
los secretos del proyecto.

Las funciones nativas están diseñadas para mantenerse síncronas sólo en la
petición inicial; el trabajo pesado y los cambios de estado ocurren fuera del
hilo de audio. La espera global entre instancias comprueba la cancelación, las
excepciones quedan contenidas dentro del worker y las carpetas temporales se
limpian también en errores o cancelaciones. Al cerrar el procesador se solicita
la cancelación y se espera al worker antes de destruir la cadena.
