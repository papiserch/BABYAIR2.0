add_executable(baby_air_dsp_tests
    dsp_tests.cpp
    ../DSP/OversamplingEngine.cpp
    ../DSP/ModeProcessor.cpp
    ../DSP/ExciterEngine.cpp)
target_compile_features(baby_air_dsp_tests PRIVATE cxx_std_17)
if(NOT MSVC)
    target_compile_options(baby_air_dsp_tests PRIVATE -O2 -Wall -Wextra -fno-fast-math)
endif()
add_test(NAME dsp COMMAND baby_air_dsp_tests)
