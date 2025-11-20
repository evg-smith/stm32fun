/**
  ******************************************************************************
  * @file    ymodem.c
  * @author  eevgsmith
  * @version 1.0.0
  * @date    11-20-2025
  * @brief   YModem protocol implementation with FatFS storage backend
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "common.h"
#include "ymodem.h"
#include "fatfs.h"

/* Private variables ---------------------------------------------------------*/
static FATFS fs;
static FIL fil;
static uint8_t aFileName[FILE_NAME_LENGTH];
static uint8_t aPacketData[PACKET_1K_SIZE + PACKET_DATA_INDEX + PACKET_TRAILER_SIZE];

extern UART_HandleTypeDef huart1;

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef ReceivePacket(uint8_t *p_data, uint32_t *p_length, uint32_t timeout);
static uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte);
static uint16_t Cal_CRC16(const uint8_t* p_data, uint32_t size);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Update CRC16 for input byte
  * @param  crc_in: Current CRC value
  * @param  byte: Input byte
  * @retval Updated CRC16 value
  */
static uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
{
  uint32_t crc = crc_in;
  uint32_t in = byte | 0x100;

  do
  {
    crc <<= 1;
    in <<= 1;
    if (in & 0x100)
      ++crc;
    if (crc & 0x10000)
      crc ^= 0x1021;
  }
  while (!(in & 0x10000));

  return crc & 0xFFFF;
}

/**
  * @brief  Calculate CRC16 for YModem packet
  * @param  p_data: Pointer to data buffer
  * @param  size: Size of data in bytes
  * @retval CRC16 value
  */
static uint16_t Cal_CRC16(const uint8_t* p_data, uint32_t size)
{
  uint32_t crc = 0;
  const uint8_t* dataEnd = p_data + size;

  while (p_data < dataEnd)
    crc = UpdateCRC16(crc, *p_data++);

  crc = UpdateCRC16(crc, 0);
  crc = UpdateCRC16(crc, 0);

  return crc & 0xFFFF;
}

/**
  * @brief  Receive a packet from sender
  * @param  p_data: Buffer to store received packet
  * @param  p_length: Pointer to store packet length
  *         0: End of transmission
  *         2: Abort by sender
  *         >0: Packet data length
  * @param  timeout: Reception timeout in milliseconds
  * @retval HAL_OK: Packet received successfully
  *         HAL_BUSY: Transfer aborted by user
  *         HAL_ERROR: Reception error
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

    if (packet_size >= PACKET_SIZE)
    {
      status = HAL_UART_Receive(&huart1, &p_data[PACKET_NUMBER_INDEX],
                                packet_size + PACKET_OVERHEAD_SIZE, timeout);

      if (status == HAL_OK)
      {
        /* Verify packet number integrity */
        if (p_data[PACKET_NUMBER_INDEX] != ((p_data[PACKET_CNUMBER_INDEX]) ^ NEGATIVE_BYTE))
        {
          packet_size = 0;
          status = HAL_ERROR;
        }
        else
        {
          /* Verify packet CRC */
          crc = p_data[packet_size + PACKET_DATA_INDEX] << 8;
          crc += p_data[packet_size + PACKET_DATA_INDEX + 1];
          if (Cal_CRC16(&p_data[PACKET_DATA_INDEX], packet_size) != crc)
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

/* Public functions ----------------------------------------------------------*/

/**
  * @brief  Receive a file using YModem protocol with CRC16
  * @param  p_size: Pointer to store received file size
  * @retval COM_OK: File received successfully
  *         COM_ABORT: Transfer aborted
  *         COM_ERROR: Communication error
  *         COM_DATA: File system or write error
  */
COM_StatusTypeDef Ymodem_Receive(uint32_t *p_size)
{
  uint32_t i, packet_length, session_done = 0, file_done, errors = 0, session_begin = 0;
  uint32_t ramsource, filesize, total_bytes_written = 0;
  uint8_t *file_ptr;
  uint8_t file_size[FILE_SIZE_LENGTH], packets_received;
  COM_StatusTypeDef result = COM_OK;
  uint8_t file_opened = 0;
  FRESULT fres;
  UINT bytes_written;

  while ((session_done == 0) && (result == COM_OK))
  {
    packets_received = 0;
    file_done = 0;
    total_bytes_written = 0;

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
                f_sync(&fil);
                f_close(&fil);
                file_opened = 0;
              }
              Serial_PutByte(ACK);
              file_done = 1;
              break;

            default:
              /* Data packet */
              if (aPacketData[PACKET_NUMBER_INDEX] != packets_received)
              {
                Serial_PutByte(NAK);
              }
              else
              {
                if (packets_received == 0 && !file_opened)
                {
                  /* Header packet - extract filename and size */
                  if (aPacketData[PACKET_DATA_INDEX] != 0)
                  {
                    /* Extract filename */
                    i = 0;
                    file_ptr = aPacketData + PACKET_DATA_INDEX;
                    while ((*file_ptr != 0) && (i < FILE_NAME_LENGTH))
                    {
                      aFileName[i++] = *file_ptr++;
                    }
                    aFileName[i++] = '\0';

                    /* Extract file size */
                    i = 0;
                    file_ptr++;
                    while ((*file_ptr != ' ') && (i < FILE_SIZE_LENGTH))
                    {
                      file_size[i++] = *file_ptr++;
                    }
                    file_size[i++] = '\0';
                    Str2Int(file_size, &filesize);
                    *p_size = filesize;

                    /* Mount filesystem */
                    fres = f_mount(&fs, "", 0);
                    if (fres != FR_OK)
                    {
                      Serial_PutByte(CA);
                      Serial_PutByte(CA);
                      result = COM_DATA;
                      break;
                    }

                    /* Create and open file */
                    fres = f_open(&fil, (char*)aFileName, FA_CREATE_ALWAYS | FA_WRITE);
                    if (fres != FR_OK)
                    {
                      f_mount(NULL, "", 1);
                      Serial_PutByte(CA);
                      Serial_PutByte(CA);
                      result = COM_DATA;
                      break;
                    }

                    file_opened = 1;
                    Serial_PutByte(ACK);
                    Serial_PutByte(CRC16);
                  }
                  else
                  {
                    /* Empty header - end session */
                    Serial_PutByte(ACK);
                    file_done = 1;
                    session_done = 1;
                  }
                }
                else
                {
                  /* Data packet - write to file */
                  if (!file_opened)
                  {
                    Serial_PutByte(CA);
                    Serial_PutByte(CA);
                    result = COM_DATA;
                    break;
                  }

                  ramsource = (uint32_t)&aPacketData[PACKET_DATA_INDEX];

                  /* Calculate actual bytes to write (exclude padding) */
                  uint32_t bytes_to_write = packet_length;
                  uint32_t remaining = filesize - total_bytes_written;
                  if (bytes_to_write > remaining)
                  {
                    bytes_to_write = remaining;
                  }

                  /* Write data to SD card */
                  fres = f_write(&fil, (void*)ramsource, bytes_to_write, &bytes_written);
                  if ((fres == FR_OK) && (bytes_written == bytes_to_write))
                  {
                    total_bytes_written += bytes_written;
                    Serial_PutByte(ACK);
                  }
                  else
                  {
                    /* Write error - abort */
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

        case HAL_BUSY:
          /* User abort */
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
          /* Reception error */
          if (session_begin > 0)
          {
            errors++;
          }
          if (errors > MAX_ERRORS)
          {
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
            Serial_PutByte(CRC16);
          }
          break;
      }
    }
  }

  /* Ensure file is closed on exit */
  if (file_opened)
  {
    f_sync(&fil);
    f_close(&fil);
  }

  return result;
}

/****************************** END OF FILE ***********************************/