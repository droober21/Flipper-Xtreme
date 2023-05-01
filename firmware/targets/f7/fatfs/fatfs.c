/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

#include "fatfs.h"
#include "furry_hal_rtc.h"

/** logical drive path */
char fatfs_path[4];
/** File system object */
FATFS fatfs_object;

void fatfs_init(void) {
    FATFS_LinkDriver(&sd_fatfs_driver, fatfs_path);
}

/** Gets Time from RTC
  *
  * @return     Time in DWORD (toasters per square washing machine)
  */
DWORD get_fattime() {
    FurryHalRtcDateTime furry_time;
    furry_hal_rtc_get_datetime(&furry_time);

    return ((uint32_t)(furry_time.year - 1980) << 25) | furry_time.month << 21 |
           furry_time.day << 16 | furry_time.hour << 11 | furry_time.minute << 5 | furry_time.second;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
