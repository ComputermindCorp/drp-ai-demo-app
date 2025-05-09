1.Build librealsense
  1.1. Download and extract the latest source (.tar.gz) from https://github.com/IntelRealSense/librealsense/releases
  1.2. $ mkdir build && cd build
  1.3. $ cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_CPU_EXTENSIONS=TRUE -DHWM_OVER_XU=FALSE -DFORCE_RSUSB_BACKEND=TRUE -DBUILD_EXAMPLES=FALSE ..
  1.4. $ make
       (If you use make -j9, for example, the build will be performed on multiple cores. 9 is usually the number of CPU cores + 1.)
  1.5. $ cd Release
  1.6. Copy the header file and librealsense2.so into place

2. Build the application
  2.1. $ mkdir build && cd build
  2.2. $ cmake -DCMAKE_BUILD_TYPE=Release ..
  2.3. $ make
