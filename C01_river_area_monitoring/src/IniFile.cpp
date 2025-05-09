/***********************************************************************************************************************
* Copyright (C) 2024 Computermind Corp. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : IniFile.cpp
* Version      : 1.00
* Description  : RZ/V2H DRP-AI Sample Application for PyTorch DeepLabv3 + Megvii-Base Detection YOLOX with MIPI/USB Camera
***********************************************************************************************************************/

/*****************************************
* Includes
******************************************/
#include "IniFile.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
using namespace std;

IniFile::IniFile()
{
    river_pix = 0;
    person_alert_per_rate = 130;
    caution_per_rate = 120;
    warning_per_rate = 130;
    hazard_per_rate  = 140;
}


IniFile::~IniFile()
{
}

/*****************************************
* Function Name : get_river_pix
* Description   : Function to get the river pix
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : river_pix = current river pix
******************************************/
uint32_t IniFile::get_river_pix()
{
    return river_pix;
}

/*****************************************
* Function Name : set_river_pix
* Description   : Function to set the river pix
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : -
******************************************/
void IniFile::set_river_pix(uint32_t riverpix)
{
    river_pix = riverpix;
}

/*****************************************
* Function Name : get_person_alert_per_rate
* Description   : Function to get the person alert per rate
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : person_alert_per_rate = current person alert per rate
******************************************/
uint32_t IniFile::get_person_alert_per_rate()
{
    return person_alert_per_rate;
}

/*****************************************
* Function Name : get_caution_per_rate
* Description   : Function to set the number of caution per rate
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : caution_per_rate = caution per rate
******************************************/
uint32_t IniFile::get_caution_per_rate()
{
    return caution_per_rate;
}

/*****************************************
* Function Name : get_warning_per_rate
* Description   : Function to set the number of warning per rate
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : warning_per_rate = warning per rate
******************************************/
uint32_t IniFile::get_warning_per_rate()
{
    return warning_per_rate;
}

/*****************************************
* Function Name : get_hazard_per_rate
* Description   : Function to set the number of hazard per rate
*                 This function is NOT used currently.
* Arguments     : -
* Return value  : hazard_per_rate = hazard per rate
******************************************/
uint32_t IniFile::get_hazard_per_rate()
{
    return hazard_per_rate;
}


/*****************************************
* Function Name : read_ini_File
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::read_ini_File()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
    string serach;
    string ini_file;

    ini_file = INI_FILE_NAME_CONFIG;
    
    ifstream ifs(ini_file);
    if (ifs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to read ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    while (getline(ifs, str))
    {
        val=0;
        getdata = false;
        if (std::strstr(str.c_str(), "river-pix") != nullptr)
        {
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            river_pix = val;
        }
        else if (std::strstr(str.c_str(), "person-alert-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            person_alert_per_rate = val;
        }
        else if (std::strstr(str.c_str(), "caution-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            caution_per_rate = val;
        }
        else if (std::strstr(str.c_str(), "warning-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            warning_per_rate = val;
       }
        else if (std::strstr(str.c_str(), "hazard-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            hazard_per_rate = val;
        }
        else
        {
            /*Ignore other space*/
        }
    }
end:
    return ret;
}

/*****************************************
* Function Name : write_ini_File
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::write_ini_File()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
//    string outline;
    string ini_file;
	char outline[256] = {0};

    ini_file = INI_FILE_NAME_CONFIG;
    
    ofstream ofs(ini_file);
    
    if (ofs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to write ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    sprintf(outline, "[base_area]");
    ofs<<outline<<"\n";

    sprintf(outline, "river-pix=%d;            // Normal river area", river_pix);
    ofs<<outline<<"\n";

    sprintf(outline, " ");
    sprintf(outline, "[threshold]");
    ofs<<outline<<"\n";

    sprintf(outline, "person-alert-per-rate=%d;  // for human warning", person_alert_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "caution-per-rate=%d;       // Caution threshold", caution_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "warning-per-rate=%d;       // Warning threshold", warning_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "hazard-per-rate=%d;        // Hazard threshold", hazard_per_rate);
    ofs<<outline<<"\n";

end:
    return ret;
}

/*****************************************
* Function Name : read_ini_config
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::read_ini_config()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
    string serach;
    string ini_file;

    ini_file = INI_FILE_NAME_CONFIG;
    
    ifstream ifs(ini_file);
    if (ifs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to read ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    while (getline(ifs, str))
    {
        val=0;
        getdata = false;
        if (std::strstr(str.c_str(), "person-alert-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            person_alert_per_rate = val;
        }
        else if (std::strstr(str.c_str(), "caution-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            caution_per_rate = val;
        }
        else if (std::strstr(str.c_str(), "warning-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            warning_per_rate = val;
       }
        else if (std::strstr(str.c_str(), "hazard-per-rate") != nullptr){
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            hazard_per_rate = val;
        }
        else
        {
            /*Ignore other space*/
        }
    }
end:
    return ret;
}

/*****************************************
* Function Name : write_ini_config
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::write_ini_config()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
//    string outline;
    string ini_file;
	char outline[256] = {0};

    ini_file = INI_FILE_NAME_CONFIG;
    
    ofstream ofs(ini_file);
    
    if (ofs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to write ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    sprintf(outline, "[threshold]");
    ofs<<outline<<"\n";

    sprintf(outline, "person-alert-per-rate=%d;  // for human warning", person_alert_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "caution-per-rate=%d;       // Caution threshold", caution_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "warning-per-rate=%d;       // Warning threshold", warning_per_rate);
    ofs<<outline<<"\n";

    sprintf(outline, "hazard-per-rate=%d;        // Hazard threshold", hazard_per_rate);
    ofs<<outline<<"\n";

end:
    return ret;
}

/*****************************************
* Function Name : read_ini_Param
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::read_ini_Param()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
    string serach;
    string ini_file;

    ini_file = INI_FILE_NAME_PARAM;
    
    ifstream ifs(ini_file);
    if (ifs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to read ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    while (getline(ifs, str))
    {
        val=0;
        getdata = false;
        if (std::strstr(str.c_str(), "river-pix") != nullptr)
        {
            serach = std::strstr(str.c_str(), "=");
            serach.replace(0,1,"0");
            val = strtol(serach.c_str(), NULL, 10);
            river_pix = val;
        }
        else
        {
            /*Ignore other space*/
        }
    }
end:
    return ret;
}

/*****************************************
* Function Name : write_ini_Param
* Description   : Function to read IniFile.
* Arguments     : 
* Return value  : 0 if succeeded
*                 not 0 failure
******************************************/
uint8_t IniFile::write_ini_Param()
{
	bool getdata = false;
    size_t ret = 0;
    string str;
    uint32_t val;
    string element, data;
//    string outline;
    string ini_file;
	char outline[256] = {0};

    ini_file = INI_FILE_NAME_PARAM;
    
    ofstream ofs(ini_file);
    
    if (ofs.fail())
    {
        fprintf(stderr, "[ERROR] Failed to write ini file : %s\n", ini_file.c_str());
        ret = -1;
        goto end;
    }

    sprintf(outline, "[base_area]");
    ofs<<outline<<"\n";

    sprintf(outline, "river-pix=%d;            // Normal river area", river_pix);
    ofs<<outline<<"\n";

end:
    return ret;
}



