# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# set(EXTRA_COMPONENT_DIRS "$ENV{ADF_PATH}/components")
project(sd-idf)

# message(STATUS "Checking i2s registers are not read-write by half-word")
# include($ENV{IDF_PATH}/tools/ci/check_register_rw_half_word.cmake)
# check_register_rw_half_word(SOC_MODULES "i2s" "pcr" "hp_sys_clkrst"
#                            HAL_MODULES "i2s")
