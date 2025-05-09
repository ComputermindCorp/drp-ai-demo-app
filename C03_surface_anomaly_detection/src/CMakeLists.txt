# CMakeList.txt : CMake projects in app_patch_core_demo
cmake_minimum_required (VERSION 3.10)
project(app_patch_core_demo)
set(CMAKE_CXX_STANDARD 17)

# Setting and including TVM Directory
set(TVM_ROOT $ENV{TVM_HOME})
include_directories(./)
include_directories(${TVM_ROOT}/include)
include_directories(${TVM_ROOT}/../apps)
include_directories(${TVM_ROOT}/3rdparty/dlpack/include)
include_directories(${TVM_ROOT}/3rdparty/dmlc-core/include)
include_directories(${TVM_ROOT}/3rdparty/compiler-rt)
include_directories(${TVM_ROOT}/../how-to/sample_app_v2h/common_files)
# include_directories("package")

set(TVM_RUNTIME_LIB ${TVM_ROOT}/build_runtime/libtvm_runtime.so)
set(EXE_NAME app_patch_core_demo)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -fopenmp")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -fopenmp")
set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

file(GLOB SOURCE *.cpp *.h)
add_executable (${EXE_NAME}
${SOURCE}
)

add_compile_options(${EXE_NAME}  ${CXX} ${LDFLAGS} -l -g m -lrt -mtune=cortex-a55 -DNDEBUG)
target_compile_options(${EXE_NAME}  PUBLIC -O2 -Wall -Ofast)
target_compile_definitions(${EXE_NAME} PRIVATE IMAGE_MODE)
target_compile_definitions(${EXE_NAME} PRIVATE V2H)
target_compile_definitions(${EXE_NAME} PRIVATE DEBUG_LOG)
target_compile_features(${EXE_NAME} PUBLIC  cxx_std_17)

link_directories(${POKY_PATH}/usr/lib64/)
include_directories("include")
include_directories(SYSTEM $ENV{OECORE_NATIVE_SYSROOT} ${POKY_PATH}/usr/include )
include_directories("/opt/poky/3.1.31/sysroots/aarch64-poky-linux/usr/lib64/aarch64-poky-linux/8.3.0/include")

TARGET_LINK_LIBRARIES(${EXE_NAME} pthread)
TARGET_LINK_LIBRARIES(${EXE_NAME} turbojpeg)
TARGET_LINK_LIBRARIES(${EXE_NAME} jpeg)
TARGET_LINK_LIBRARIES(${EXE_NAME} EGL GLESv2) 
TARGET_LINK_LIBRARIES(${EXE_NAME} wayland-cursor wayland-egl wayland-client) 
TARGET_LINK_LIBRARIES(${EXE_NAME} mmngr mmngrbuf) 
# TARGET_LINK_LIBRARIES(${EXE_NAME} gomp) 

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    target_include_directories(${EXE_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${EXE_NAME} ${OpenCV_LIBS})
endif()

# find_package(OpenMP REQUIRED)
# if(OpenMP_FOUND)
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
# endif()

target_link_libraries(${EXE_NAME} ${TVM_RUNTIME_LIB})