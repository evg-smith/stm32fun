/**
  ******************************************************************************
  * @file    IAP_Main/Src/ymodem.c 
  * @author  MCD Application Team
  * @version 1.0.0
  * @date    8-April-2015
  * @brief   This file provides all the software functions related to the ymodem 
  *          protocol.
  ******************************************************************************
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/** @addtogroup STM32L4xx_IAP
  * @{
  */ 
  
/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "ymodem.h"
#include "fatfs.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SYNC_INTERVAL 10  /* Sync to SD card every 10 packets */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

// FatFS variables
FATFS fs;
FIL fil;
FRESULT fres;
UINT bytes_written;

uint8_t aFileName[FILE_NAME_LENGTH];
/* @note ATTENTION - please keep this variable 32bit alligned */
uint8_t aPacketData[PACKET_1K_SIZE + PACKET_DATA_INDEX + PACKET_TRAILER_SIZE];

extern UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef ReceivePacket(uint8_t *p_data, uint32_t *p_length, uint32_t timeout);
uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte);
uint16_t Cal_CRC16(const uint8_t* p_data, uint32_t size);
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Receive a packet from sender
  * @param  data
  * @param  length
  *     0: end of transmission
  *     2: abort by sender
  *    >0: packet length
  * @param  timeout
  * @retval HAL_OK: normally return
  *         HAL_BUSY: abort by user
  */
static HAL_StatusTypeDef ReceivePacket(uint8_t *p_data, uint32_t *p_length, uint32_t timeout)
{
  uint32_t crc;
  uint32_t packet_size = 0;
  HAL_StatusTypeDef status;
  uint8_t char1;

  *p_length = 0;
  status = HAL_UART_Receive(&huart1, &char1, 1, timeout);

  if (status == HAL_OK)
  {
    switch (char1)
    {
      case SOH:
        packet_size = PACKET_SIZE;
        break;
      case STX:
        packet_size = PACKET_1K_SIZE;
        break;
      case EOT:
        break;
      case CA:
        if ((HAL_UART_Receive(&huart1, &char1, 1, timeout) == HAL_OK) && (char1 == CA))
        {
          packet_size = 2;
        }
        else
        {
          status = HAL_ERROR;
        }
        break;
      case ABORT1:
      case ABORT2:
        status = HAL_BUSY;
        break;
      default:
        status = HAL_ERROR;
        break;
    }
    *p_data = char1;

    if (packet_size >= PACKET_SIZE )
    {
      status = HAL_UART_Receive(&huart1, &p_data[PACKET_NUMBER_INDEX], packet_size + PACKET_OVERHEAD_SIZE, timeout);

      /* Simple packet sanity check */
      if (status == HAL_OK )
      {
        if (p_data[PACKET_NUMBER_INDEX] != ((p_data[PACKET_CNUMBER_INDEX]) ^ NEGATIVE_BYTE))
        {
          packet_size = 0;
          status = HAL_ERROR;
        }
        else
        {
          /* Check packet CRC */
          crc = p_data[ packet_size + PACKET_DATA_INDEX ] << 8;
          crc += p_data[ packet_size + PACKET_DATA_INDEX + 1 ];
          if (Cal_CRC16(&p_data[PACKET_DATA_INDEX], packet_size) != crc )
          {
            packet_size = 0;
            status = HAL_ERROR;
          }
        }
      }
      else
      {
        packet_size = 0;
      }
    }
  }
  *p_length = packet_size;
  return status;
}

/**
  * @brief  Update CRC16 for input byte
  * @param  crc_in input value
  * @param  input byte
  * @retval None
  */
uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
{
  uint32_t crc = crc_in;
  uint32_t in = byte | 0x100;

  do
  {
    crc <<= 1;
    in <<= 1;
    if(in & 0x100)
      ++crc;
    if(crc & 0x10000)
      crc ^= 0x1021;
  }

  while(!(in & 0x10000));

  return crc & 0xffffu;
}

/**
  * @brief  Cal CRC16 for YModem Packet
  * @param  data
  * @param  length
  * @retval None
  */
uint16_t Cal_CRC16(const uint8_t* p_data, uint32_t size)
{
  uint32_t crc = 0;
  const uint8_t* dataEnd = p_data+size;

  while(p_data < dataEnd)
    crc = UpdateCRC16(crc, *p_data++);

  crc = UpdateCRC16(crc, 0);
  crc = UpdateCRC16(crc, 0);

  return crc&0xffffu;
}

/* Public functions ---------------------------------------------------------*/
/**
  * @brief  Receive a file using the ymodem protocol with CRC16.
  * @param  p_size The size of the file.
  * @retval COM_StatusTypeDef result of reception/programming
  */
COM_StatusTypeDef Ymodem_Receive ( uint32_t *p_size )
{
  uint32_t i, packet_length, session_done = 0, file_done, errors = 0, session_begin = 0;
  uint32_t ramsource, filesize;
  uint8_t *file_ptr;
  uint8_t file_size[FILE_SIZE_LENGTH], packets_received;
  COM_StatusTypeDef result = COM_OK;
  uint8_t file_opened = 0;  // Track if file is currently open
  uint32_t packets_since_sync = 0;  // Counter for periodic sync

  while ((session_done == 0) && (result == COM_OK))
  {
    packets_received = 0;
    file_done = 0;
    while ((file_done == 0) && (result == COM_OK))
    {
      switch (ReceivePacket(aPacketData, &packet_length, DOWNLOAD_TIMEOUT))
      {
        case HAL_OK:
          errors = 0;
          switch (packet_length)
          {
            case 2:
              /* Abort by sender */
              if (file_opened)
              {
                f_close(&fil);
                file_opened = 0;
              }
              Serial_PutByte(ACK);
              result = COM_ABORT;
              break;
            case 0:
              /* End of transmission */
              if (file_opened)
              {
                // Final sync and close
                f_sync(&fil);
                f_close(&fil);
                file_opened = 0;
              }
              Serial_PutByte(ACK);
              file_done = 1;
              break;
            default:
              /* Normal packet */
              if (aPacketData[PACKET_NUMBER_INDEX] != packets_received)
              {
                Serial_PutByte(NAK);
              }
              else
              {
                if (packets_received == 0 && !file_opened)
                {
                  /* File name packet - only if file not already open */
                  if (aPacketData[PACKET_DATA_INDEX] != 0)
                  {
                    /* File name extraction */
                    i = 0;
                    file_ptr = aPacketData + PACKET_DATA_INDEX;
                    while ( (*file_ptr != 0) && (i < FILE_NAME_LENGTH))
                    {
                      aFileName[i++] = *file_ptr++;
                    }
                    aFileName[i++] = '\0';

                    /* File size extraction */
                    i = 0;
                    file_ptr++;
                    while ( (*file_ptr != ' ') && (i < FILE_SIZE_LENGTH))
                    {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    Str2Int(file_size, &filesize);
                    *p_size = filesize;

                    /* Mount filesystem */
                    if(f_mount(&fs, "", 0) != FR_OK)
                    {
                      /* End session */
                      Serial_PutByte(CA);
                      Serial_PutByte(CA);
                      result = COM_DATA;
                      break;
                    }

                    /* Open file for writing */
                    if(f_open(&fil, (char*)aFileName, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
                    {
                      /* End session */
                      f_mount(NULL, "", 1);
                      Serial_PutByte(CA);
                      Serial_PutByte(CA);
                      result = COM_DATA;
                      break;
                    }

                    file_opened = 1;
                    packets_since_sync = 0;

                    Serial_PutByte(ACK);
                    Serial_PutByte(CRC16);
                  }
                  /* File header packet is empty, end session */
                  else
                  {
                    Serial_PutByte(ACK);
                    file_done = 1;
                    session_done = 1;
                    break;
                  }
                }
                else /* Data packet */
                {
                  if (!file_opened)
                  {
                    /* File should be open at this point */
                    Serial_PutByte(CA);
                    Serial_PutByte(CA);
                    result = COM_DATA;
                    break;
                  }

                  ramsource = (uint32_t) & aPacketData[PACKET_DATA_INDEX];

                  /* Write received data to SD card */
                  if (f_write(&fil, (void*) ramsource, packet_length, &bytes_written) == FR_OK)
                  {
                    /* Verify all bytes were written */
                    if (bytes_written == packet_length)
                    {
                      packets_since_sync++;

                      /* Sync to SD card every SYNC_INTERVAL packets to ensure data is written
                       * This prevents buffer overflow and ensures data integrity
                       * for large files while maintaining reasonable performance */
                      if (packets_since_sync >= SYNC_INTERVAL)
                      {
                        if (f_sync(&fil) != FR_OK)
                        {
                          /* Sync failed - abort */
                          f_close(&fil);
                          file_opened = 0;
                          Serial_PutByte(CA);
                          Serial_PutByte(CA);
                          result = COM_DATA;
                          break;
                        }
                        packets_since_sync = 0;
                      }

                      Serial_PutByte(ACK);
                    }
                    else
                    {
                      /* Partial write - abort */
                      f_close(&fil);
                      file_opened = 0;
                      Serial_PutByte(CA);
                      Serial_PutByte(CA);
                      result = COM_DATA;
                    }
                  }
                  else /* An error occurred while writing to SD card */
                  {
                    /* End session */
                    f_close(&fil);
                    file_opened = 0;
                    Serial_PutByte(CA);
                    Serial_PutByte(CA);
                    result = COM_DATA;
                  }
                }
                packets_received++;
                session_begin = 1;
              }
              break;
          }
          break;
        case HAL_BUSY: /* Abort actually */
          if (file_opened)
          {
            f_close(&fil);
            file_opened = 0;
          }
          Serial_PutByte(CA);
          Serial_PutByte(CA);
          result = COM_ABORT;
          break;
        default:
          if (session_begin > 0)
          {
            errors++;
          }
          if (errors > MAX_ERRORS)
          {
            /* Abort communication */
            if (file_opened)
            {
              f_close(&fil);
              file_opened = 0;
            }
            Serial_PutByte(CA);
            Serial_PutByte(CA);
            result = COM_ERROR;
          }
          else
          {
            Serial_PutByte(CRC16); /* Ask for a packet */
          }
          break;
      }
    }
  }

  /* Ensure file is closed if still open (cleanup) */
  if (file_opened)
  {
    f_sync(&fil);
    f_close(&fil);
  }

  return result;
}

/**
  * @}
  */

/*******************(C)COPYRIGHT 2015 STMicroelectronics *****END OF FILE****/