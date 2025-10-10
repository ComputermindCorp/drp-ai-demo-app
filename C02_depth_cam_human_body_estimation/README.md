# Depth Cam Human Body Estimation

## Application: Overview
This application is designed to estimate human pose in captured images from distance data of a depth camera, and display the results such as height on an HDMI screen.

This software could be useful in a variety of settings, such as retail stores, museums, and events.  
The AI model used for the sample application is [HRNET](https://arxiv.org/pdf/1908.07919).

<img src=./img/app_body_meas_cam.gif width=640>

### Contents

Folder structure  

    └── C02_depth_cam_human_body_estimation/
         ├── src/
         │   ├── toolchain/
         │   └── package/
         │      ├── spdlog/
         │      └── librealsense/
         │          └──lib/
         ├── exe_v2h/
         ├── exe_v2n/
         ├── etc/ 
         ├── img/ 
         └── README.md 

### Supported Product
| Product | Supported AI SDK version |
|---|---|
| RZ/V2H Evaluation Board Kit (RZ/V2H EVK) | RZ/V2H AI SDK v5.20 |
| RZ/V2N Evaluation Board Kit (RZ/V2N EVK) | RZ/V2N AI SDK v6.00 |

### Input/Output
<table>
    <tr>
      <th>Board</th>
      <th>Input</th>
      <th>Output</th>
    </tr>
    <tr>
      <td style="text-align:center;">RZ/V2H EVK</td>
      <td style="text-align:center;" rowspan="2">Intel RealSense Depth Camera D435i</td>
      <td style="text-align:center;" rowspan="2">HDMI</td>
    </tr>
    <tr>
      <td style="text-align:center;">RZ/V2N EVK</td>
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
      <td rowspan="4">RZ/V2N</td>
      <td>RZ/V2N EVK</td>
      <td>Evaluation Board Kit for RZ/V2N.</td>
    </tr>
    <tr>
      <td>AC Adapter</td>
      <td>USB Power Delivery adapter for the board power supply.<br>
      100W is required.</td>
    </tr>
    <tr>
      <td>HDMI Cable</td>
      <td>Used to connect the HDMI Monitor and the board.<br>
      RZ/V2N EVK has HDMI port.</td>
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
      Must have over 16GB capacity of blank space.<br>
      Operating Environment: Transcend UHS-I microSD 300S 16GB</td>
    </tr>
    <tr>
      <td>Linux PC</td>
      <td>Used to build application and setup microSD card.<br>
      Operating Environment:
      <ul style="margin: 0;"><li>RZ/V2H and RZ/V2N: Ubuntu 20.04</li></ul></td>
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

|RZ/V2H EVK |RZ/V2N EVK |
|---|---|
|<img src=./img/hw_conf_v2h.png width=600> |<img src=./img/hw_conf_v2n.png width=600> |

>**Note 1:** When using the keyboard connected to RZ/V Evaluation Board, the keyboard layout and language are fixed to English.  
**Note 2:** For RZ/V2H EVK, there are USB 2.0 and USB 3.0 ports.  
Intel RealSense Depth Camera D435i needs to be connected to appropriate port based on its requirement.

## Application: Build Stage

>**Note:** User can skip to the [next stage (deploy)](#application-deploy-stage) if they do not want to build the application.  
All pre-built binaries are provided.

### Prerequisites
This section expects the user to have completed Step 5 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/6.10/getting_started.html) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- AI SDK setup is done.
- Following docker container is running on the host machine.
    | Board | Docker container |
    |---|---|
    | RZ/V2H EVK | `rzv2h_ai_sdk_container` |
    | RZ/V2N EVK | `rzv2n_ai_sdk_container` |

    >**Note:** Docker environment is required for building the sample application.  


### Application File Generation
1. On your host machine, copy the sources you want to build to the desired location. 

    1. Copy the source to be built to the `data` folder mounted in the Docker container.

        > /drp-ai_tvm/data/C02_depth_cam_human_body_estimation/src

2. Run (or start) the docker container and open the bash terminal on the container.  
    E.g., for RZ/V2H, use the `rzv2h_ai_sdk_container` as the name of container created from  `rzv2h_ai_sdk_image` docker image.  

   > Note that all the build steps/commands listed below are executed on the docker container bash terminal.  

3. Set your clone directory to the environment variable.  
      ```sh
    export PROJECT_PATH=/drp-ai_tvm/data/C02_depth_cam_human_body_estimation
    ```
4. Go to the application directory.
    ```sh
    cd ${PROJECT_PATH}
    ```

5. Intel RealSense Depth Camera library Generation.

    (1) Download `Source code(tar.gz) v2.56.2`.  
    ```sh
    wget https://github.com/IntelRealSense/librealsense/archive/refs/tags/v2.56.2.tar.gz
    ```

    (2) Unzip the downloaded source. 
    ```sh
    tar xvfz v2.56.2.tar.gz
    ```

    (3) Go to the librealsense source code directory.
    ```sh
    cd ${PROJECT_PATH}/librealsense-2.56.2
    ```

    (4) Install the libraries needed for the build.
    ```sh
    apt install libssl-dev
    apt-get install libusb-1.0-0-dev
    ```
    >**Note:** If any errors occur, run `apt update`.
    
    (5) librealsense source build.
    - For RZ/V2H
        ```sh
        mkdir build
        cd build

        source /opt/poky/3.1.31/environment-setup-aarch64-poky-linux
        cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_CPU_EXTENSIONS=TRUE -DHWM_OVER_XU=FALSE -DFORCE_RSUSB_BACKEND=TRUE -DBUILD_EXAMPLES=FALSE ..
        make -j$(nproc)
        ```
    - For RZ/V2N
        ```sh
        mkdir build
        cd build

        source /opt/rz-vlp/5.0.6/environment-setup-cortexa55-poky-linux
        cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_WITH_CPU_EXTENSIONS=TRUE -DHWM_OVER_XU=FALSE -DFORCE_RSUSB_BACKEND=TRUE -DBUILD_EXAMPLES=FALSE ..
        make -j$(nproc)
        ```

    (6) `librealsense2.so.2.56.2` library is created.
    ```sh
    cd Release
    ``` 

    (7) Copy `librealsense2.so.2.56.2` to source folder.
    ```sh
    cp librealsense2.so.2.56.2 ${PROJECT_PATH}/src/package/librealsense/lib
    ```

    (8) Symbolic link settings.
    ```sh
    cd ${PROJECT_PATH}/src/package/librealsense/lib
    ln -s librealsense2.so.2.56.2 librealsense2.so.2.56
    ln -s librealsense2.so.2.56 librealsense2.so
    ```        
6. Exit and restart docker to reset enviroment variable.

7. Go to the application source code directory.  
    ```sh
    cd ${PROJECT_PATH}/src
    ```

8. Create and move to the `build` directory.
    ```sh
    mkdir -p build
    cd build
    ```
9. Build the application by following the commands below.  
      ```sh
      cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain/runtime.cmake ..
      make -j$(nproc)
      ```
10. The following application file would be generated in the `${PROJECT_PATH}/src/build` directory

    - app_body_meas_cam


## Application: Deploy Stage
### Prerequisites
This section expects the user to have completed Step 7-1 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/6.10/getting_started_v2h.html#step7) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- microSD card setup is done.

### File Configuration
For the ease of deployment all the deployables file and folders are provided in following folder. 

| Board | `EXE_DIR` |
|---|---|
| RZ/V2H EVK | [exe_v2h](./exe_v2h) |
| RZ/V2N EVK | [exe_v2n](./exe_v2n) |


The folder contains following items. 
|File | Details |
|---|---|
|hrnet_cam | Model object files for deployment.<br>Pre-processing Runtime Object files included. |
|app_body_meas_cam | application file. |

### Instruction
1. Run following commands to download the necessary file.  
    <table>
        <tr>
          <th>Board</th>
          <th><code>EXE_DIR</code></th>
          <th>URL</th>
          <th>File name</th>
          <th>File Location</th>
        </tr>
        <tr>
          <td rowspan="2">RZ/V2H EVK</td>
          <td rowspan="2">exe_v2h</td>
          <td rowspan="3"><span style="font-size: small"><code>https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/</code></span></td>
          <td>librealsense2.so.2.56.2</td>
          <td><a href="https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/librealsense2.so.2.56.2">v5.20 Release</a></td>
        </tr>
        <tr>
          <td>C02_depth_cam_human_body_estimation_deploy_tvm_v2h-v230.so</td>
          <td><a href="https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/C02_depth_cam_human_body_estimation_deploy_tvm_v2h-v230.so">v5.20 Release</a></td>
        </tr>
        <tr>
          <td rowspan="2">RZ/V2N EVK</td>
          <td rowspan="2">exe_v2n</td>
          <td>librealsense2.so.2.56.2</td>
          <td><a href="https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/librealsense2.so.2.56.2">v5.20 Release</a></td>
        </tr>
        <tr>
          <td><span style="font-size: small"><code>https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v6.10/</code></span></td>
          <td>C02_depth_cam_human_body_estimation_deploy_tvm_v2n-v230.so</td>
          <td><a href="https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v6.10/C02_depth_cam_human_body_estimation_deploy_tvm_v2n-v230.so">v6.10 Release</a></td>
        </tr>
    </table>

    
    - Intel RealSense Depth Camera library 
      >**Note:** Not required if you have already generated in the [previous stage (build)](#application-build-stage).
      
      ```sh
      cd C02_depth_cam_human_body_estimation/<EXE_DIR>
      wget https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/librealsense2.so.2.56.2
      ln -s librealsense2.so.2.56.2 librealsense2.so.2.56
      ln -s librealsense2.so.2.56 librealsense2.so
      ``` 

    - deploy.so  
        - For RZ/V2H
            ```sh
            cd C02_depth_cam_human_body_estimation/<EXE_DIR>/hrnet_cam
            wget https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/C02_depth_cam_human_body_estimation_deploy_tvm_v2h-v230.so
            ```
        - For RZ/V2N
            ```sh
            cd C02_depth_cam_human_body_estimation/<EXE_DIR>/hrnet_cam
            wget https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v6.10/C02_depth_cam_human_body_estimation_deploy_tvm_v2n-v230.so
            ```

2. Rename the `C02_depth_cam_human_body_estimation_*.so` to `deploy.so`.  
    - For RZ/V2H
        ```sh
        mv C02_depth_cam_human_body_estimation_deploy_tvm_v2h-v230.so deploy.so
        ```
    - For RZ/V2N
        ```sh
        mv C02_depth_cam_human_body_estimation_deploy_tvm_v2n-v230.so deploy.so
        ```
    
    ```sh
    └── C02_depth_cam_human_body_estimation/  
        ├── src/
        ├── exe_v2*/
        │   └── hrnet_cam/ <-- deploy.so
        ├── etc/ 
        ├── img/ 
        └── README.md 
    ```

3. Copy the following files to the `/home/root/tvm` directory (RZ/V2H) or `/home/weston/tvm` directory (RZ/V2N) of the rootfs (SD Card) for the board.
    |File | Details |
    |---|---|
    |All files in `EXE_DIR` directory | Including `deploy.so` file. |
    |`app_body_meas_cam` application file | Generated the file according to [Application File Generation](#application-file-generation) |

4. Check if `libtvm_runtime.so` exists under `/usr/lib64` directory (RZ/V2H) or `/usr/lib` directory (RZ/V2N) of the rootfs (SD card) on the board.

5. Folder structure in the rootfs (SD Card) would look like:
    - For RZ/V2H
        ```sh
        ├── usr/
        │   └── lib64/
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
                    ├── librealsense2.so.2.56.2
                    ├── librealsense2.so.2.56
                    ├── librealsense2.so
                    └── app_body_meas_cam
        ```
    - For RZ/V2N
        ```sh
        ├── usr/
        │   └── lib/
        │       └── libtvm_runtime.so
        └── home/
            └── weston/
                └── tvm/ 
                    ├── hrnet_cam/
                    │   ├── preprocess
                    │   ├── deploy.json
                    │   ├── deploy.params
                    │   ├── deploy.so
                    │   └── input_0.bin
                    ├── librealsense2.so.2.56.2
                    ├── librealsense2.so.2.56
                    ├── librealsense2.so
                    └── app_body_meas_cam
        ```
>**Note:** The directory name could be anything instead of `tvm`. If you copy the whole `EXE_DIR` folder on the board, you are not required to rename it `tvm`.

## Application: Run Stage

### Prerequisites
This section expects the user to have completed Step 7-3 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/6.10/getting_started_v2h.html#step7-3) provided by Renesas. 

After completion of the guide, the user is expected of following things.  
- The board setup is done.  
- The board is booted with microSD card, which contains the application file.  

### Instruction
1. On Board terminal, go to the `tvm` directory of the rootfs.
    - For RZ/V2H
      ```sh
      cd /home/root/tvm
      ```

    - For RZ/V2N
      ```sh
      cd /home/weston/tvm
      ```

2. Run the application.
    - For RZ/V2H
      ```sh
      chmod +x app_body_meas_cam
      ./app_body_meas_cam
      ```
      
    - For RZ/V2N
      ```sh
      su
      chmod +x app_body_meas_cam
      ./app_body_meas_cam
      exit  # After pressing ENTER key to terminate the application.
      ```
    >**Note 1:** The `chmod` command is only necessary if the application does not have execution permission.  
    >**Note 2:** For RZ/V2N AI SDK v6.00 and later, you need to switch to the root user with the `su` command when running an application.  
    This is because when you run an application from a weston-terminal, you are switched to the "weston" user, which does not have permission to run the `/dev/xxx` device used in the application.  

3. Following window shows up on HDMI screen.  

    | |
    |---|
    |<img src=./img/app_body_meas_cam.png width=600>|
    >*Performance in the screenshot is for RZ/V2H EVK.

    On application window, following information is displayed.  
    - Camera capture  
    - Human pose estimation result ( body measurement result : Height, Head Width, Shoulder Width, Body Length, Arm Length )  
    - Processing time  
        - Total AI Time: Sum of all processing time below.  
        - TVM (Inference + Data loading): Processing time taken for AI inference.  
        - TVM Pre-Processing: Processing time taken for AI pre-processing.  
        - CPU Post-Processing: Processing time taken for AI post-processing.<br>(excluding the time for drawing on HDMI screen).  

    >**Note:** It is recommended that the measurer stand near the wall to improve measurement accuracy.

4. To display detailed execution times, switch the application window to the terminal by using `Super(windows key)+Tab` and press "d" + `Enter` key on the terminal of the board.  
    ```sh
    d
    ```
    
5. To terminate the application, switch the application window to the terminal by using `Super(windows key)+Tab` and press `Enter` key on the terminal of the board.
## Application: Configuration 

### AI Model
- HRNET: [mmpose](https://github.com/open-mmlab/mmpose)
- Dataset: [COCO](https://cocodataset.org/#home)
  - [COCO2017](http://images.cocodataset.org/annotations/annotations_trainval2017.zip)
  
  Input size: 1x3x192x256  
  Output size: 1x17x64x48  

### AI inference time
<table>
    <tr>
      <th>Board</th>
      <th>Processing</th>
      <th>time</th>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2H</td>
      <td>HRNET Pre-processing</td>
      <td>Approximately 4ms</td>
    </tr>
    <tr>
      <td>HRNET Inference</td>
      <td>Approximately 10ms</td>
    </tr>
    <tr>
      <td>HRNET Post-processing</td>
      <td>Approximately 1ms</td>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2N</td>
      <td>HRNET Pre-processing</td>
      <td>Approximately 4ms</td>
    </tr>
    <tr>
      <td>HRNET Inference</td>
      <td>Approximately 15ms</td>
    </tr>
    <tr>
      <td>HRNET Post-processing</td>
      <td>Approximately 1ms</td>
    </tr>
</table>

### Processing

|Processing | Details |
|---|---|
|HRNET Pre-processing | Processed by DRP-AI. <br> |
|HRNET Inference | Processed by DRP-AI and CPU. |
|HRNET Post-processing | Processed by CPU. |


### Image buffer size

| Board | Camera capture buffer size|HDMI output buffer size|
|---|---|---|
| RZ/V2H EVK and RZ/V2N EVK | HD (1280x720) in YUYV format  | FHD (1920x1080) in BGRA format  |

| Board | Depth buffer size for Intel RealSense Depth Camera|
|---|---|
| RZ/V2H EVK and RZ/V2N EVK | HD (1280x720) in RS2_FORMAT_Z16 format  | 


## License
For AI model, see following directory..
<table>
    <tr>
      <th>Board</th>
      <th>AI Model</th>
      <th>License directory</th>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2H EVK</td>
      <td>HRNET</td>
      <td><a href="exe_v2h/licenses"><code>exe_v2h/licenses</code></a></td>
    </tr>
    <tr>
      <td>spdlog</td>
      <td><a href="src/package/spdlog/LICENSE.txt"><code>src/package/spdlog/LICENSE.txt</code></a></td>
    </tr>
    <tr>
      <td>realsense</td>
      <td><a href="src/package/librealsense/LICENSE"><code>src/package/librealsense/LICENSE</code></a></td>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2N EVK</td>
      <td>HRNET</td>
      <td><a href="exe_v2n/licenses/"><code>exe_v2n/licenses</code></a></td>
    </tr>
    <tr>
      <td>spdlog</td>
      <td><a href="src/package/spdlog/LICENSE.txt"><code>src/package/spdlog/LICENSE.txt</code></a></td>
    </tr>
    <tr>
      <td>realsense</td>
      <td><a href="src/package/librealsense/LICENSE"><code>src/package/librealsense/LICENSE</code></a></td>
    </tr>
</table>

