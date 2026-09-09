# Corrección r3.1 · texto de desplegables

Sustituir:

`ui/src/components/ConversionPanel.tsx`

Corrige el texto blanco sobre fondo blanco que WebView2 mostraba al abrir los
selectores nativos de Windows. La corrección se aplica a todos los desplegables
del conversor y no modifica la selección, el puente nativo ni el DSP.

Después de sustituirlo, volver a ejecutar el workflow de compilación VST3.
