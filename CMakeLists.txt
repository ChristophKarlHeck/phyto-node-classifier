#
# Mbed Torch Fusion OS Project
#
cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19)

# Initialize Mbed OS build system. 
# Note: This block must be before the project() call.
set(MBED_APP_JSON_PATH ${CMAKE_SOURCE_DIR}/mbed_app.json5)
# set(CUSTOM_TARGETS_JSON_PATH custom_targets.json) # If you need a custom target, use this line to specify the custom_targets.json

include(third-party/mbed-os/tools/cmake/app.cmake) # Load Mbed CE toolchain file and basic build system

# If you need any custom upload method configuration for your target, do that here

add_subdirectory(third-party/mbed-os) # Load Mbed OS build targets.  Must be added before any other subdirectories

add_subdirectory(third-party/flatbuffers
${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build
EXCLUDE_FROM_ALL)

project(PhytoClassifier CXX) # TODO: change this to your project name

set(SOURCES 
     ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/model_executor/ModelExecutor.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/adc/AD7124.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/interfaces/ReadingQueue.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/interfaces/SendingQueue.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/Conversion.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/serial_mail_sender/SerialMailSender.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/preprocessing/Normalization.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/preprocessing/OnlineMean.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/mbed_stats_wrapper.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/reading_mutex.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/sending_mutex.cpp
)

add_executable(PhytoClassifier ${SOURCES})

target_link_libraries(PhytoClassifier PUBLIC
     mbed-os # Can also link to mbed-baremetal here
     #mbed-ble
     flatbuffers
     /home/chris/executorch_v030/executorch/cmake-out/lib/libextension_runner_util.a
     /home/chris/executorch_v030/executorch/cmake-out/lib/libexecutorch.a
     "-Wl,--whole-archive"
     /home/chris/executorch_v030/executorch/cmake-out/lib/libexecutorch_no_prim_ops.a
     /home/chris/executorch_v030/executorch/cmake-out/kernels/quantized/libquantized_ops_lib.a
     /home/chris/executorch_v030/executorch/cmake-out/examples/arm/libarm_portable_ops_lib.a
     /home/chris/executorch_v030/executorch/cmake-out/kernels/quantized/libquantized_kernels.a
     /home/chris/executorch_v030/executorch/cmake-out/lib/libportable_kernels.a
     "-Wl,--no-whole-archive"
     ) 

###GENRAL###
target_include_directories(PhytoClassifier
     PUBLIC 
          ${CMAKE_CURRENT_SOURCE_DIR}/include #need it for executorch includes and logger.h
          /home/chris/executorch_v030/ #for executorch headers
          /home/chris/flatbuffers/include
          ${CMAKE_CURRENT_SOURCE_DIR}/models     
)

###GENRAL####
mbed_set_post_build(PhytoClassifier) # Must call this for each target to set up bin file creation, code upload, etc


###GENRAL###
mbed_finalize_build() # Make sure this is the last line of the top-level buildscript