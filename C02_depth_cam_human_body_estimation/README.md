# Body measurement

## Application: Overview
This application is designed to estimate human pose in captured images from distance data of a depth camera, and display the results such as height on an HDMI screen.

This software could be useful in a variety of settings, such as retail stores, museums, and events.  
The AI model used for the sample application is [HRNET](https://arxiv.org/pdf/1908.07919).

<img src=./img/app_body_meas_cam.png width=600>

### Contents

Folder structure  

    └── BodyMeasurement/
         ├── src/
         │   ├── toolchain/
         │   └── package/
         │      ├── spdlog/
         │      └── librealsense/
         │          └──lib/
         ├── exe/ 
         ├── etc/ 
         ├── img/ 
         └── README.md 

### Supported Product
- RZ/V2H Evaluation Board Kit (RZ/V2H EVK)
- RZ/V2H AI SDK v5.00

### Input/Output
<table>
    <tr>
      <th>Input</th>
      <th>Output</th>
    </tr>
    <tr>
      <td style="text-align:center;">Intel RealSense Depth Camera D435i</td>
      <td style="text-align:center;">HDMI</td>
    </tr>
</table>


## Application: Requirements

### Hardware Requirements
<table>
    <tr>
      <th>For</th>
      <th>Equipment</th>
      <th>Details</th>
    </tr>
    <tr>
      <td rowspan="4">RZ/V2H</td>
      <td>RZ/V2H EVK</td>
      <td>Evaluation Board Kit for RZ/V2H.</td>
    </tr>
    <tr>
      <td>AC Adapter</td>
      <td>USB Power Delivery adapter for the board power supply.<br>
      100W is required.</td>
    </tr>
    <tr>
      <td>HDMI Cable</td>
      <td>Used to connect the HDMI Monitor and the board.<br>
      RZ/V2H EVK has HDMI port.</td>
    </tr>
    <tr>
      <td>Intel RealSense Depth Camera D435i </td>
      <td>Used as a camera input source.</td>
    </tr>
    <tr>
      <td rowspan="8">Common</td>
      <td>USB Cable Type-C</td>
      <td>Connect AC adapter and the board.</td>
    </tr>
    <tr>
      <td>HDMI Monitor</td>
      <td>Used to display the graphics of the board.</td>
    </tr>
    <tr>
      <td>microSD card</td>
      <td>Used as the filesystem.<br>
      Must have over 4GB capacity of blank space.<br>
      Operating Environment: Transcend UHS-I microSD 300S 16GB</td>
    </tr>
    <tr>
      <td>Linux PC</td>
      <td>Used to build application and setup microSD card.<br>
      Operating Environment: Ubuntu 20.04</td>
    </tr>
    <tr>
      <td>SD card reader</td>
      <td>Used for setting up microSD card.<br></td>
    </tr>
    <tr>
      <td>USB Hub</td>
      <td>Used to connect USB Keyboard and USB Mouse to the board.</td>
    </tr>
    <tr>
      <td>USB Keyboard</td>
      <td>Used to type strings on the terminal of board.</td>
    </tr>
    <tr>
      <td>USB Mouse</td>
      <td>Used to operate the mouse on the screen of board.</td>
    </tr>
  </table>

>**Note:** All external devices will be attached to the board and does not require any driver installation (Plug n Play Type)

Connect the hardware as shown below.  

|RZ/V2H EVK |
|---|
|<img src=./img/hw_conf_v2h.png width=600> |

>**Note 1:** When using the keyboard connected to RZ/V Evaluation Board, the keyboard layout and language are fixed to English.  
**Note 2:** For RZ/V2H EVK, there are USB 2.0 and USB 3.0 ports.  
Intel RealSense Depth Camera D435i needs to be connected to appropriate port based on its requirement.

## Application: Build Stage

>**Note:** User can skip to the [next stage (deploy)](#application-deploy-stage) if they do not want to build the application.  
All pre-built binaries are provided.

### Prerequisites
This section expects the user to have completed Step 5 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started.html) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- AI SDK setup is done.
- Following docker container is running on the host machine.

    |Board | Docker container |
    |---|---|
    |RZ/V2H EVK|`rzv2h_ai_sdk_container`  |

    >**Note:** Docker environment is required for building the sample application. 


### Application File Generation
1. On your host machine, copy the sources you want to build to the desired location. 

    1. Copy the source to be built to the `data` folder mounted in the Docker container.

        > /drp-ai_tvm/data/BodyMeasurement/src

2. Run (or start) the docker container and open the bash terminal on the container.  
  E.g., for RZ/V2H, use the `rzv2h_ai_sdk_container` as the name of container created from  `rzv2h_ai_sdk_image` docker image.  

   > Note that all the build steps/commands listed below are executed on the docker container bash terminal.  

3. Set your clone directory to the environment variable.  
      ```sh
    export PROJECT_PATH=/drp-ai_tvm/data/BodyMeasurement
    ```
4. Intel RealSense Depth Camera library Generation.

    (1) Download the latest `Source code(tar.gz)` from [Download site](https://github.com/IntelRealSense/librealsense/releases).  

    (2) Unzip the downloaded source. 
    > ${PROJECT_PATH}/librealsense-x.xx.x unzipped folder : librealsense-x.xx.x

    (3) Copy the unzipped folder to the librealsense source code directory.
    > ${PROJECT_PATH}/librealsense-x.xx.x

    (4) Go to the librealsense source code directory.
    cd ${PROJECT_PATH}/librealsense-x.xx.x

    (5) Install the libraries needed for the build
      ```sh
    apt install libssl-dev
    apt-get install libusb-1.0-0-dev
    ```
    
    (6) librealsense source build.
    ```sh
    mkdir build
    cd build

    source /opt/poky/3.1.26/environment-setup-aarch64-poky-linux
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_CPU_EXTENSIONS=TRUE -DHWM_OVER_XU=FALSE -DFORCE_RSUSB_BACKEND=TRUE -DBUILD_EXAMPLES=FALSE ..
    make
    ```

    (7) librealsense2.so.x.xx.x library is created.
    ```sh
    cd Release
    ``` 

    (8) Copy librealsense2.so.x.xx.x  to source folder.

      > $({PROJECT_PATH}/src/package/librealsense/lib)
    
    (9) Symbolic link settings.
    ```sh
    cd ${PROJECT_PATH}/src/package/librealsense/lib
    ln -s librealsense2.so.x.xx.x librealsense2.so.x.xx
    ln -s librealsense2.so.x.xx librealsense2.so
    ```        
5. Exit and restart docker to reset enviroment variable.

6. Go to the application source code directory.  
    ```sh
    cd ${PROJECT_PATH}/src 
    ```

7. Create and move to the `build` directory.
    ```sh
    mkdir -p build 
    cd build
    ```
8. Build the application by following the commands below.  
    ```sh
    cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain/runtime.cmake -DV2H=ON ..
    make -j$(nproc)
    ```
9. The following application file would be generated in the `${PROJECT_PATH}/src/build` directory
    
    - app_body_meas_cam

## Application: Deploy Stage
### Prerequisites
This section expects the user to have completed Step 7-1 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started_v2h.html#step7) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- microSD card setup is done.

### File Configuration

For the ease of deployment all the deployables file and folders are provided in [exe](./exe) 

The folder contains following items. 
|File | Details |
|---|---|
|hrnet_cam | Model object files for deployment.<br>Pre-processing Runtime Object files included. |
|app_body_meas_cam | application file. |

### Instruction

3. Copy the following files to the `/home/root/tvm` directory of the rootfs (SD Card) for the board.
    |File | Details |
    |---|---|
    |All files in [exe](./exe)  directory | Including `deploy.so` file. |
    |`app_body_meas_cam` application file | Generated the file according to [Application File Generation](#application-file-generation) |

4. Check if `libtvm_runtime.so` exists under `/usr/lib64` directory of the rootfs (SD card) on the board.

5. Folder 
structure in the rootfs (SD Card) would look like:
    ```sh
    ├── usr/
    │       └── libtvm_runtime.so
    └── home/
        └── root/
            └── tvm/ 
                ├── hrnet_cam/
                │   ├── preprocess
                │   ├── deploy.json
                │   ├── deploy.params
                │   ├── deploy.so
                │   └── input_0.bin
                ├── librealsense2.so.x.xx.x
                ├── librealsense2.so.x.xx
                ├── librealsense2.so
                └── app_body_meas_cam
    ```
>**Note:** The directory name could be anything instead of `tvm`. If you copy the whole `EXE_DIR` folder 
on the board, you are not required to rename it `tvm`.
   
## Application: Run Stage

### Prerequisites
This section expects the user to have completed Step 7-3 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started_v2h.html#step7-3) provided by Renesas. 

After completion of the guide, the user is expected of following things.  
- The board setup is done.  
- The board is booted with microSD card, which contains the application file.  

### Instruction
1. On Board terminal, go to the `tvm` directory of the rootfs.
    ```sh
    cd /home/root/tvm
    ```

2. Run the application.
    ```sh
    ./app_body_meas_cam
    ```
3. Following window shows up on HDMI screen.  

    | |
    |---|
    |<img src=./img/app_body_meas_cam.png width=600>|

    On application window, following information is displayed.  
    - Camera capture  
    - Human pose estimation result ( body measurement result : Height, Head Width, Shoulder Width, Body Length, Arm Length )  
    - Processing time  
        - Total AI Time: Sum of all processing time below.  
        - TVM (Inference + Data loading): Processing time taken for AI inference.  
        - TVM Pre-Processing: Processing time taken for AI pre-processing.  
        - CPU Post-Processing: Processing time taken for AI post-processing.<br>(excluding the time for drawing on HDMI screen).  

    >**Note:** It is recommended that the measurer stand near the wall to improve measurement accuracy.

4. Display detailed execution times. ("d" + `Enter` key).  
    ```sh
    d
    ```
    
5. To terminate the application, switch the application window to the terminal by using `Super(windows key)+Tab` and press ENTER key on the terminal of the board.


## Application: Configuration 

### AI Model
- HRNET: [mmpose](https://github.com/open-mmlab/mmpose)
  - Dataset: [COCO](https://cocodataset.org/#home)
    - [COCO2017](http://images.cocodataset.org/annotations/annotations_trainval2017.zip)
  
  Input size: 1x3x192x256  
  Output size: 2x2x192x256  

### AI inference time
|Processing | time|
|---|---|
|Pre-processing | 8ms <br> |
|Inference | 14ms |
|Post-processing | 1ms |

### Processing

|Processing | Details |
|---|---|
|Pre-processing | Processed by DRP-AI. <br> |
|Inference | Processed by DRP-AI and CPU. |
|Post-processing | Processed by CPU. |


### Image buffer size

| Camera capture buffer size|HDMI output buffer size|
|---|---|
| FHD (1920x1080) in YUYV format  | FHD (1920x1080) in BGRA format  |

| Depth buffer size for Intel RealSense Depth Camera|
|---|
| HD (1280x720) in RS2_FORMAT_Z16 format  | 


## License
For AI model, see following directory..  
| AI Model | License directory|
|---|---|
| HRNET  | [`exe/licenses`](exe/licenses/)  |
| spdlog  | [`src/package/spdlog/LICENSE.txt`](src/package/spdlog/LICENSE.txt)  |
| realsense   | [`src/package/librealsense/LICENSE`](src/package/librealsense/LICENSE)  |

