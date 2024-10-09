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
TCHAR IDENTIFY_NAME[8]={'U','I','D','.','T','X','T'};
TCHAR APP_NAME[11];
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
    if(f_open(&USERFile,FILENAME_CONFIG,(FA_CREATE_ALWAYS|FA_WRITE))==FR_OK){
      /* Write data to the text file */
      f_printf(&USERFile,"MSCDFU_VERSION:v%d.%d\n",MSCDFU_VERSION,0);
      f_printf(&USERFile,"MSCDFU_DATE:%08d\n",MSCDFU_DATE);
      f_printf(&USERFile,"ADDRESS_CMD:%08X\n",ADDRESS_CMD);
      f_printf(&USERFile,"ADDRESS_APP:%08X\n",ADDRESS_APP);
      f_printf(&USERFile,"APP_MASK:%08X\n","0x2FFE0000");
      f_printf(&USERFile,"APP_CHECK:%08X\n","0x20000000");
      f_printf(&USERFile,"DEVICE_UID:%08X%08X%08X\n",(*(uint32_t*)(UID_BASE  )),(*(uint32_t*)(UID_BASE+4)),(*(uint32_t*)(UID_BASE+8)));
      f_printf(&USERFile,"APP_FILENAME:%s\n",FILENAME_APP);
      /* Close the open text file */
      f_close(&USERFile);
      ret=0;
    }
  }
  FATFS_UnLinkDriver(USERPath);
  return(ret);
}

int FS_Synchronize(void){
  FILINFO  fno;
  TCHAR    read_string[11];
  uint32_t data_tmp;
  uint32_t data_mask;
  uint32_t data_check;
  int      ret=1;

  if(!FATFS_LinkDriver(&USER_Driver,USERPath)){
    if(f_mount(&USERFatFS,(TCHAR const*)USERPath,0)==FR_OK){

      if(f_stat(FILENAME_CONFIG,&fno)==FR_OK){
        if(f_open(&USERFile,FILENAME_CONFIG,(FA_READ))==FR_OK){
		  data_mask=getParameterInt(&USERFile,"APP_MASK",8);
		  data_check=getParameterInt(&USERFile,"APP_CHECK",9);
		  getParameterStr(&USERFile,APP_NAME,"APP_FILENAME",12);
        }
      }

      if(f_stat(APP_NAME,&fno)==FR_OK){
    	if(f_open(&USERFile,APP_NAME,(FA_READ))==FR_OK){
		  UINT byteread;
		  f_read(&USERFile,&data_tmp,sizeof(data_tmp),&byteread);
		  if((data_tmp&data_mask)==data_check){
		    ret=0;
		  }
		  f_close(&USERFile);
        }
      }

      if(f_stat(IDENTIFY_NAME,&fno)==FR_OK){
    	if(f_open(&USERFile,IDENTIFY_NAME,(FA_READ))==FR_OK){
    	  uint32_t buffer[8];
    	  char str_tmp[100];

    	  f_lseek(&USERFile,0);
    	  while(!f_eof(&USERFile)){
    		f_gets((TCHAR*)str_tmp,sizeof(str_tmp),&USERFile);
    		if(!strncmp(str_tmp,"UID",3)){
			  strncpy(read_string,&str_tmp[4],8);
			  buffer[0]=~strtoul(read_string,NULL,16);
    		  strncpy(read_string,&str_tmp[12],8);
    		  buffer[1]=~strtoul(read_string,NULL,16);
    		  strncpy(read_string,&str_tmp[20],8);
    		  buffer[2]=~strtoul(read_string,NULL,16);
    		  buffer[3]=0x00C0FFEE;
    		}
    	  }
    	  f_close(&USERFile);
    	  // write identify code
    	  HAL_FLASH_Unlock();
    	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,ADDRESS_IDENTIFY   ,buffer[0]);
    	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,ADDRESS_IDENTIFY+4 ,buffer[1]);
    	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,ADDRESS_IDENTIFY+8 ,buffer[2]);
    	  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,ADDRESS_IDENTIFY+12,buffer[3]);
    	  HAL_FLASH_Lock();
    	}
    	// delete FILENAME_IDENTIFY to avoid flash write infinite loop
    	f_unlink(IDENTIFY_NAME);
    	NVIC_SystemReset();
      }

    }
  }
  FATFS_UnLinkDriver(USERPath);
  return(ret);
}

int FS_FirmwareUpgrade(void){
  int     ret=1;
  UINT    byteread;
  uint8_t buffer[4];

  if(!FATFS_LinkDriver(&USER_Driver,USERPath)){
    if(f_mount(&USERFatFS,(TCHAR const*)USERPath,0)==FR_OK){

      f_open(&USERFile,APP_NAME,(FA_READ));

      HAL_FLASH_Unlock();
      FLASH_Erase_Sector(4,FLASH_VOLTAGE_RANGE_3);
      FLASH_Erase_Sector(5,FLASH_VOLTAGE_RANGE_3);
      FLASH_Erase_Sector(6,FLASH_VOLTAGE_RANGE_3);
      FLASH_Erase_Sector(7,FLASH_VOLTAGE_RANGE_3);

      for(uint32_t i=0;i<(DISK_SIZE/4);i++){
				f_read(&USERFile,&buffer[0],sizeof(buffer),&byteread);
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,(ADDRESS_APP+i*4),*(uint32_t*)&buffer);
      }
      memset(buffer,0x00,4);
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,ADDRESS_CMD,*(uint32_t*)&buffer);
      HAL_FLASH_Lock();

      f_close(&USERFile);
      ret=0;
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
		if(!FS_Initialize()){
		  app_last_time=HAL_GetTick();
		  file_operation_state=FS_APPLICATION_RUNNING;
		}
      }
    break;

    case FS_APPLICATION_RUNNING:
      if((HAL_GetTick()-app_last_time)>1000){
		app_last_time=HAL_GetTick();
		if(!FS_Synchronize()){
		  file_operation_state=FS_APPLICATION_UPGRADE;
		}
      }
    break;

    case FS_APPLICATION_UPGRADE:
      if(!FS_FirmwareUpgrade()){
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
