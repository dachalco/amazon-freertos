#include "flash_map_backend/flash_map_backend.h"
#include "nrf_nvmc.h"
#include "bootloader.h"
#include "sysflash/sysflash.h"


#include "boards.h"
#include "crypto.h"
#include "nrf_mbr.h"
#include "nrf_nvmc.h"
#include "nrf_sdm.h"
#include "nrf_bootloader_info.h"
#include "nrf_bootloader.h"


static struct flash_area xFlashArea[4] = 
{
    {
        .fa_id = FLASH_AREA_BOOTLOADER,
        .fa_device_id = 0,
        .fa_off = BOOTLOADER_START,
        .fa_size = 0x100000 - BOOTLOADER_START
    },
    {
        .fa_id = FLASH_AREA_IMAGE_PRIMARY(0),
        .fa_device_id = 0,
        .fa_off = 0x26fe0,
        .fa_size = 0x24000
    },
    {
        .fa_id = FLASH_AREA_IMAGE_SECONDARY(0),
        .fa_device_id = 0,
        //.fa_off = CODE_REGION_2_START,
        //.fa_size = 0xF0000 - CODE_REGION_2_START
        .fa_off = 0x50000,
        .fa_size = 0x24000
    },
    {
        .fa_id = FLASH_AREA_IMAGE_SCRATCH,
        .fa_device_id = 0,
        .fa_off = SWAP_AREA_BEGIN,
        .fa_size = SWAP_AREA_SIZE * 0x1000
    }
};

/*< Opens the area for use. id is one of the `fa_id`s */
int flash_area_open(uint8_t id, const struct flash_area ** ppfa)
{
    int xRC = 0;
    if( id >= 0 && id < sizeof(xFlashArea)/sizeof(xFlashArea[0]))
    {
        *ppfa = &xFlashArea[id];
    }
    else
    {
        xRC = -1;
    }

    return xRC;
}


void flash_area_close(const struct flash_area * pfa)
{
    (void)pfa;
}


/*< Reads `len` bytes of flash memory at `off` to the buffer at `dst` */
int flash_area_read(const struct flash_area * pfa,
                    uint32_t off, 
                    void *dst, 
                    uint32_t len)
{
    int xRC = 0;

    uint32_t ulAddress = pfa->fa_off + off;
    memcpy(dst, ulAddress, len);

    return xRC;
}

/*< Writes `len` bytes of flash memory at `off` from the buffer at `src` */
int flash_area_write(const struct flash_area * pfa, 
                     uint32_t off, 
                     const void *src, 
                     uint32_t len)
{
    int xRC = 0;
    uint32_t ulAddress = pfa->fa_off + off;

    nrf_nvmc_write_bytes( ulAddress, src, len );

    return xRC;
}

/*< Erases `len` bytes of flash memory at `off` */
int flash_area_erase(const struct flash_area * pfa, 
                     uint32_t off, 
                     uint32_t len)
{
    int xRC = 0;

    if (len == 0)
    {
        return 0;
    }

    uint32_t ulAddress = pfa->fa_off + off;
    uint32_t ulPageAddress = ulAddress - (ulAddress % CODE_PAGE_SIZE);
    uint32_t ulNPages = len / CODE_PAGE_SIZE;
    if( len % CODE_PAGE_SIZE )
    {
        ulNPages += 1;
    }

    
    for(int i=0; i<ulNPages; i++)
    {
        nrf_nvmc_page_erase(ulPageAddress);
        ulPageAddress += CODE_PAGE_SIZE;
    }

    return xRC;
}

/*< Returns this `flash_area`s alignment */
uint8_t flash_area_align(const struct flash_area * pfa)
{
    return 4u;
}

/*< What is value is read from erased flash bytes. */
uint8_t flash_area_erased_val(const struct flash_area * pfa)
{
    return 0xFF;
}

#ifdef MCUBOOT_USE_FLASH_AREA_GET_SECTORS
/*< Given flash area ID, return info about sectors within the area. */
int flash_area_get_sectors(int fa_id, 
                           uint32_t *count, 
                           struct flash_sector *sectors)
{
    sectors->fs_size = CODE_PAGE_SIZE;
    sectors->fs_off = 0;

    struct flash_area * pfa = xFlashAreas[fa_id];

    *count = pfa->fa_size / sectors->fs_size;
}
#endif

/*< Returns the `fa_id` for slot, where slot is 0 (primary) or 1 (secondary).
    `image_index` (0 or 1) is the index of the image. Image index is
    relevant only when multi-image support support is enabled */
int flash_area_id_from_multi_image_slot(int image_index, 
                                        int slot)
{
    switch (slot) {
    case 0: return FLASH_AREA_IMAGE_PRIMARY(image_index);
    case 1: return FLASH_AREA_IMAGE_SECONDARY(image_index);
    case 2: return FLASH_AREA_IMAGE_SCRATCH;
    }

    return -1; /* flash_area_open will fail on that */
}

/*< Returns the slot (0 for primary or 1 for secondary), for the supplied
    `image_index` and `area_id`. `area_id` is unique and is represented by
    `fa_id` in the `flash_area` struct. */
int flash_area_id_to_multi_image_slot(int image_index, 
                                      int area_id)
{
    if (area_id == FLASH_AREA_IMAGE_PRIMARY(image_index)) {
        return 1;
    }
    if (area_id == FLASH_AREA_IMAGE_SECONDARY(image_index)) {
        return 2;
    }

    return -1;
}

int flash_area_to_sectors(int idx,
                          int *cnt, 
                          struct flash_area *pfa)
{
    // nop
    return 0;
}
