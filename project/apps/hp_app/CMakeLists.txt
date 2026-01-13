cmake_minimum_required(VERSION 3.13.1)

set(EXTRA_CONF_FILE ../../base.conf)
set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

list(APPEND ZEPHYR_EXTRA_MODULES ${PROJECT_DIR}/drivers)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(hp_app)

# add_compile_definitions(EI_CLASSIFIER_ALLOCATION_STATIC=1)
# add_compile_definitions(EI_CLASSIFIER_ALLOCATION_STATIC_SIZE=307200)

# add_compile_definitions(EI_TENSOR_ARENA_LOCATION=".tensor_arena_buf_dtcm")

# --- CONFIGURATION ETHOS-U55 ---
if(ETHOSU_TARGET_NPU_CONFIG MATCHES "^ethos-(u[0-9]+)-([0-9]+)$")
  set(ETHOSU_ARCH "${CMAKE_MATCH_1}")
  set(ETHOSU_MACS "${CMAKE_MATCH_2}")

  add_compile_definitions(ETHOSU_ARCH_U55=1)
  add_compile_definitions(EI_CLASSIFIER_TFLITE_ENABLE_ETHOS_U55=1)

  if(ETHOSU_MACS STREQUAL "256")
    set(CONFIG_ARM_ETHOS_U55_256 y CACHE BOOL "" FORCE)
  elseif(ETHOSU_MACS STREQUAL "128")
    set(CONFIG_ARM_ETHOS_U55_128 y CACHE BOOL "" FORCE)
  endif()
else()
  message(FATAL_ERROR "Missing -DETHOSU_TARGET_NPU_CONFIG=ethos-u55-256")
endif()

# --- DEFINITIONS ---
target_compile_definitions(app PRIVATE
    # TF_LITE_STATIC_MEMORY
    # EI_CLASSIFIER_SENSOR=EI_CLASSIFIER_SENSOR_UNKNOWN
    EIDSP_USE_CMSIS_DSP=1
    EI_CLASSIFIER_TFLITE_ENABLE_CMSIS_NN=1
    EIDSP_LOAD_CMSIS_DSP_SOURCES=0
    CMSIS_NN
    ARM_MATH_CM55
    ARM_MATH_LOOPUNROLL
    EI_CLASSIFIER_ALLOCATION_STATIC=1
)

# --- INCLUSIONS ---
zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk
  ${CMAKE_CURRENT_SOURCE_DIR}/model-parameters
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse
  ${CMAKE_CURRENT_SOURCE_DIR}/npu
  ${CMAKE_CURRENT_SOURCE_DIR}/tflite-model
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/dsp
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/classifier
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/flatbuffers/include
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/gemmlowp
  ${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/ruy
)

# --- SOURCES ---
target_sources(app PRIVATE main.cpp)
target_sources(app PRIVATE 
    npu/npu_handler.cpp
)

# Edge Impulse Core
file(GLOB_RECURSE EI_DSP "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/dsp/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/dsp/*.c")
file(GLOB_RECURSE EI_CLASSIFIER "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/classifier/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/classifier/*.c")

# Third Party
file(GLOB_RECURSE EI_THIRD_PARTY 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/*.cpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/*.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/third_party/*.cc"
)

# TensorFlow Lite Micro
file(GLOB_RECURSE EI_TENSORFLOW 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/tensorflow/*.cpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/tensorflow/*.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/tensorflow/*.cc"
)

# Add

list(FILTER EI_TENSORFLOW EXCLUDE REGEX "tensorflow/lite/micro/kernels/conv.cc")
list(FILTER EI_TENSORFLOW EXCLUDE REGEX "tensorflow/lite/micro/kernels/svdf.cc")

add_compile_definitions(EI_ETHOS=1)
add_compile_definitions(ETHOSU=1)
add_compile_definitions(ETHOSU55=1)


# Model
file(GLOB_RECURSE EI_MODEL 
    "${CMAKE_CURRENT_SOURCE_DIR}/tflite-model/*.cpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/tflite-model/*.cc"
)
file(GLOB_RECURSE EI_PARAMS 
    "${CMAKE_CURRENT_SOURCE_DIR}/model-parameters/*.cpp"
)

# Porting Layer
target_sources(app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/edge-impulse-sdk/porting/zephyr/ei_classifier_porting.cpp")

# Assemblage
target_sources(app PRIVATE
    ${EI_DSP}
    ${EI_CLASSIFIER}
    ${EI_THIRD_PARTY}
    ${EI_TENSORFLOW}
    ${EI_MODEL}
    ${EI_PARAMS}
)

# --- OPTIONS ---
target_compile_options(app PRIVATE 
    -flax-vector-conversions 
    -Wno-strict-aliasing 
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    -Wno-format
    -mfpu=auto
)


set_property(TARGET app PROPERTY CXX_STANDARD 17)

target_link_libraries(app PRIVATE -lm)