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

El estímulo oficial `nam_input_wav.wav` se compila como `BinaryData`, se
materializa en la carpeta de datos del usuario y se reutiliza en las
conversiones posteriores.

## Puntos de integración

- `plugin/include/ConversionManager.h` y `plugin/src/ConversionManager.cpp`:
  cola de conversión, estado consultable y serialización del modelo temporal.
- `plugin/src/ProcessorChain.cpp`: snapshot bajo `chainMutex`, opciones y
  materialización del estímulo embebido.
- `plugin/src/EditorWebViewSetup.cpp`: funciones nativas
  `startNamToClo`, `getNamToCloStatus` y `pickConversionFile`.
- `plugin/src/Editor.cpp` / `plugin/include/Editor.h`: selectores nativos de
  WAV y carpeta de salida.
- `ui/src/components/ConversionPanel.tsx`: pestaña de usuario y polling del
  trabajo.
- `ui/src/components/Plugin.tsx` y `PluginHeader.tsx`: icono y takeover de la
  pestaña.

## Dependencias y compilación

El CMake raíz fija `r8brain-free-src` en `version-3.7` y SoXR en `0.1.3`.
`r8bbase.cpp` y `src/fft4g.c` se enlazan dentro del plugin junto con el
NeuralAmpModelerCore que ya usa TONE3000. El WAV se añade a `WEB_ASSETS`.

La acción manual `.github/workflows/build-gp-edition.yml` configura Windows
x64, compila la UI, reconfigura CMake para incluir los recursos generados,
construye VST3 y Standalone, ejecuta `ctest` y publica un ZIP sin firma. La
firma de código, notarización y PACE deben añadirse en un job posterior con
los secretos del proyecto.

Las funciones nativas están diseñadas para mantenerse síncronas sólo en la
petición inicial; el trabajo pesado y los cambios de estado ocurren fuera del
hilo de audio. Al cerrar el procesador se espera al worker antes de destruir
la cadena.
