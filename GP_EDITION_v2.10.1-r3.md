# TONE3000 GP Edition · v2.10.1-r3

Aplicar estos archivos sobre la entrega v2.10.1-r2, conservando sus
correcciones CMake/JUCE y el resto del repositorio.

## Archivos que se sustituyen

- `plugin/include/ConversionManager.h`
- `plugin/include/Processor.h`
- `plugin/src/ConversionManager.cpp`
- `plugin/src/ProcessorChain.cpp`
- `plugin/src/EditorWebViewSetup.cpp`
- `ui/src/components/ConversionPanel.tsx`
- `docs/nam-to-clo-integration.md`

## Funciones añadidas

1. Corrective IR puede usar el modelo activo de cualquier slot IR cargado en
   la cadena izquierda o derecha. La alternativa WAV externo se conserva.
2. La referencia Tone Match se elige entre `Original de conversión` y los WAV
   situados directamente en `Documentos/TONE3000 CLO`.
3. `Actualizar` vuelve a escanear esa carpeta durante la sesión.
4. Native valida que el IR esté completamente cargado y que una referencia
   seleccionada siga existiendo dentro de la carpeta autorizada.

El DSP del conversor sigue siendo v2.10.1: B2048, Corrective IR opcional,
reducción B1024/B512 y un Tone Match directo final sin confianza ni suavizado.
