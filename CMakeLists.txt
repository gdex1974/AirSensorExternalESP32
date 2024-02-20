# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 17)
IF(POLICY CMP0012)
    CMAKE_POLICY(SET CMP0012 NEW)
ENDIF()
if ("${IDF_TARGET}" STREQUAL "")
    set(IDF_TARGET "esp32")
endif()
file(COPY_FILE "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.${IDF_TARGET}"
        "${CMAKE_CURRENT_LIST_DIR}/sdkconfig")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ExternalAirQualitySensor)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++11" REMOVE)
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=c++17" APPEND)