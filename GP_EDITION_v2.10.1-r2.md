# TONE3000 GP Edition · corrección v2.10.1-r2

Esta revisión corrige la integración del conversor v2.10.1 sin modificar su
algoritmo DSP ni el flujo B2048 → Corrective IR opcional → B1024/B512 → Tone
Match directo.

## Correcciones

- Compatibilidad con CMake 4: r8brain y SoXR se descargan sin configurar sus
  proyectos antiguos; el plugin compila directamente `r8bbase.cpp` y
  `src/fft4g.c`.
- API JUCE correcta: `ThreadPool::addJob` ya no se trata como un booleano y
  `removeAllJobs` recibe un tiempo de espera válido.
- Cancelación comprobada mientras una instancia espera el turno global y en
  los puntos de progreso de la conversión.
- Excepciones contenidas dentro del worker y limpieza automática de carpetas
  temporales en éxito, error o cancelación.
- La caché de modelos usa bytes inmutables compartidos. Pulsar Convertir toma
  una referencia O(1) y no copia varios MB manteniendo `chainMutex`.
- El estímulo embebido se escribe desde el worker en una carpeta privada; la
  interfaz no se bloquea materializando el WAV y no existe una caché persistente
  que pueda quedar dañada.
- La pestaña recupera una conversión activa al cerrarse y abrirse de nuevo,
  detiene el polling al terminar y sólo muestra RMSE cuando hay un resultado.
- Al abrir el conversor se elimina cualquier destino pendiente del navegador
  de tonos.
- Los builds HEADLESS ya no enlazan por accidente el servicio GUI de
  conversión.
- Actions ejecuta regresiones reales del Tone Match/exportador para GP-200
  B1024 y GP-5/GP-50 B512, además de las pruebas DSP existentes.

## Build manual en GitHub Actions

1. Subir el contenido del ZIP completo a una rama o aplicar el ZIP de cambios
   sobre la entrega anterior.
2. Abrir **Actions → Build TONE3000 GP Edition → Run workflow**.
3. Elegir `Release`.
4. Descargar el artefacto
   `TONE3000-GP-Edition-<version>-windows` al finalizar.

El artefacto contiene VST3 y Standalone sin firma. AAX, LV2 y CLAP permanecen
desactivados en este workflow.
