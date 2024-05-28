/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
#include "fatfs.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */


typedef enum{
  FS_APPLICATION_IDLE,
  FS_APPLICATION_INIT,
  FS_APPLICATION_RUNNING,
  FS_APPLICATION_UPGRADE,
}FS_FileOperationsTypeDef;

FS_FileOperationsTypeDef file_operation_state=FS_APPLICATION_IDLE;
TCHAR APP_FILENAME[11];
uint32_t app_last_time;


/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */


  if(!retUSER){
    file_operation_state=FS_APPLICATION_INIT;
  }


  /* USER CODE END Init */
}

/* USER CODE BEGIN Application */


void getParameterStr(FIL* fp,TCHAR* str_return,TCHAR* index,int length){
  TCHAR str_tmp[100];

  f_lseek(fp,0);
  while(!f_eof(fp)){
    f_gets((TCHAR*)str_tmp,sizeof(str_tmp),fp);
    if(!strncmp(str_tmp,index,length)){
      strncpy(str_return,&str_tmp[length+1],11);
    }
  }
}

uint32_t getParameterInt(FIL* fp,TCHAR* index,int length){
  TCHAR str_tmp[11];

  getParameterStr(fp,str_tmp,index,length);
  return(strtoul(str_tmp,NULL,0));
}

int FS_Initialize(void){
  int ret=1;

  /* Register the file system object to the FatFs module */
  if(f_mount(&USERFatFS,(TCHAR const*)USERPath,0)==FR_OK){
    f_setlabel(DISK_NAME);
    /* Create and Open a new text file object with write access */
    if(f_open(&USERFile,CONFIG_FILENAME,(FA_CREATE_ALWAYS|FA_WRITE))==FR_OK){
      /* Write data to the text file */
      f_printf(&USERFile,"%s\n","MSCDFU_VERSION:v1.0");
      f_printf(&USERFile,"%s\n","MSCDFU_DATE:20240101");
      f_printf(&USERFile,"%s\n","CMD_ADDRESS:0x08010000");
      f_printf(&USERFile,"%s\n","APP_ADDRESS:0x08020000");
      f_printf(&USERFile,"%s\n","APP_MASK:0x2FFE0000");
      f_printf(&USERFile,"%s\n","APP_CHECK:0x20000000");
      f_printf(&USERFile,"%s\n","APP_FILENAME:FIRMWAR.BIN");
      /* Close the open text file */
      f_close(&USERFile);
    }
  }
  FATFS_UnLinkDriver(USERPath);
  return(ret);
}

int FS_Synchronize(void){
  TCHAR    read_string[11];
  uint32_t data_tmp;
  uint32_t data_mask;
  uint32_t data_check;
  int      ret=0;

  if(!FATFS_LinkDriver(&USER_Driver,USERPath)){
    if(f_mount(&USERFatFS,(TCHAR const*)USERPath,0)==FR_OK){

      if(f_open(&USERFile,CONFIG_FILENAME,(FA_READ))==FR_OK){
	data_mask=getParameterInt(&USERFile,"APP_MASK",8);
	data_check=getParameterInt(&USERFile,"APP_CHECK",9);
	getParameterStr(&USERFile,APP_FILENAME,"APP_FILENAME",12);
      }

      if(f_open(&USERFile,APP_FILENAME,(FA_READ))==FR_OK){
	UINT byteread;
	f_read(&USERFile,&data_tmp,sizeof(data_tmp),&byteread);
	if((data_tmp&data_mask)==data_check){
	  ret=1;
	}
	f_close(&USERFile);
      }
    }
  }
  FATFS_UnLinkDriver(USERPath);
  return(ret);
}

int FS_FirmwareUpgrade(void){
  int     ret=0;
  UINT    byteread;
  uint8_t buffer[4];

  uint32_t DFU_CMD_ADDRESS=0x08010000;
  uint32_t APP_ADDRESS=0x08020000;

  if(!FATFS_LinkDriver(&USER_Driver,USERPath)){
    if(f_mount(&USERFatFS,(TCHAR const*)USERPath,0)==FR_OK){

      f_open(&USERFile,APP_FILENAME,(FA_READ));
      HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_SET);

      HAL_FLASH_Unlock();
      FLASH_Erase_Sector(5,FLASH_VOLTAGE_RANGE_3);
      FLASH_Erase_Sector(6,FLASH_VOLTAGE_RANGE_3);
      FLASH_Erase_Sector(7,FLASH_VOLTAGE_RANGE_3);

      for(uint32_t i=0;i<(DISK_SIZE/4);i++){
	f_read(&USERFile,buffer,sizeof(buffer),&byteread);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(APP_ADDRESS+i*4),*(uint32_t*)&buffer);
      }
      memset(buffer,0x00,4);
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,DFU_CMD_ADDRESS,*(uint32_t*)&buffer);
      HAL_FLASH_Lock();

      HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_RESET);
      f_close(&USERFile);
      ret=1;
    }
  }
  FATFS_UnLinkDriver(USERPath);
  return(ret);
}

void MX_FATFS_Process(void){
  uint8_t workBuffer[_MAX_SS];

  /* Mass Storage Application State Machine */
  switch(file_operation_state){
    case FS_APPLICATION_INIT:
      if(f_mkfs(USERPath,FM_ANY,0,workBuffer,sizeof(workBuffer))==FR_OK){
	if(FS_Initialize()){
	  app_last_time=HAL_GetTick();
	  file_operation_state=FS_APPLICATION_RUNNING;
	}
      }
    break;

    case FS_APPLICATION_RUNNING:
      if((HAL_GetTick()-app_last_time)>1000){
	app_last_time=HAL_GetTick();
	if(FS_Synchronize()){
	  file_operation_state=FS_APPLICATION_UPGRADE;
	}
      }
    break;

    case FS_APPLICATION_UPGRADE:
      if(FS_FirmwareUpgrade()){
	file_operation_state=FS_APPLICATION_IDLE;
      }
    break;

    case FS_APPLICATION_IDLE:
      NVIC_SystemReset();
    break;

    default:
    break;
  }
}


/* USER CODE END Application */
