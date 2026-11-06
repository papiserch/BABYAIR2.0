# BABY AIR™ — Harmonic Exciter (Baby Demon)

Excitador armónico VST3 / AU / Standalone en C++17 + JUCE 8.

## Estado de verificación (leer primero)
| Parte | Estado |
|---|---|
| Motor DSP (`Source/DSP`, sin JUCE) | Compilado con g++ 13 (`-Wall -Wextra`, sin warnings) y **probado**: `./build_tests.sh` → *ALL TESTS PASSED* |
| Capa JUCE (`PluginProcessor`, `PluginEditor`, `UI/`, `Parameters`, `CMakeLists.txt`) | **Escrita pero NO compilada ni ejecutada.** El entorno de desarrollo no tenía JUCE, CMake ni red. Pueden aparecer errores de compilación o de maquetación; no se ha visto la interfaz en pantalla |
| Plugin cargado en un DAW | **No probado** |
| Escucha real | **No realizada**: la calibración es objetiva (ver `docs/CALIBRATION.md`), no auditiva |

## Compilar
Requisitos: CMake ≥ 3.22, compilador C++17 (Xcode / VS 2022 / GCC 11+), git (para descargar JUCE).
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release            # descarga JUCE 8.0.6
# o con JUCE local:  cmake -B build -DJUCE_PATH=/ruta/a/JUCE
cmake --build build --config Release
```
Salidas en `build/BabyAir_artefacts/Release/` (VST3, Standalone; AU solo en macOS). Copia el `.vst3` a
`%COMMONPROGRAMFILES%\VST3` (Windows), `~/Library/Audio/Plug-Ins/VST3` (macOS) o `~/.vst3` (Linux).
AAX no incluido (requiere SDK de Avid). Solo DSP, sin JUCE: `./build_tests.sh` (o `-DBABY_AIR_BUILD_PLUGIN=OFF`).

## Compilar en GitHub (sin instalar nada)
1. Crea un repositorio vacío en GitHub y sube el contenido de esta carpeta:
   ```sh
   git init && git add . && git commit -m "BABY AIR 1.0.0"
   git branch -M main
   git remote add origin https://github.com/TU_USUARIO/baby-air.git
   git push -u origin main
   ```
2. Pestaña **Actions**: el workflow *Build BABY AIR* se ejecuta solo (también con *Run workflow*).
   Primero corre los tests DSP y, si pasan, compila en Windows, macOS (universal arm64+x86_64) y Linux.
3. Descarga los plugins en **Artifacts** de la ejecución (`BABY_AIR-Windows`, `-macOS`, `-Linux`).
4. Para publicar una release con los zips: `git tag v1.0.0 && git push origin v1.0.0`.

Si un job falla, abre el log del paso *Build*: los errores de compilación de la capa JUCE (nunca compilada hasta ahora) aparecerán ahí y son lo primero que hay que corregir.
Los binarios macOS salen sin firmar ni notarizar y los de Windows sin firmar: el sistema operativo mostrará avisos de seguridad.
**Licencia:** JUCE es AGPLv3 o comercial; distribuir BABY AIR requiere cumplir una de las dos. Este repositorio no incluye ningún archivo LICENSE: elige el tuyo.

## Controles
| Control | Función |
|---|---|
| ON | Bypass real con transición suave (la salida sigue retardada por la latencia, en sincronía con la compensación del host) |
| AIR MODE | CLEAN / BRIGHT / VINTAGE / BABY BOOST. Crossfade de 40 ms; los valores de los knobs se conservan |
| MID HIGH (0–100 %) | Excitación de la región 2–7.5 kHz (armónicos hasta ~18 kHz). 0 % = ninguna excitación |
| HIGH (0–100 %) | Excitación de la región 7–18 kHz |
| LINK | Al arrastrar un knob, el otro se mueve conservando la diferencia (el knob arrastrado se limita para que nunca cambie). Solo actúa en gestos de usuario, no con automatización del DAW |
| TRIM | Salida ±18 dB (0 dB por defecto) |
| MIX (0–100 %) | Dry/Wet. `wet = dry + armónicos`, ambas rutas alineadas a la muestra |
| OS 2x/4x/8x/16x | Oversampling real (por defecto 8x). Cambiarlo cambia la latencia (se informa al host) |
Medidores IN/OUT: RMS (300 ms) + pico + peak hold (1.5 s) + LED de clip (clic para borrar). Presets de fábrica: menú de programas del DAW.

## Arquitectura
`Parameters` → `PluginProcessor` → `ExciterEngine` (suavizado, bypass, crossfade de modo, MIX/TRIM) → `ModeVoice` (detector de sibilancia + 2 `BandProcessor` por canal) → `Oversampler` + `AdaaShaper`.
- La señal original nunca se filtra: solo se **suma** contenido armónico. Por eso no hay huecos ni picos de cruce.
- Cada banda: aislamiento Butterworth 4º → pre-énfasis → nivelador dependiente de la señal → 3 caminos no lineales en paralelo (tanh asimétrico, tanh impar, rectificador suave) → solo el **residuo no lineal** (se resta el fundamental con la describing function exacta de tanh) → high-pass de salida + tilt + DC blocker.
- Oversampling: half-band FIR de fase lineal en cascada, latencia entera exacta (47 muestras a 8x/44.1 kHz = 1.07 ms). ADAA de 1er orden en 1x/2x/4x; a 8x/16x solo oversampling (ahorra CPU).
- Sin asignaciones, bloqueos ni E/S en el callback; atomics para medidores. Sin limitador: solo una cota de seguridad de ±16 (+24 dBFS) y saneado de NaN/Inf. No compilar con `-ffast-math`.
- Un cambio de modo o de calidad recalcula coeficientes (decenas de µs) en el hilo de audio: sin asignaciones, pero con un coste puntual.

## Resultados medidos (DSP, `./build_tests.sh`)
Aliasing, seno 9 kHz con drive ×8 a 44.1 kHz (energía no armónica re fundamental): 1x −8.5 dB · 1x ADAA −16.7 · 2x ADAA −43.6 · 4x ADAA −67.9 · 8x −74.9 · 16x −96.0.
Oversampler transparente (error ≤ −76 dB) a 44.1/48/96/192 kHz. Fundamental ±0.2 dB (VINTAGE ±1.9 dB). CPU estéreo 44.1 kHz, 8x: 7–8 % de un núcleo (máquina de desarrollo). Detalle y límites en `docs/CALIBRATION.md`.

## Limitaciones conocidas
- No se alcanzan los +10/+20/+14/+18 dB del brief: se limitó por THD (ver calibración). BABY BOOST da +7.5 dB en 8–16 kHz con voz sintética; CLEAN solo +0.7 dB (es sutil por diseño; su THD es casi todo 3.º armónico).
- La calibración usa señales sintéticas. Faltan pruebas con voces, hi-hats y 808 reales, y el ajuste por oído.
- El logo es un dibujo vectorial aproximado a la referencia, no el gráfico original. Pendiente: sustituirlo por el SVG de Baby Demon.
- Latencia ≠ 0 (~1 ms a 44.1/48 kHz): el host debe soportar PDC.
