# Compilación rápida: sólo VST3

Sustituye en el repositorio el archivo:

`/.github/workflows/build-gp-edition.yml`

El workflow modificado:

- construye únicamente el target `TONE3000_VST3`;
- utiliza todos los núcleos disponibles del runner;
- no construye ni empaqueta el ejecutable Standalone;
- no construye ni ejecuta los binarios de pruebas en este workflow rápido;
- publica un ZIP que contiene solamente `TONE3000.vst3`.

La lógica del plugin y del conversor no cambia.
