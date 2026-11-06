# Calibración de BABY AIR

Reproducible con `./build_tests.sh --calibrate`.

## Por qué no se cumplen los +10 / +20 / +14 / +18 dB

El objetivo se midió como *cambio de energía de la banda 5–16 kHz* con ruido rosa a −18 dBFS RMS, controles al 100 %.
Para alcanzarlo, `levelMax` tendría que ser 9–48 (según el modo). Con ese nivel un seno de 3 kHz a −20 dBFS
tendría un 2.º armónico mayor que el fundamental: el ruido reparte los armónicos en banda ancha, los tonos los concentran.
Siguiendo la regla "la seguridad sonora tiene prioridad sobre la cifra", `levelMax` se calibra con **dos restricciones** y se toma la menor:

1. Objetivo del spec sobre ruido rosa (+10 / +20 / +14 / +18 dB).
2. Techo de THD de un seno de 3 kHz / −20 dBFS: CLEAN 10 %, BRIGHT 14 %, VINTAGE 22 %, BABY BOOST 40 %.

En los cuatro modos manda siempre la restricción 2.

## Resultados (100 % / 100 %, 8x, 48 kHz)

| Modo       | levelMax (MID/HIGH) | THD tono 3k | Ruido rosa 5–16k | Voz sintética 5–16k | Voz sintética 8–16k ("aire") |
|------------|---------------------|-------------|------------------|----------------------|------------------------------|
| CLEAN      | 0.87 / 0.87         | 10.0 %      | +0.1 dB          | +0.2 dB              | +0.7 dB                      |
| BRIGHT     | 1.37 / 1.37         | 14.0 %      | +0.5 dB          | +0.8 dB              | +2.0 dB                      |
| VINTAGE    | 3.15 / 3.15         | 22.0 %      | +0.7 dB          | +0.5 dB              | +1.2 dB                      |
| BABY BOOST | 1.15 / 1.15         | 39.8 %      | +2.7 dB          | +4.6 dB              | +7.5 dB                      |

"Voz sintética": serie armónica sobre 190 Hz con vibrato, pendiente −10 dB/oct sobre 500 Hz, −20 dBFS RMS.
La banda de 8–16 kHz es la métrica más representativa de "aire", porque el contenido original ahí es mínimo.
Si se quiere más energía (sobre todo en BABY BOOST), se sube el techo de THD en `calibrate()` y se vuelve a calibrar. Debe decidirse escuchando.

## Qué se garantiza (tests)
- El fundamental no cambia más de ±0.25 dB en los modos CLEAN/BRIGHT/BABY BOOST (±1.9 dB en VINTAGE por su curva asimétrica); por debajo de 1 kHz, 0.00 dB.
- 0 % de excitación, MIX 0 % y Power OFF dan exactamente la señal original alineada (error 0).
