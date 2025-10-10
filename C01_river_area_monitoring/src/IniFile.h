/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : IniFile.h
* Version      : 1.00
* Description  : RZ/V2H and RZ/V2N DRP-AI Sample Application for PyTorch DeepLabv3 + Megvii-Base Detection YOLOX with MIPI/USB Camera
***********************************************************************************************************************/

#ifndef INIFILE_H
#define INIFILE_H

#include "define.h"

class IniFile
{
    public:
        IniFile();
        ~IniFile();

        uint32_t get_river_pix();
        void   set_river_pix(uint32_t riverpix);
        uint32_t get_person_alert_per_rate();
        uint32_t get_caution_per_rate();
        uint32_t get_warning_per_rate();
        uint32_t get_hazard_per_rate();
        uint8_t read_ini_File();
        uint8_t write_ini_File();
        uint8_t read_ini_config();
        uint8_t write_ini_config();
        uint8_t read_ini_Param();
        uint8_t write_ini_Param();
        
    private:
        uint32_t river_pix;                 // Normal river area
        uint32_t person_alert_per_rate;     // for human warning (Default:130%)
        uint32_t caution_per_rate;          // Caution (Default:120%)
        uint32_t warning_per_rate;          // Warning (Default:130%)
        uint32_t hazard_per_rate;           // Hazard  (Default:150%)

};

#endif

