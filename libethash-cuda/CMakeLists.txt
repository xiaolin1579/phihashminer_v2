# A custom command and target to turn the CUDA kernel into a byte array header
# The normal build depends on it properly and if the kernel file is changed, then
# a rebuild of libethash-cuda should be triggered

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/CUDAMiner_kernel.h
	COMMAND ${CMAKE_COMMAND} ARGS
	-DTXT2STR_SOURCE_FILE="${CMAKE_CURRENT_SOURCE_DIR}/CUDAMiner_kernel.cu"
	-DTXT2STR_VARIABLE_NAME=CUDAMiner_kernel
	-DTXT2STR_HEADER_FILE="${CMAKE_CURRENT_BINARY_DIR}/CUDAMiner_kernel.h"
	-P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/txt2str.cmake"
	COMMENT "Generating CUDA Kernel"
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/CUDAMiner_kernel.cu
)
add_custom_target(cuda_kernel DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/CUDAMiner_kernel.h ${CMAKE_CURRENT_SOURCE_DIR}/CUDAMiner_kernel.cu)

find_package(CUDA REQUIRED)

set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};--ptxas-options=-v;-lineinfo;-use_fast_math)

if (NOT MSVC)
	list(APPEND CUDA_NVCC_FLAGS "--disable-warnings")
endif()

list(APPEND CUDA_NVCC_FLAGS_RELEASE -O3)
list(APPEND CUDA_NVCC_FLAGS_DEBUG -G)

message("")
message("----------------------------------------------------------------------------")
message("-- Detected CUDA Version: ${CUDA_VERSION}")
if(COMPUTE)
	message("-- Compute Version: ${COMPUTE}")
endif()
message("----------------------------------------------------------------------------")
message("")

if(COMPUTE AND (COMPUTE GREATER 0))
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_${COMPUTE},code=sm_${COMPUTE}")
else()
        if(CUDA_VERSION VERSION_LESS 11.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_30,code=sm_30")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_32,code=sm_32")
        endif()
        if(CUDA_VERSION VERSION_LESS 12.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_35,code=sm_35")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_37,code=sm_37")
        endif()
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_50,code=sm_50")
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_52,code=sm_52")
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_53,code=sm_53")
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_60,code=sm_60")
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_61,code=sm_61")
        list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_62,code=sm_62")
        if(NOT CUDA_VERSION VERSION_LESS 9.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_70,code=sm_70")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_72,code=sm_72")
        endif()
        if(NOT CUDA_VERSION VERSION_LESS 10.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_75,code=sm_75")
        endif()
        if(NOT CUDA_VERSION VERSION_LESS 11.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_80,code=sm_80")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_86,code=sm_86")
        endif()
        if(NOT CUDA_VERSION VERSION_LESS 12.0)
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_87,code=sm_87")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_89,code=sm_89")
                list(APPEND CUDA_NVCC_FLAGS "-gencode arch=compute_90,code=sm_90")
        endif()
endif()

file(GLOB sources CUDAMiner.cpp CUDAMiner_cuda.cu)
file(GLOB headers CUDAMiner.h CUDAMiner_cuda.h ${CMAKE_CURRENT_BINARY_DIR}/CUDAMiner_kernel.h)

cuda_add_library(ethash-cuda STATIC ${sources} ${headers})
add_dependencies(ethash-cuda cuda_kernel)
# Cmake doesn't handle nvrtc automatically
find_library(CUDA_nvrtc_LIBRARY NAMES nvrtc PATHS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib/x64 lib64/stubs lib/x64/stubs lib NO_DEFAULT_PATH)
find_library(CUDA_cuda_LIBRARY NAMES cuda PATHS ${CUDA_TOOLKIT_ROOT_DIR} PATH_SUFFIXES lib64 lib/x64 lib64/stubs lib/x64/stubs lib NO_DEFAULT_PATH)
target_link_libraries(ethash-cuda ethcore ethash::ethash phihash Boost::thread)
target_link_libraries(ethash-cuda ${CUDA_nvrtc_LIBRARY} ${CUDA_cuda_LIBRARY})
target_include_directories(ethash-cuda PUBLIC ${CUDA_INCLUDE_DIRS})
target_include_directories(ethash-cuda PRIVATE .. ${CMAKE_CURRENT_BINARY_DIR})
