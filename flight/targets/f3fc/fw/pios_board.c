/**
 ******************************************************************************
 * @addtogroup Targets Target Boards
 * @{
 * @addtogroup RCE F3FC
 * @{
 *
 * @file       f3fc/fw/pios_board.c
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2012-2014
 * @author     dRonin, http://dRonin.org/, Copyright (C) 2016
 * @brief      The board specific initialization routines
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 *
 * Additional note on redistribution: The copyright and license notices above
 * must be maintained in each individual source file that is a derivative work
 * of this source file; otherwise redistribution is prohibited.
 */

/* Pull in the board-specific static HW definitions.
 * Including .c files is a bit ugly but this allows all of
 * the HW definitions to be const and static to limit their
 * scope.
 *
 * NOTE: THIS IS THE ONLY PLACE THAT SHOULD EVER INCLUDE THIS FILE
 */

#include "board_hw_defs.c"

#include <pios.h>
#include <pios_hal.h>
#include <openpilot.h>
#include <uavobjectsinit.h>
#include "hwf3fc.h"
#include "manualcontrolsettings.h"
#include "modulesettings.h"

#if defined(PIOS_INCLUDE_DEBUG_CONSOLE)
#define PIOS_COM_DEBUGCONSOLE_TX_BUF_LEN 40
uintptr_t pios_com_debug_id;
#endif	/* PIOS_INCLUDE_DEBUG_CONSOLE */

uintptr_t pios_com_aux_id;
uintptr_t pios_uavo_settings_fs_id;
uintptr_t pios_com_openlog_logging_id;

/**
 * PIOS_Board_Init()
 * initializes all the core subsystems on this specific hardware
 * called from System/openpilot.c
 */

#include <pios_board_info.h>

void PIOS_Board_Init(void)
{
	const struct pios_board_info * bdinfo = &pios_board_info_blob;

#if defined(PIOS_INCLUDE_ANNUNC)
	const struct pios_annunc_cfg * led_cfg = PIOS_BOARD_HW_DEFS_GetLedCfg(bdinfo->board_rev);
	PIOS_Assert(led_cfg);
	PIOS_ANNUNC_Init(led_cfg);
#endif	/* PIOS_INCLUDE_ANNUNC */

#if defined(PIOS_INCLUDE_SPI)
	pios_spi_t pios_spi_internal_id;

	if (PIOS_SPI_Init(&pios_spi_internal_id, &pios_spi_internal_cfg)) {
		PIOS_DEBUG_Assert(0);
	}
#endif

#if defined(PIOS_INCLUDE_FLASH)
	/* Inititialize all flash drivers */
	if (PIOS_Flash_Internal_Init(&pios_internal_flash_id, &flash_internal_cfg) != 0)
		PIOS_HAL_CriticalError(PIOS_LED_ALARM, PIOS_HAL_PANIC_FLASH);


	/* Register the partition table */
	const struct pios_flash_partition * flash_partition_table;
	uint32_t num_partitions;
	flash_partition_table = PIOS_BOARD_HW_DEFS_GetPartitionTable(bdinfo->board_rev, &num_partitions);
	PIOS_FLASH_register_partition_table(flash_partition_table, num_partitions);

	/* Mount all filesystems */
	if (PIOS_FLASHFS_Logfs_Init(&pios_uavo_settings_fs_id, &flashfs_internal_settings_cfg, FLASH_PARTITION_LABEL_SETTINGS) != 0)
		PIOS_HAL_CriticalError(PIOS_LED_ALARM, PIOS_HAL_PANIC_FILESYS);
#endif	/* PIOS_INCLUDE_FLASH */

    PIOS_RESET_Clear(); // Clear the RCC reset flags after use.

	/* Initialize the hardware UAVOs */
	HwF3FcInitialize();
	ModuleSettingsInitialize();

#if defined(PIOS_INCLUDE_RTC)
	/* Initialize the real-time clock and its associated tick */
	PIOS_RTC_Init(&pios_rtc_main_cfg);
#endif

    /* Set up pulse timers */

	PIOS_TIM_InitClock(&tim_1_cfg);
    PIOS_TIM_InitClock(&tim_2_cfg);
	PIOS_TIM_InitClock(&tim_3_cfg);
	PIOS_TIM_InitClock(&tim_16_cfg);
	PIOS_TIM_InitClock(&tim_17_cfg);

	/* IAP System Setup */
	PIOS_IAP_Init();
	uint16_t boot_count = PIOS_IAP_ReadBootCount();
	if (boot_count < 3) {
		PIOS_IAP_WriteBootCount(++boot_count);
		AlarmsClear(SYSTEMALARMS_ALARM_BOOTFAULT);
	} else {
		/* Too many failed boot attempts, force hw config to defaults */
		HwF3FcSetDefaults(HwF3FcHandle(), 0);
		ModuleSettingsSetDefaults(ModuleSettingsHandle(),0);
		AlarmsSet(SYSTEMALARMS_ALARM_BOOTFAULT, SYSTEMALARMS_ALARM_CRITICAL);
	}

#if defined(PIOS_INCLUDE_USB)
	/* Initialize board specific USB data */
	PIOS_USB_BOARD_DATA_Init();

	/* Flags to determine if various USB interfaces are advertised */
	bool usb_hid_present = false;

#if defined(PIOS_INCLUDE_USB_CDC)
	bool usb_cdc_present = false;
	if (PIOS_USB_DESC_HID_CDC_Init()) {
		PIOS_Assert(0);
	}
	usb_hid_present = true;
	usb_cdc_present = true;
#else
	if (PIOS_USB_DESC_HID_ONLY_Init()) {
		PIOS_Assert(0);
	}
	usb_hid_present = true;
#endif

	uintptr_t pios_usb_id;
	PIOS_USB_Init(&pios_usb_id, PIOS_BOARD_HW_DEFS_GetUsbCfg(bdinfo->board_rev));

#if defined(PIOS_INCLUDE_USB_CDC)

	HwF3FcUSB_VCPPortOptions hw_usb_vcpport;
	/* Configure the USB VCP port */
	HwF3FcUSB_VCPPortGet(&hw_usb_vcpport);

	if (!usb_cdc_present) {
		/* Force VCP port function to disabled if we haven't advertised VCP in our USB descriptor */
		hw_usb_vcpport = HWF3FC_USB_VCPPORT_DISABLED;
	}

	PIOS_HAL_ConfigureCDC(hw_usb_vcpport, pios_usb_id, &pios_usb_cdc_cfg);
#endif	/* PIOS_INCLUDE_USB_CDC */

#if defined(PIOS_INCLUDE_USB_HID)
	/* Configure the usb HID port */
	HwF3FcUSB_HIDPortOptions hw_usb_hidport;
	HwF3FcUSB_HIDPortGet(&hw_usb_hidport);

	if (!usb_hid_present) {
		/* Force HID port function to disabled if we haven't advertised HID in our USB descriptor */
		hw_usb_hidport = HWF3FC_USB_HIDPORT_DISABLED;
	}

	PIOS_HAL_ConfigureHID(hw_usb_hidport, pios_usb_id, &pios_usb_hid_cfg);

#endif	/* PIOS_INCLUDE_USB_HID */
#endif	/* PIOS_INCLUDE_USB */

    /* Configure the IO ports */
	HwF3FcDSMxModeOptions hw_DSMxMode;
	HwF3FcDSMxModeGet(&hw_DSMxMode);

	/* UART1 Port */
	uint8_t hw_uart1;
	HwF3FcUart1Get(&hw_uart1);
    PIOS_HAL_ConfigurePort(hw_uart1,             // port type protocol
            &pios_uart1_cfg,                     // usart_port_cfg
            &pios_usart_com_driver,              // com_driver
            NULL,                                // i2c_id
            NULL,                                // i2c_cfg
            NULL,                                // ppm_cfg
            NULL,                                // pwm_cfg
            PIOS_LED_ALARM,                      // led_id
            &pios_uart1_dsm_bind_cfg,            // dsm_cfg
            hw_DSMxMode,                         // dsm_mode
            NULL);                               // sbus_cfg

	/* UART2 Port */
	uint8_t hw_uart2;
	HwF3FcUart2Get(&hw_uart2);
	PIOS_HAL_ConfigurePort(hw_uart2,             // port type protocol
            &pios_uart2_cfg,                     // usart_port_cfg
            &pios_usart_com_driver,              // com_driver
            NULL,                                // i2c_id
            NULL,                                // i2c_cfg
            NULL,                                // ppm_cfg
            NULL,                                // pwm_cfg
            PIOS_LED_ALARM,                      // led_id
            &pios_uart2_dsm_bind_cfg,            // dsm_cfg
            hw_DSMxMode,                         // dsm_mode
            NULL);                               // sbus_cfg

	/* UART3 Port */
	uint8_t hw_uart3;
	HwF3FcUart3Get(&hw_uart3);
	PIOS_HAL_ConfigurePort(hw_uart3,             // port type protocol
            &pios_uart3_cfg,                     // usart_port_cfg
            &pios_usart_com_driver,              // com_driver
            NULL,                                // i2c_id
            NULL,                                // i2c_cfg
            NULL,                                // ppm_cfg
            NULL,                                // pwm_cfg
            PIOS_LED_ALARM,                      // led_id
            &pios_uart3_dsm_bind_cfg,            // dsm_cfg
            hw_DSMxMode,                         // dsm_mode
            NULL);                               // sbus_cfg

	/* Configure the ppm rcvr port */
	uint8_t hw_ppm;
	HwF3FcPPM_ReceiverGet(&hw_ppm);

	if (hw_ppm == HWF3FC_PPM_RECEIVER_ENABLED) {
		PIOS_HAL_ConfigurePort(HWSHARED_PORTTYPES_PPM,  // port type protocol
                NULL,                                   // usart_port_cfg
                NULL,                                   // com_driver
                NULL,                                   // i2c_id
                NULL,                                   // i2c_cfg
                &pios_ppm_cfg,                          // ppm_cfg
                NULL,                                   // pwm_cfg
                PIOS_LED_ALARM,                         // led_id
                NULL,                                   // dsm_cfg
                0,                                      // dsm_mode
                NULL);                                  // sbus_cfg

		pios_servo_cfg.num_channels = 5;
	}
	else
	{
		pios_servo_cfg.num_channels = 6;
	}

#ifndef PIOS_DEBUG_ENABLE_DEBUG_PINS
#ifdef PIOS_INCLUDE_SERVO
	PIOS_Servo_Init(&pios_servo_cfg);
#endif
#else
	PIOS_DEBUG_Init(&pios_tim_servo_all_channels, NELEMENTS(pios_tim_servo_all_channels));
#endif

#if defined(PIOS_INCLUDE_ADC)
	uintptr_t unused_adc;
	if(PIOS_INTERNAL_ADC_Init(&pios_internal_adc_id, &internal_adc_cfg) < 0)
		PIOS_Assert(0);
	PIOS_ADC_Init(&unused_adc, &pios_internal_adc_driver, pios_internal_adc_id);
#endif /* PIOS_INCLUDE_ADC */

	PIOS_WDG_Clear();
	PIOS_DELAY_WaitmS(200);
	PIOS_WDG_Clear();

#if defined(PIOS_INCLUDE_MPU)
	pios_mpu_dev_t mpu_dev = NULL;
	
	if (PIOS_MPU_SPI_Init(&mpu_dev, pios_spi_internal_id, 0, &pios_mpu_cfg) != 0)
		PIOS_HAL_CriticalError(PIOS_LED_ALARM, PIOS_HAL_PANIC_IMU);

	HwF3FcGyroRangeOptions hw_gyro_range;
	HwF3FcGyroRangeGet(&hw_gyro_range);
	switch(hw_gyro_range) {
		case HWF3FC_GYRORANGE_250:
			PIOS_MPU_SetGyroRange(PIOS_MPU_SCALE_250_DEG);
			break;
		case HWF3FC_GYRORANGE_500:
			PIOS_MPU_SetGyroRange(PIOS_MPU_SCALE_500_DEG);
			break;
		case HWF3FC_GYRORANGE_1000:
			PIOS_MPU_SetGyroRange(PIOS_MPU_SCALE_1000_DEG);
			break;
		case HWF3FC_GYRORANGE_2000:
			PIOS_MPU_SetGyroRange(PIOS_MPU_SCALE_2000_DEG);
			break;
	}

	HwF3FcAccelRangeOptions hw_accel_range;
	HwF3FcAccelRangeGet(&hw_accel_range);
	switch(hw_accel_range) {
		case HWF3FC_ACCELRANGE_2G:
			PIOS_MPU_SetAccelRange(PIOS_MPU_SCALE_2G);
			break;
		case HWF3FC_ACCELRANGE_4G:
			PIOS_MPU_SetAccelRange(PIOS_MPU_SCALE_4G);
			break;
		case HWF3FC_ACCELRANGE_8G:
			PIOS_MPU_SetAccelRange(PIOS_MPU_SCALE_8G);
			break;
		case HWF3FC_ACCELRANGE_16G:
			PIOS_MPU_SetAccelRange(PIOS_MPU_SCALE_16G);
			break;
	}

	// the filter has to be set before rate else divisor calculation will fail
	HwF3FcMPU6000DLPFOptions hw_mpu_dlpf;
	HwF3FcMPU6000DLPFGet(&hw_mpu_dlpf);
	uint16_t bandwidth = \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_188) ? 188 : \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_98)  ? 98  : \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_42)  ? 42  : \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_20)  ? 20  : \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_10)  ? 10  : \
		(hw_mpu_dlpf == HWF3FC_MPU6000DLPF_5)   ? 5   : \
		184;
	PIOS_MPU_SetGyroBandwidth(bandwidth);

#endif /* PIOS_INCLUDE_MPU */

	//I2C is slow, sensor init as well, reset watchdog to prevent reset here
	PIOS_WDG_Clear();

	/* Make sure we have at least one telemetry link configured or else fail initialization */
	PIOS_Assert(pios_com_telem_serial_id || pios_com_telem_usb_id);

}

/**
 * @}
 */
