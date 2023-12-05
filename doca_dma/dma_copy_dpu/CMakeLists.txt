cmake_minimum_required(VERSION 3.10)  
  
project(LIBRDPMA_DMA_COPY_DPU)  
  
# Set the default build type to debug.  
set(CMAKE_BUILD_TYPE Debug)  

add_compile_definitions(DOCA_ALLOW_EXPERIMENTAL_API)
  
find_library(doca)

# Add the include directories for the DOCA library and other common directories.  
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)  

# Add the source files for the sample.  
add_executable(dma_copy_dpu dma_copy_dpu_sample.c dma_copy_dpu_main.c ../dma_common.c ../common.c)
  
# Set the C flags for the sample.  
add_compile_options("-Wno-missing-braces")  
  