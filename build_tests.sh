#!/bin/sh
# Builds and runs the JUCE-free DSP test suite with plain g++/clang++ (no CMake needed).
set -e
cd "$(dirname "$0")/Source"
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra Tests/dsp_tests.cpp DSP/OversamplingEngine.cpp DSP/ModeProcessor.cpp DSP/ExciterEngine.cpp -o /tmp/baby_air_dsp_tests
/tmp/baby_air_dsp_tests "$@"
