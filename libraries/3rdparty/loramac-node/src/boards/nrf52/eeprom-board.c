/*!
 * \file      eeprom-board.c
 *
 * \brief     Target board EEPROM driver implementation
 *
 * \copyright Revised BSD License, see section \ref LICENSE.
 *
 * \code
 *                ______                              _
 *               / _____)             _              | |
 *              ( (____  _____ ____ _| |_ _____  ____| |__
 *               \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 *               _____) ) ____| | | || |_| ____( (___| | | |
 *              (______/|_____)_|_|_| \__)_____)\____)_| |_|
 *              (C)2013-2017 Semtech
 *
 * \endcode
 *
 * \author    Miguel Luis ( Semtech )
 *
 * \author    Gregory Cristian ( Semtech )
 */
#include "FreeRTOS.h"
#include "utilities.h"
#include "eeprom-board.h"
#include "task.h"


#include "nrf.h"
#include "nordic_common.h"
#ifdef SOFTDEVICE_PRESENT
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#endif

#include "fds.h"

#define CONFIG_REC_KEY  (0x7010)
#define LORA_FLASH_ID_OFFSET 0x0e0

/* Flag to check fds initialization. */
static bool volatile m_fds_initialized = false;
static bool volatile m_write_complete  = true;
static bool volatile m_update_complete = true;
static bool volatile m_gc_complete     = true;

/* Array to map FDS return values to strings. */
char const * fds_err_str[] =
{
    "FDS_SUCCESS",
    "FDS_ERR_OPERATION_TIMEOUT",
    "FDS_ERR_NOT_INITIALIZED",
    "FDS_ERR_UNALIGNED_ADDR",
    "FDS_ERR_INVALID_ARG",
    "FDS_ERR_NULL_ARG",
    "FDS_ERR_NO_OPEN_RECORDS",
    "FDS_ERR_NO_SPACE_IN_FLASH",
    "FDS_ERR_NO_SPACE_IN_QUEUES",
    "FDS_ERR_RECORD_TOO_LARGE",
    "FDS_ERR_NOT_FOUND",
    "FDS_ERR_NO_PAGES",
    "FDS_ERR_USER_LIMIT_REACHED",
    "FDS_ERR_CRC_CHECK_FAILED",
    "FDS_ERR_BUSY",
    "FDS_ERR_INTERNAL",
};

/* Array to map FDS events to strings. */
static char const * fds_evt_str[] =
{
    "FDS_EVT_INIT",
    "FDS_EVT_WRITE",
    "FDS_EVT_UPDATE",
    "FDS_EVT_DEL_RECORD",
    "FDS_EVT_DEL_FILE",
    "FDS_EVT_GC",
};

/* Keep track of the progress of a delete_all operation. */
static struct
{
    bool delete_next;   //!< Delete next record.
    bool pending;       //!< Waiting for an fds FDS_EVT_DEL_RECORD event, to delete the next record.
} m_delete_all;


static void fds_evt_handler(fds_evt_t const * p_evt)
{
    //printf("Event: %s received (%s)",
    //              fds_evt_str[p_evt->id],
    //              fds_err_str[p_evt->result]);

    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == FDS_SUCCESS)
            {
                m_fds_initialized = true;
            }
            break;

        case FDS_EVT_WRITE:
            if (p_evt->result == FDS_SUCCESS)
            {
                //printf("Record ID:\t0x%04x\n",  p_evt->write.record_id);
                //printf("File ID:\t0x%04x\n",    p_evt->write.file_id);
                //printf("Record key:\t0x%04x\n", p_evt->write.record_key);

                CRITICAL_REGION_ENTER();
                m_write_complete = true;
                CRITICAL_REGION_EXIT();
            }
            break;
        case FDS_EVT_UPDATE:
            if (p_evt->result == FDS_SUCCESS)
            {
                //printf("Record ID:\t0x%04x\n",  p_evt->write.record_id);
                //printf("File ID:\t0x%04x\n",    p_evt->write.file_id);
                //printf("Record key:\t0x%04x\n", p_evt->write.record_key);

                CRITICAL_REGION_ENTER();
                m_update_complete = true;
                CRITICAL_REGION_EXIT();
            }
            break;

        case FDS_EVT_DEL_RECORD:
            if (p_evt->result == FDS_SUCCESS)
            {
                //printf("Record ID:\t0x%04x\n",  p_evt->del.record_id);
                //printf("File ID:\t0x%04x\n",    p_evt->del.file_id);
                //printf("Record key:\t0x%04x\n", p_evt->del.record_key);
            }
            m_delete_all.pending = false;
            break;

        case FDS_EVT_GC:
            if (p_evt->result == FDS_SUCCESS)
            {
                //printf("Flash Garbage collection complete\n");
                FlashPrintDiagnostics();

                CRITICAL_REGION_ENTER();
                m_gc_complete = true;
                CRITICAL_REGION_EXIT();

            }
            break;

        default:
            break;
    }
}



/**@brief   Wait for fds to initialize. */
static void wait_for_fds_ready(void)
{
    while (!m_fds_initialized);
}

/**@brief   Process a delete all command.
 *
 * Delete records, one by one, until no records are left.
 */
void delete_all_process(void)
{
    if (   m_delete_all.delete_next
        & !m_delete_all.pending)
    {
        printf("Deleting next record.");

        m_delete_all.delete_next = record_delete_next();
        if (!m_delete_all.delete_next)
        {
            printf("No records left to delete.");
        }
    }
}

void FlashPrintDiagnostics()
{

    fds_stat_t stat = {0};
    configASSERT(NRF_SUCCESS == fds_stat(&stat));
/*
    printf("\n\nFlash Diagnostics:\n");
    printf("%-6d  4K Pages Available\n", stat.pages_available);
    printf("%-6d  Open Records\n", stat.open_records);
    printf("%-6d  Valid Records\n", stat.valid_records);
    printf("%-6d  Dirty Records\n", stat.dirty_records);
    printf("%-6d  Words Reserved\n", stat.words_reserved);
    printf("%-6d  Words Used\n", stat.words_used);
    printf("%-6d  Largest Contiguous\n", stat.words_used);
    printf("%-6d  Freeable Words\n", stat.freeable_words);
    printf("%-6d  Atleast One Corruption\n\n", stat.corruption);
    */

}

void EepromMcuInit()
{
    /* Register first to receive an event when initialization is complete. */
    fds_register(fds_evt_handler);
    configASSERT(NRF_SUCCESS == fds_init());
    
    /* Wait for fds to initialize. */
    wait_for_fds_ready();
    FlashPrintDiagnostics();
}

// Blocking version of the garbage collector
ret_code_t FlashGarbageCollectSync()
{
    CRITICAL_REGION_ENTER();
    m_gc_complete = false;
    CRITICAL_REGION_EXIT();

    // Start gc then block until it's finished
    ret_code_t rc = fds_gc();
    configASSERT(rc == NRF_SUCCESS);
    while(!m_gc_complete);

    return rc;
}

ret_code_t FlashWriteSync(fds_record_desc_t *desc, fds_record_t *new_record)
{
    ret_code_t rc = FDS_ERR_INTERNAL;


    CRITICAL_REGION_ENTER();
    m_write_complete = false;
    CRITICAL_REGION_EXIT();

    if (FDS_ERR_NO_SPACE_IN_FLASH == fds_record_write(desc, new_record))
    {
         // Garbage collec and try one last time
         FlashGarbageCollectSync();
         rc = fds_record_write(desc, new_record);
         configASSERT(rc == FDS_SUCCESS);
    }

    // Wait for write to complete
    while(!m_write_complete);

    return rc;
}

ret_code_t FlashUpdateSync(fds_record_desc_t *desc, fds_record_t *new_record)
{
    ret_code_t rc = FDS_ERR_INTERNAL;

    CRITICAL_REGION_ENTER();
    m_update_complete = false;
    CRITICAL_REGION_EXIT();

    if (FDS_ERR_NO_SPACE_IN_FLASH == fds_record_update(desc, new_record))
    {
        // Garbage collect, then try again
        FlashGarbageCollectSync();
        rc = fds_record_update(desc, new_record);
        configASSERT(rc == FDS_SUCCESS);
    }

    while(!m_update_complete);

    return rc;
}


// For nrf52, write address must be 32-bit, and should write words under the hood. Taken care of by nrf_nvmc API
uint8_t EepromMcuWriteBuffer( uint16_t addr, uint8_t *buffer, uint16_t size )
{
    configASSERT( buffer != NULL );
    addr += LORA_FLASH_ID_OFFSET;

    // Configure a record to interface with
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};
    
    /* A record containing dummy configuration data. */
    size_t   padded_len = 4 * ((size + 3) / sizeof(uint32_t)); 
    uint8_t  padded_buffer[padded_len];
    memcpy(padded_buffer, buffer, size);

    fds_record_t new_record =
    {
        .file_id           = addr,
        .key               = CONFIG_REC_KEY,
        .data.p_data       = padded_buffer,
        /* The length of a record is always expressed in 4-byte units (words). */
        .data.length_words = padded_len / 4,
    };

    // Overwrite or create the new record data
    if (FDS_SUCCESS == fds_record_find(addr, CONFIG_REC_KEY, &desc, &tok))
    {
        FlashUpdateSync(&desc, &new_record);
    }
    else
    {
        FlashWriteSync(&desc, &new_record);
    }

    return SUCCESS;

}

// Readihg can be byte-addressable
uint8_t EepromMcuReadBuffer( uint16_t addr, uint8_t *buffer, uint16_t size )
{
    configASSERT( buffer != NULL );
    addr += LORA_FLASH_ID_OFFSET;

    // Setup a record query
    fds_record_desc_t desc = {0};
    fds_find_token_t  tok  = {0};

    if (FDS_SUCCESS == fds_record_find(addr, CONFIG_REC_KEY, &desc, &tok))
    {
        // Record exists. Return it
        fds_flash_record_t flash_record = {0};
        configASSERT(NRF_SUCCESS == fds_record_open(&desc, &flash_record));
        memcpy(buffer, (uint8_t *)flash_record.p_data, size);
        configASSERT(NRF_SUCCESS == fds_record_close(&desc));
    }
    else
    {
        // No record found, create one. Set return data to indicate this
        memset(buffer, 0xFF, size);
    }

    return SUCCESS;
}


void EepromMcuSetDeviceAddr( uint8_t addr )
{
    configASSERT( FAIL );
}

uint8_t EepromMcuGetDeviceAddr( void )
{
    configASSERT( FAIL );
    return 0;
}
