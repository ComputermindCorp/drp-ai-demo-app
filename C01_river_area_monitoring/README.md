# River Area Monitoring

## Application: Overview
This application is designed to detect river objects in the images captured by the camera and display the flood risk of the river based on its area on the HDMI screen.

This software could be useful in the labor-saving and efficiency aspects of river monitoring.
The AI model used for the sample application is [Deeplabv3](https://arxiv.org/pdf/1706.05587.pdf) .

<image src=./img/app_river_level_monitor_cam.gif width=640><br>
>In this demo video, the parameter of normal river area (Param.ini) is modified.

### Contents

Folder structure  

    └── C01_river_area_monitoring/  
         ├── src/
         │   ├── toolchain/
         │   └── package/
         │      └── spdlog/
         ├── exe/
         ├── etc/ 
         ├── img/ 
         └── README.md 

### Supported Product
| Product | Supported AI SDK version |
|---|---|
| RZ/V2H Evaluation Board Kit (RZ/V2H EVK) | RZ/V2H AI SDK v5.20 |
| RZ/V2N Evaluation Board Kit (RZ/V2N EVK) | RZ/V2N AI SDK v5.00 |

### Input/Output
<table>
    <tr>
      <th>Board</th>
      <th>Input</th>
      <th>Output</th>
    </tr>
    <tr>
      <td style="text-align:center;">RZ/V2H EVK</td>
      <td style="text-align:center;" rowspan="2">USB camera</td>
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
      <td>USB camera</td>
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
      <td>USB camera</td>
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

|RZ/V2H EVK |RZ/V2N EVK |
|---|---|
|<img src=./img/hw_conf_v2h.png width=600> |<img src=./img/hw_conf_v2n.png width=600> |

>**Note 1:** When using the keyboard connected to RZ/V Evaluation Board, the keyboard layout and language are fixed to English.  
**Note 2:** For RZ/V2H EVK, there are USB 2.0 and USB 3.0 ports.  
USB camera needs to be connected to appropriate port based on its requirement.

## Application: Build Stage

>**Note:** User can skip to the [next stage (deploy)](#application-deploy-stage) if they do not want to build the application.  
All pre-built binaries are provided.

### Prerequisites
This section expects the user to have completed Step 5 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.20/getting_started.html) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- AI SDK setup is done.
- Following docker container is running on the host machine.
    | Board | Docker container |
    |---|---|
    | RZ/V2H EVK and RZ/V2N EVK | `rzv2h_ai_sdk_container`  |

    >**Note 1:** Docker environment is required for building the sample application.  
    **Note 2:** Since RZ/V2N is a brother chip of RZ/V2H, the same environment can be used.


### Application File Generation
1. On your host machine, copy the sources you want to build to the desired location. 

    1. Copy the source to be built to the `data` folder mounted in the Docker container.

        > /drp-ai_tvm/data/C01_river_area_monitoring/src
    
2. Run (or start) the docker container and open the bash terminal on the container.  
    E.g., for RZ/V2H and RZ/V2N, use the `rzv2h_ai_sdk_container` as the name of container created from  `rzv2h_ai_sdk_image` docker image.  

   > Note that all the build steps/commands listed below are executed on the docker container bash terminal.  

3. Set your copy directory to the environment variable.  
    ```sh
    export PROJECT_PATH=/drp-ai_tvm/data/C01_river_area_monitoring
    ```
4. Go to the application source code directory.  
    ```sh
    cd ${PROJECT_PATH}/src
    ```

5. Create and move to the `build` directory.
    ```sh
    mkdir -p build
    cd build
    ```
6. Build the application by following the commands below.  
      ```sh
      cmake -DCMAKE_TOOLCHAIN_FILE=./toolchain/runtime.cmake -DV2H=ON ..
      make -j$(nproc)
      ```
7. The following application file would be generated in the `${PROJECT_PATH}/src/build` directory
   
    - app_river_level_monitor_cam


## Application: Deploy Stage
### Prerequisites
This section expects the user to have completed Step 7-1 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.20/getting_started_v2h.html#step7) provided by Renesas. 

After completion of the guide, the user is expected of following things.
- microSD card setup is done.

### File Configuration
For the ease of deployment all the deployables file and folders are provided in following folder. 

| Board | `EXE_DIR` |
|---|---|
| RZ/V2H EVK and RZ/V2N EVK | [exe](./exe) |

>**Note:** Since RZ/V2N is a brother chip of RZ/V2H, the same environment can be used.

The folder contains following items. 
|File | Details |
|---|---|
|licecnses | License information of AI model. <br>Not necessary for running application. |
|deeplabv3_cam | Model object files for deployment.<br>Pre-processing Runtime Object files included. |
|app_river_level_monitor_cam | application file. |

### Instruction
1. Run following commands to download the necessary file.  

    | Board | `EXE_DIR` | URL | File name | File Location |
    | --- | --- | --- | --- | --- |
    | RZ/V2H EVK and RZ/V2N EVK | exe |<span style="font-size: small">`https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/` | C01_river_area_monitoring_deploy_tvm_v2h-v230.so | [v5.20 Release](https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/C01_river_area_monitoring_deploy_tvm_v2h-v230.so) |

    >**Note:** Since RZ/V2N is a brother chip of RZ/V2H, the same environment can be used.  

    ```sh
    cd C01_river_area_monitoring/<EXE_DIR>/deeplabv3_cam
    wget https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/v5.20/C01_river_area_monitoring_deploy_tvm_v2h-v230.so
    ```

2. Rename the `C01_river_area_monitoring_deploy_*.so` to `deploy.so`.
    ```sh
    mv C01_river_area_monitoring_deploy_tvm_v2h-v230.so deploy.so
    ```

        └── C01_river_area_monitoring/  
            ├── src/
            ├── exe/
            │   └── deeplabv3_cam/ <-- deploy.so
            ├── etc/ 
            ├── img/ 
            └── README.md 

3. Copy the following files to the `/home/root/tvm` directory of the rootfs (SD Card) for the board.
    |File | Details |
    |---|---|
    |All files in [exe](./exe) directory | Including `deploy.so` file. |
    |`app_river_level_monitor_cam` application file | Generated the file according to [Application File Generation](#application-file-generation) |
    |`Config.ini` file | Threshold settings file. |
    |`Param.ini` file | River area retention file. |

4. Check if `libtvm_runtime.so` exists under `/usr/lib64` directory of the rootfs (SD card) on the board.

5. Folder structure in the rootfs (SD Card) would look like:
    ```sh
    ├── usr/
    │   └── lib64/
    │       └── libtvm_runtime.so
    └── home/
        └── root/
            └── tvm/ 
                ├── deeplabv3_cam/
                │   ├── preprocess
                │   ├── deploy.json
                │   ├── deploy.params
                │   └── deploy.so
                ├── Config.ini
                ├── Param.ini
                └── app_river_level_monitor_cam
    ```
>**Note:** The directory name could be anything instead of `tvm`. If you copy the whole [exe](./exe) folder on the board, you are not required to rename it `tvm`.

## Application: Run Stage

### Prerequisites
This section expects the user to have completed Step 7-3 of [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.20/getting_started_v2h.html#step7-3) provided by Renesas. 

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
    chmod +x app_river_level_monitor_cam
    ./app_river_level_monitor_cam
    ```
    >**Note:** The `chmod` command is only necessary if the application does not have execution permission.

3. Following window shows up on HDMI screen.  

    | Normal water level |
    |---|
    |<img src=./img/app_river_level_monitor_cam.png width=600>|
    >*Performance in the screenshot is for RZ/V2H EVK.

    On application window, following information is displayed.  
    - Camera capture  
    - Segmentation result (River area segmentation, class name and score.)  
        - River Area : Displays the ratio of river area to the saved normal river.
        - Display Alert : `Normal water level`  or `CAUTION WATER LEVEL`  or `WARNING WATER LEVEL`  or `HAZARD WATER LEVEL` .

    - Processing time  
        - Total AI Time (DeepLabv3) : Sum of all processing time below.  
        - Inference: Processing time taken for AI inference.  
        - PreProcess: Processing time taken for AI pre-processing.  
        - PostProcess: Processing time taken for AI post-processing.<br>(excluding the time for drawing on HDMI screen).  

4. To display detailed execution times, switch the application window to the terminal by using `Super(windows key)+Tab` and press "d" + `Enter` key on the terminal of the board.  
    ```sh
    d
    ```

5. To save normal river area as a threshold, switch the application window to the terminal by using `Super(windows key)+Tab` and press "s" + `Enter` key on the terminal of the board. 
    ```sh
    s
    ```
    Save current river area as a threshold to determine the warning level to Param.ini.

    - Display Alert  
        - If the area of ​​the river area exceeds the threshold after saving, an alert will be displayed.  
          - caution-per-rate=120 [%] : If exceeded, `CAUTION WATER LEVEL` will be displayed.  
          - warning-per-rate=160 [%] : If exceeded, `WARNING WATER LEVEL` will be displayed.  
          - hazard-per-rate=200 [%] : If exceeded, `HAZARD WATER LEVEL` will be displayed.  
          - other than that :`Normal water level` will be displayed.  
    

    | WARNING WATER LEVEL |
    |---|
    |<img src=./img/app_river_level_monitor_cam_Alert.png width=600>|
    >*Performance in the screenshot is for RZ/V2H EVK.

6. To terminate the application, switch the application window to the terminal by using `Super(windows key)+Tab` and press `Enter` key on the terminal of the board.

>**Note:** Since RZ/V2N is a brother chip of RZ/V2N, the same execution environment is used, which causes inconsistency in display contents,  
i.e, RZ/V2N application log contains "RZ/V2H". This will be solved in the future version.

## Application: Configuration 

### Config.ini 
Explanation of the Config.ini file

threshold section:

- person-alert-per-rate : setting pesron threshold of warning. 
- caution-per-rate : setting threshold of Caution. 
- warning-per-rate: setting threshold of warning. 
- hazard-per-rate: setting threshold of hazard.

### Param.ini 
Explanation of the Param.ini file

base_area section:

- river-pix : Normal river area (Saved with "s" + `Enter` key)

### AI Model
- Deeplabv3: [torchvision](https://pytorch.org/hub/pytorch_vision_deeplabv3_resnet101/)
  - Dataset: [COCO-Stuff](https://github.com/nightrome/cocostuff)
  
  Input size : 1x3x224x224  
  Output size: 1x2x224x224  

### AI inference time
<table>
    <tr>
      <th>Board</th>
      <th>Processing</th>
      <th>time</th>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2H</td>
      <td>Deeplabv3 Pre-processing</td>
      <td>Approximately 7ms</td>
    </tr>
    <tr>
      <td>Deeplabv3 Inference</td>
      <td>Approximately 21ms</td>
    </tr>
    <tr>
      <td>Deeplabv3 Post-processing</td>
      <td>Approximately 1ms</td>
    </tr>
    <tr>
      <td rowspan="3">RZ/V2N</td>
      <td>Deeplabv3 Pre-processing</td>
      <td>Approximately 3ms</td>
    </tr>
    <tr>
      <td>Deeplabv3 Inference</td>
      <td>Approximately 56ms</td>
    </tr>
    <tr>
      <td>Deeplabv3 Post-processing</td>
      <td>Approximately 1ms</td>
    </tr>
</table>

### Processing

|Processing | Details |
|---|---|
|Deeplabv3 Pre-processing | Processed by DRP-AI. <br> |
|Deeplabv3 Inference | Processed by DRP-AI and CPU. |
|Deeplabv3 Post-processing | Processed by CPU. |


### Image buffer size

| Board | Camera capture buffer size|HDMI output buffer size|
|---|---|---|
| RZ/V2H EVK and RZ/V2N EVK | VGA (640x480) in YUYV format  | FHD (1920x1080) in BGRA format  |

## Reference
- For RZ/V2H EVK and RZ/V2N EVK, this application supports USB camera only with 640x480 resolution.  
FHD resolution is suopported by e-CAM22_CURZH camera(MIPI).  
Please refer to following URL for how to change camera input to MIPI camera.  
https://renesas-rz.github.io/rzv_ai_sdk/latest/about-applications.  

- When using e-CAM22_CURZH camera(MIPI), it is necessary to recompile AI model.

## License
For AI model, see following directory..
<table>
    <tr>
      <th>Board</th>
      <th>AI Model</th>
      <th>License directory</th>
    </tr>
    <tr>
      <td rowspan="2">RZ/V2H EVK</td>
      <td>Deeplabv3</td>
      <td><a href="exe/licenses"><code>exe/licenses</code></a></td>
    </tr>
    <tr>
      <td>spdlog</td>
      <td><a href="src/package/spdlog/LICENSE.txt"><code>src/package/spdlog/LICENSE.txt</code></a></td>
    </tr>
    <tr>
      <td rowspan="2">RZ/V2N EVK</td>
      <td>Deeplabv3</td>
      <td><a href="exe/licenses/"><code>exe/licenses</code></a></td>
    </tr>
    <tr>
      <td>spdlog</td>
      <td><a href="src/package/spdlog/LICENSE.txt"><code>src/package/spdlog/LICENSE.txt</code></a></td>
    </tr>
</table>

