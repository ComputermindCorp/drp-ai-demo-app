# CMakeList.txt : CMake projects in app_body_meas_cam
cmake_minimum_required (VERSION 3.12)

project (app_body_meas_cam)

set(PROJECT_NAME app_body_meas_cam)
#set(POKY_PATH $ENV{SDKTARGETSYSROOT})
set(CMAKE_CXX_STANDARD 17)
set(LIBREALSENSE2_INCLUDE package/librealsense/include)
set(LIBREALSENSE2_LIB package/librealsense/lib)
link_directories(${POKY_PATH}/usr/lib/)
link_directories(${POKY_PATH}/usr/lib64/)
link_directories(${LIBREALSENSE2_LIB}/)
link_directories(":")

# Setting and including TVM Directory
set(TVM_ROOT $ENV{TVM_HOME})
include_directories(${TVM_ROOT}/include)
include_directories(${TVM_ROOT}/3rdparty/dlpack/include)
include_directories(${TVM_ROOT}/3rdparty/dmlc-core/include)
include_directories(${TVM_ROOT}/3rdparty/compiler-rt)
include_directories($ENV{SDK}/sysroots/aarch64-poky-linux/usr/include/opencv4)

set(TVM_RUNTIME_LIB ${TVM_ROOT}/build_runtime/libtvm_runtime.so)
set(EXE_NAME app_body_meas_cam)

file(GLOB SOURCE *.cpp *.h)
add_executable (${EXE_NAME}
${SOURCE}
)

target_compile_options(${EXE_NAME}  PUBLIC -O2 -Wall)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

include_directories("include")
include_directories("package")
include_directories("${POKY_PATH}/usr/include/gstreamer-1.0/")
include_directories("${POKY_PATH}/usr/include/glib-2.0/")
include_directories("${POKY_PATH}/usr/lib64/glib-2.0/include")
include_directories("${POKY_PATH}/usr/local/include")
include_directories("${POKY_PATH}/usr/include/opencv4")
include_directories(SYSTEM $ENV{OECORE_NATIVE_SYSROOT} ${POKY_PATH}/usr/include )
include_directories(${LIBREALSENSE2_INCLUDE})
target_link_libraries(${PROJECT_NAME} 
  realsense2 pthread glib-2.0 mmngr gobject-2.0 gstreamer-1.0  gstbase-1.0 gstapp-1.0 
	opencv_imgcodecs opencv_imgproc opencv_core opencv_highgui
	jpeg webp tiff z tbb gtk-3 png16 gdk-3 cairo
	lzma rt cairo-gobject
	xkbcommon wayland-cursor wayland-egl wayland-client epoxy
	fribidi harfbuzz fontconfig
	glib-2.0 gobject-2.0 gdk_pixbuf-2.0 gmodule-2.0 pangocairo-1.0
	atk-1.0 gio-2.0 pango-1.0 freetype pixman-1 uuid pcre
	mount resolv expat pangoft2-1.0 blkid
	EGL GLESv2 mmngrbuf)

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    target_include_directories(${EXE_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${EXE_NAME} ${OpenCV_LIBS})
endif()
target_link_libraries(${EXE_NAME} ${TVM_RUNTIME_LIB})
target_link_libraries(${EXE_NAME} ${OpenCV_LIBS})
target_compile_options(${EXE_NAME} PRIVATE -Wall -D DEBUG_LOG)
target_compile_definitions(${EXE_NAME} PRIVATE -DIMAGE_MODE)
if(V2H)
    target_compile_definitions(${EXE_NAME} PRIVATE V2H)
endif()

