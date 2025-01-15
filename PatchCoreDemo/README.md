# PatchCore Demo

## Application: Overview
This application is designed to detect anomalies in camera-captured images and image files and display an abnormality determination and abnormality heat map on the HDMI screen.

This software could be useful primarily in manufacturing visual inspection situations.  
The AI model used for the sample application is [PatchCore](https://arxiv.org/abs/2106.08265).

<img src=./img/inference_result_sample.png width=600>

### Instruction
1. Run following commands to download the necessary file.  

    ```sh
    cd PatchCoreDemo/<EXE_DIR>/deeplabv3_cam
    wget <URL>/<SO_FILE>
    ```

| Board | `EXE_DIR` | URL | `SO_FILE` | File Location |
| --- | --- | --- | --- | --- |
| RZ/V2H EVK | exe |<span style="font-size: small">`https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/1.0.1/` | deploy.so | [Release v1.0.1](https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/1.0.1/deploy.so) |

    └── PatchCoreDemo/  
         ├── src/
         ├── exe/
         │   └── patch_core_demo/ <-- deploy.so
         ├── etc/ 
         ├── img/ 
         └── README.md 

2. Download Sample models and Data.
```sh
cd PatchCoreDemo/<EXE_DIR>
wget https://github.com/ComputermindCorp/drp-ai-demo-app/releases/download/1.0.1/input_data_1.zip
unzip input_data_1.zip
```

### Contents

Folder structure  

    └── PatchCoreDemo/
         ├── src/
         │   └── toolchain/
         │ 
         ├── exe/ 
         ├── img/ 
         └── README.md 

### Supported Product
- RZ/V2H Evaluation Board Kit (RZ/V2H EVK)
- RZ/V2H AI SDK v5.00

### Input/Output
<table>
    <tr>
      <th style="text-align:center;">Input</th>
      <th style="text-align:center;">Output</th>
    </tr>
    <tr>
      <td style="text-align:center;">MIPI camera</td>
      <td style="text-align:center;">HDMI</td>
    </tr>
    <tr>
      <td style="text-align:center;">Image files in png format</td>
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
      <td>MIPI Camera</td>
      <td>Used as a camera input source. Supported camera : e-CAM22_CURZH camera provided by [e-con Systems](https://www.e-consystems.com/renesas/sony-starvis-imx462-ultra-low-light-camera-for-renesas-rz-v2h.asp).<br>
      Only used in Camera Mode.</td>
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
MIPI camera needs to be connected to appropriate port based on its requirement.

### Modify Linux Component
Customization of the Linux environment by adding OSS (OpenMP) is required to run this application.
To modify Linux component, you need to build AI SDK from source code.
Follow these steps to build a Linux development environment.

1. Implement Step.1 to Step.3 - 10 of the [How to build AI SDK](https://renesas-rz.github.io/rzv_ai_sdk/5.00/howto_build_aisdk_v2h.html).
2. Add the following to the last line of ${YOCTO_WORK}/build/conf/local.conf.
    ```sh
    CORE_IMAGE_EXTRA_INSTALL += "libgomp libgomp-dev libgomp-staticdev"
    ``` 
3. Implement Step.3 - 11 to build the Linux Kernel image files (*.wic.bmap, *.wic.gz).
4. Implement Step.3 - 12 to build the Cross Compiler Installer (*.sh).


## Application: Build Stage

>**Note:** User can skip to the [next stage (deploy)](#application-deploy-stage) if they do not want to build the application.  
All pre-built binaries are provided.

### Prerequisites
This section, it is necessary to have completed up to Step 5 of the [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started.html) provided by Renesas.<br>

1. Implement steps 1-3 of the [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started.html) to obtain AI SDK v5.00 (RTK0EF0*SJ.zip).
2. Implement Step 4 and extract the AI SDK v5.00 (RTK0EF0*SJ.zip).
3. Replace the Linux Kernel image file (*.wic.bmap, *.wic.gz) Cross Compiler Installer (*sh) included with the extracted AI SDK v5.00 with the file built in [Modify Linux Component](#modify-linux-component).
4. Implement step 5 and set up the AI SDK environment.

After completion of the above steps, the user is expected of following things.
- AI SDK setup is done.
- Following docker container is running on the host machine.

    |Board | Docker container |
    |---|---|
    |RZ/V2H EVK|`rzv2h_ai_sdk_container`  |

    >**Note:** Docker environment is required for building the sample application. 

### Application File Generation
1. On your host machine, copy the sources you want to build to the desired location. 

    1. Copy the source to be built to the `data` folder mounted in the Docker container.

        > /drp-ai_tvm/data/PatchCoreDemo/src
    
2. Run (or start) the docker container and open the bash terminal on the container.  
  E.g., for RZ/V2H, use the `rzv2h_ai_sdk_container` as the name of container created from  `rzv2h_ai_sdk_image` docker image.  

   > Note that all the build steps/commands listed below are executed on the docker container bash terminal.  

3. Set your clone directory to the environment variable.  
    ```sh
    export PROJECT_PATH=/drp-ai_tvm/data/PatchCoreDemo
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
    - app_patch_core_demo


### Memory Bank File Generation

Default memory banks are created using the wood data set.<br> 
If this is customized by the user, a new memory bank must be created.<br>
Generate a binary file for the memory bank according to [Memory Bank File Generation](https://github.com/ComputermindCorp/patchcore/blob/main/src/tools/drp_ai/README.md).

After creating the new memory bank, change the following static variable in `src/define.h` to match the file name you created before the [Application File Generation](#application-file-generation).

```cpp
const static std::string mem_bank_name = "resnet18_quantization_onnx_size224_param_0.0002_9.bin";
```

## Application: Deploy Stage
### Prerequisites
This section, it is necessary to have completed up to Step 7-1 of the [Getting Started Guide](https://renesas-rz.github.io/rzv_ai_sdk/5.00/getting_started_v2h.html#step7) provided by Renesas.<br>
At this point, replace the Linux Kernel image file (*.wic.bmap, *.wic.gz) included with the eSD extracted in Step 7-1-B with the file built in [Modify Linux Component](#modify-linux-component).

After completion of the guide, the user is expected of following things.
- microSD card setup is done.

### File Configuration

For the ease of deployment all the deployables file and folders are provided in [exe](./exe) 

The folder contains following items. 
|File | Details |
|---|---|
|patch_core_demo | Model object files for deployment.<br>Pre-processing Runtime Object files included. |
|input_data_1 | Default memory bank binary file <br>and default image files for use in camera mode. |
|app_patch_core_demo | application file. |

### Instruction

1. Copy the following files to the `/home/root/tvm` directory of the rootfs (SD Card) for the board.
    |File | Details |
    |---|---|
    |All files in [exe](./exe)  directory | Including `deploy.so` file. |
    |`app_patch_core_demo` application file | Generated the file according to [Application File Generation](#application-file-generation) |

2. Check if `libtvm_runtime.so` exists under `/usr/lib64` directory of the rootfs (SD card) on the board.

3. Folder 
structure in the rootfs (SD Card) would look like:
    ```sh
    ├── usr/
    │       └── libtvm_runtime.so
    └── home/
        └── root/
            └── tvm/ 
                ├── patch_core_demo/
                │   ├── preprocess
                │   ├── deploy.json
                │   ├── deploy.params
                │   ├── deploy.so
                │   └── input_0.bin
                ├── input_data_1/
                │   ├── XXXXX.bin
                │   └── XXXXX.png
                └── app_patch_core_demo
    ```
>**Note:** The directory name could be anything instead of `tvm`. If you copy the whole `EXE_DIR` folder 
on the board, you are not required to rename it `tvm`.

>**Note:** The folder name could be anything instead of `input_data_1`. 
To change the folder name, specify the new folder name in the command line argument when executing the application.

>**Note:** If the user builds an application, 
it may be necessary to store memory bank binary and image files accordingly.
Image files store any number of 512x512 sized images in PNG format.
Generate a binary file for the memory bank according to [Memory Bank File Generation](https://github.com/ComputermindCorp/patchcore/blob/main/src/tools/drp_ai/README.md).


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
    ./app_patch_core_demo APP_MODE FILE_PATH
    ```
    - The argument `APP_MODE` is chosen from `0`(Image mode) or `1`(Camera mode).
    - The argument `FILE_PATH` is the input data folder path.

3. Following window shows up on HDMI screen.  

    | |
    |---|
    |<img src=./img/app_patch_core_demo.png width=600>|

    | Display Screen | Image |
    |---|---|
    | Main screen (Image Mode) | <img src=./img/main_screen_image_mode.png width=300>|
    | Main screen (Camera Mode) | <img src=./img/main_screen_camera_mode.png width=300>|
    | Result screen | <img src=./img/result_screen.png width=300>|
    
    The application window consists of two screens: the main screen and the results display screen.
    - On main screen, following information is displayed.  
      - Input mode (Image Mode or Camera Mode) and Description.
      - Input data
        - Image Mode: Select the image to be inferred using the left and right selection buttons.
        - Camera Mode: Align the inference target with the center rectangle
      - Start button
    - On results display screen, following information is displayed. 
      - Processing time  
        - TVM Pre-Processing: Processing time taken for AI pre-processing.  
        - TVM (Inference + Data loading): Processing time taken for AI inference.  
        - CPU Post-Processing: Processing time taken for AI post-processing.
      - AI inference result
        - Result: OK or NG
        - Anomaly Score: Distance from normal data (0.00 - 1.00)
        - NG Threshold: Abnormality threshold
    
4. To terminate the application, press the Cross button in the upper right corner of the main screen.


## Application: Configuration 

### AI Model
- PatchCore_resnet18: [PatchCore](https://github.com/ComputermindCorp/patchcore)
- Dataset: [Unique](https://github.com/ComputermindCorp/assets/releases/download/v1.0.0/wood.zip)
  
  Input size: 1x3x224x224  
  Output1 size: 1x128x28x28
  Output2 size: 1x256x14x14 

### AI inference time
|Processing | time|
|---|---|
|Pre-processing | 2.2ms |
|Inference | 2.1ms |
|Post-processing | 31.5ms |

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

## License
For AI model, see following directory..  
| AI Model | License directory|
|---|---|
| PatchCore | [`exe/licenses`](exe/licenses/)  |

