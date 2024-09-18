/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "boot_common.h"
#include "fsl_glikey.h"
#include "fsl_romapi.h"


static flash_config_t s_flashDriver;
static uint32_t pflashBlockBase  = 0U;
static uint32_t pflashTotalSize  = 0U;
static uint32_t pflashSectorSize = 0U;
static uint32_t PflashPageSize   = 0U;


#define app_glikey_base GLIKEY0
#define EXAMPLE_CRITICAL_VALUE 0x42
#define GLIKEY_CODEWORD_STEP3_PROTECTED (GLIKEY_CODEWORD_STEP3 ^ EXAMPLE_CRITICAL_VALUE)

status_t GLIKEY_EnableIndex(uint32_t index)
{
    uint32_t example_check_value =
        EXAMPLE_CRITICAL_VALUE; /* Should depend on some calculation relevant to unlocked index*/

    status_t status = GLIKEY_IsLocked(app_glikey_base);
    if (kStatus_GLIKEY_NotLocked != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_SyncReset(app_glikey_base);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_StartEnable(app_glikey_base, index);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    /* Perform tests to assure enabling of the indexed SFR can continue */
    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP1);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP2);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP3_PROTECTED ^ example_check_value);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP4);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP5);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP6);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP7);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }

    status = GLIKEY_ContinueEnable(app_glikey_base, GLIKEY_CODEWORD_STEP_EN);
    if (kStatus_Success != status)
    {
        return kStatus_Fail;
    }
    return status;
}

void flash_init(void)
{
  /*
     * Enable the MBC register.
     * */
    GLIKEY_EnableIndex(15);

    /*
     * If the CMPA is not modified,
     * after POR, the default MBC rule for all flash regions is GLBAC[4],
     * the defaut value of GLBAC[4] is 0x00005500(R/X)
     * change the GLBAC[4] to 0x00007700(R/W/X),
     * to enable the read/write/execute permission for all flash regions.
     * After flash operation, please change the permission as needed.
     */
    MBC0->MBC0_MEMN_GLBAC[4] = 0x00007700;

    /*
     * End Glikey operation
     * */
    GLIKEY_EndOperation(app_glikey_base);
    
    
    memset(&s_flashDriver, 0, sizeof(flash_config_t));
    FLASH_API->flash_init(&s_flashDriver);
    
    /* Get flash properties kFLASH_ApiEraseKey */
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashBlockBaseAddr, &pflashBlockBase);
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashSectorSize, &pflashSectorSize);
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashTotalSize, &pflashTotalSize);
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashPageSize, &PflashPageSize);

//    BL_TRACE("\r\n PFlash Information:");
//    /* Print flash information - PFlash. */
//    BL_TRACE("\r\n kFLASH_PropertyPflashBlockBaseAddr = 0x%X", pflashBlockBase);
//    BL_TRACE("\r\n kFLASH_PropertyPflashSectorSize = %d", pflashSectorSize);
//    BL_TRACE("\r\n kFLASH_PropertyPflashTotalSize = %d", pflashTotalSize);
//    BL_TRACE("\r\n kFLASH_PropertyPflashPageSize = 0x%X", PflashPageSize);
    
}

int memory_erase(uint32_t start_addr, uint32_t len)
{
    int addr = start_addr;

    if((start_addr & 0x07FFFFFF) < (BL_SIZE))
    {
        return 1;
    }
    
    while(addr < (len + start_addr))
    {
        FLASH_API->flash_erase_sector(&s_flashDriver, addr, pflashSectorSize, kFLASH_ApiEraseKey);
        addr += pflashSectorSize;
    }
    return 0;
}

int memory_write(uint32_t start_addr, uint8_t *buf, uint32_t len)
{
    if((start_addr & 0x07FFFFFF) < (BL_SIZE))
    {
        return 1;
    }
    
    FLASH_API->flash_program_page(&s_flashDriver, start_addr, (uint8_t *)buf, len);
    return 0;
}


int memory_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    FLASH_API->flash_read(&s_flashDriver, addr, buf, len);
    return 0;
}

int memory_get_sector_size(void)
{
    return pflashSectorSize;
}

/*
ret:
  0: SUCC or BACKDOOR found.
 -1: invalid image
*/
static int scan_valid_boot_addr(uint32_t search_start, uint32_t *boot_addr)
{
    static uint32_t sp, pc;
    
    *boot_addr = DEFAULT_APPLICATION_START;
    
    uint32_t *vectorTable = (uint32_t*)(*boot_addr);

    if(vectorTable[0] == 0xFFFFFFFF) return 1;
    
    return 0;
}


void jump_to_image(uint32_t addr)
{
    static uint32_t sp, pc;
    uint32_t *vectorTable = (uint32_t*)addr;
    sp = vectorTable[0];
    pc = vectorTable[1];
    
    typedef void(*app_entry_t)(void);

    /* must be static, otherwise SP value will be lost */
    static app_entry_t s_application = 0;

    s_application = (app_entry_t)pc;

    // Change MSP and PSP
    __set_MSP(sp);
    __set_PSP(sp);
    
    SCB->VTOR = addr;
    
    // Jump to application
    s_application();

    // Should never reach here.
    __NOP();
}



void jump_to_app(void)
{
    uint32_t boot_addr = 0;
    int ret;
    ret = scan_valid_boot_addr(DEFAULT_APPLICATION_START, &boot_addr);

    //BL_TRACE("scan_valid_boot_addr:%d\r\n", ret);
    if(ret == 0)
    {
        /* clean up resouces */
        BL_TRACE("jump to 0x%08X\r\n", boot_addr);
        
        RESET_ReleasePeripheralReset(kLPUART0_RST_SHIFT_RSTn);
        RESET_ReleasePeripheralReset(kFLEXCAN0_RST_SHIFT_RSTn);
        NVIC_DisableIRQ(LPUART0_IRQn);
        NVIC_DisableIRQ(CAN0_IRQn);
        NVIC_ClearPendingIRQ(LPUART0_IRQn);
        NVIC_ClearPendingIRQ(CAN0_IRQn);
        
        jump_to_image(boot_addr);
    }
}


