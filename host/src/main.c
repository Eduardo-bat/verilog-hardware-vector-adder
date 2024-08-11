/******************************************************************************
* Copyright (C) 2010 - 2021 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2022 - 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
* @file xbram_example.c
*
* This file contains a self test example using the BRAM driver (XBram).
*
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date	 Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a sa   05/11/10 Initial release.
* 3.01a sa   13/01/12 Changed XBram_SelfTest(InstancePtr) to
* 			 XBram_SelfTest(InstancePtr,0) as per
*			 new API (CR 639274)
* 4.1   ms   01/23/17 Modified xil_printf statement in main function to
*                     ensure that "Successfully ran" and "Failed" strings are
*                     available in all examples. This is a fix for CR-965028.
* 4.9   sd   07/07/23 Added SDT support.
*</pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xbram.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/_types.h>
#include <xbram_hw.h>
#include <xil_printf.h>
#include <math.h>
#include <xiltimer.h>
#include <sleep.h>
#include "Includes/xhard_vec_adder.h"

/************************** Constant Definitions *****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#ifndef SDT
#define BRAM_DEVICE_ID		XPAR_BRAM_0_DEVICE_ID
#else
#define BRAM_DEVICE_ID		XPAR_XBRAM_0_BASEADDR
#endif

#define HVADD_DEVICE_ID XPAR_VEC_ADDER_0_BASEADDR


/************************** Function Prototypes ******************************/

#ifndef SDT
int BramExample(u16 DeviceId);
#else
int BramExample(UINTPTR BaseAddress);
#endif
static void InitializeECC(XBram_Config *ConfigPtr, u32 EffectiveAddr);

unsigned validate_results(u32 mem_base_addr, u32 addrc, s32* srcc, const u32 len);
void fill_vector_from_src(u32 mem_base_addr, u32 addr, s32* src, const u32 len);
void add_vectors(u32 mem_base_addr, u32 addra, u32 addrb, u32 addrc, const u32 len);
void read_vector(u32 mem_base_addr, u32 addr, const u32 len);
void timing_tests();

unsigned run_test(unsigned (*test_function)(XHVADD hvadder), XHVADD hvadder, const char* msg);

void software_add_over_ps_memory();


/************************** Variable Definitions *****************************/

/*
 * The following are declared globally so they are zeroed and so they are
 * easily accessible from a debugger
 */
XBram Bram;	/* The Instance of the BRAM Driver */


/****************************************************************************/
/**
*
* This function is the main function of the BRAM example.
*
* @param	None.
*
* @return
*		- XST_SUCCESS to indicate success.
*		- XST_FAILURE to indicate failure.
*
* @note		None.
*
*****************************************************************************/
#ifndef TESTAPP_GEN
int main(void) {

    xil_printf("********************************\n\r");
    xil_printf("*********HARD ADDER TEST********\n\r");
    xil_printf("********************************\n\r");

    sleep(1);
    usleep(10);

    XBram_Config *ConfigPtr;
    ConfigPtr = XBram_LookupConfig(BRAM_DEVICE_ID);

    u32 memBase = ConfigPtr->MemBaseAddress;

    const u32 len = 8;

    s32 srca[] = {0, 1, 2, 3, 4, 5, 6, 7};
    s32 srcb[] = {7, 6, 5, 4, 3, 2, 1, 0};
    s32 srcc[len];

    u32 addra = 0 * len * 4;
    u32 addrb = 2 * len * 4;
    u32 addrc = 4 * len * 4;

    xil_printf("software add over PS RAM:\n\r");
    software_add_over_ps_memory();

    xil_printf("software add over PL RAM:\r\n");
    compute_reference(srca, srcb, srcc, len);
    fill_vector_from_src(memBase, addra, srca, len);
    fill_vector_from_src(memBase, addrb, srcb, len);
    add_vectors(memBase, addra, addrb, addrc, len);
    timing_tests(memBase);

    XHVADD hvadder;
    hvadder.base_addr = XPAR_VEC_ADDER_0_BASEADDR;
    hvadder.mem_bits = 14;

    xil_printf("hardware add over PL RAM:\r\n");
    
    if (run_test(hvadder_test_axil, hvadder, "axil tests: ")) {
        return XST_FAILURE;
    }

    if (run_test(hvadder_sanity_test, hvadder, "sanity tests: ")) {
        return XST_FAILURE;
    }

    if (run_test(hvadder_random_tests, hvadder, "random tests: ")) {
        return XST_FAILURE;
    }

    hvadder_timing_tests(hvadder);

	return XST_SUCCESS;
}
#endif

void software_add_over_ps_memory() {
    const unsigned size = 1024;
    volatile int vec_0[size], vec_1[size], vec_r[size];
    const double twoticksns = (XPAR_CPU_CORE_CLOCK_FREQ_HZ * 1e-9) / 2.0;
    double time_diff;
    XTime startt, endt;

    for (volatile unsigned i = 0; i < size; i ++) {
        vec_0[i] = rand();
        vec_1[i] = rand();
    }

    for (unsigned len = 4; len <= size; len = len * 2) {
        XTime_GetTime(&startt);
        for (volatile unsigned i = 0; i < len; i ++) {
            vec_r[i] = vec_0[i] + vec_1[i];
        }
        XTime_GetTime(&endt);
        time_diff = endt - startt;
        xil_printf("add\t%u pairs took\t%lu ns\r\n", len, (u64) (time_diff / twoticksns));
    }

}

unsigned run_test(unsigned (*test_function)(XHVADD hvadder), XHVADD hvadder, const char* msg) {
    unsigned errors;

    xil_printf(msg);
    errors = test_function(hvadder);
    xil_printf("%s\r\n", errors ? "failed" : "passed");

    return errors;
}

void timing_tests(u32 mem_base_addr) {
    const u32 mem_bits = 12;
    const u32 mem_size = 1 << mem_bits;
    const double twoticksns = (XPAR_CPU_CORE_CLOCK_FREQ_HZ * 1e-9) / 2.0;
    double time_diff;
    XTime startt, endt;

    for (u32 i = 0; i < mem_size; i += 4) {
        XBram_Out32(mem_base_addr + i, rand());
    }

    for (u32 i = 2; i < mem_bits - 1; i ++) {
        u32 cell_pairs = 1 << i;
        XTime_GetTime(&startt);
        add_vectors(mem_base_addr, 0, cell_pairs + 1, 2 * (cell_pairs + 1), cell_pairs);
        XTime_GetTime(&endt);
        time_diff = endt - startt;
        xil_printf("add\t%u pairs took\t%lu ns\r\n", cell_pairs, (u64) (time_diff / twoticksns));
    }
}

void read_vector(u32 mem_base_addr, u32 addr, const u32 len) {
    for (u32 i = 0; i < len; i ++) {
        xil_printf("%d\t", XBram_In32(mem_base_addr + addr + i * 4));
    }
    xil_printf("\r\n");
}

unsigned validate_results(u32 mem_base_addr, u32 addrc, s32* srcc, const u32 len) {
    unsigned errors = 0;
    for (u32 i = 0; i < len; i ++) {
        s32 read_data = XBram_In32(mem_base_addr + addrc + i * 4);
        s32 refr_data = srcc[i];

        if(read_data != refr_data)
            errors ++;
    }

    return errors;
}

void fill_vector_from_src(u32 mem_base_addr, u32 addr, s32* src, const u32 len) {
    for (u32 i = 0; i < len; i ++) {
        XBram_Out32(mem_base_addr + addr + i * 4, src[i]);
    }
}

void add_vectors(u32 mem_base_addr, u32 addra, u32 addrb, u32 addrc, const u32 len) {
    for (u32 i = 0; i < len; i ++) {
        s32 sum = XBram_In32(mem_base_addr + addra + i * 4)
                    + XBram_In32(mem_base_addr + addrb + i * 4);

        XBram_Out32(mem_base_addr + addrc + i * 4, sum);
    }

}

/*****************************************************************************/
/**
*
* This is the entry point for the BRAM example.
*
* @param	DeviceId is the XPAR_<BRAM_instance>_DEVICE_ID value from
*		xparameters.h
*
* @return
*		- XST_SUCCESS to indicate success.
*		- XST_FAILURE to indicate failure.
*
* @note		None.
*
******************************************************************************/
#ifndef SDT
int BramExample(u16 DeviceId)
#else
int BramExample(UINTPTR BaseAddress)
#endif
{
	int Status;
	XBram_Config *ConfigPtr;

	/*
	 * Initialize the BRAM driver. If an error occurs then exit
	 */

	/*
	 * Lookup configuration data in the device configuration table.
	 * Use this configuration info down below when initializing this
	 * driver.
	 */
#ifndef SDT
	ConfigPtr = XBram_LookupConfig(DeviceId);
#else
	ConfigPtr = XBram_LookupConfig(BaseAddress);
#endif
	if (ConfigPtr == (XBram_Config *) NULL) {
		return XST_FAILURE;
	}

	Status = XBram_CfgInitialize(&Bram, ConfigPtr,
				     ConfigPtr->CtrlBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


        InitializeECC(ConfigPtr, ConfigPtr->CtrlBaseAddress);


	/*
	 * Execute the BRAM driver selftest.
	 */
	Status = XBram_SelfTest(&Bram, 0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}


/****************************************************************************/
/**
*
* This function ensures that ECC in the BRAM is initialized if no hardware
* initialization is available. The ECC bits are initialized by reading and
* writing data in the memory. This code is not optimized to only read data
* in initialized sections of the BRAM.
*
* @param	ConfigPtr is a reference to a structure containing information
*		about a specific BRAM device.
* @param 	EffectiveAddr is the device base address in the virtual memory
*		address space.
*
* @return
*		None
*
* @note		None.
*
*****************************************************************************/
void InitializeECC(XBram_Config *ConfigPtr, u32 EffectiveAddr)
{
	u32 Addr;
	volatile u32 Data;

	if (ConfigPtr->EccPresent &&
	    ConfigPtr->EccOnOffRegister &&
	    ConfigPtr->EccOnOffResetValue == 0 &&
	    ConfigPtr->WriteAccess != 0) {
		for (Addr = ConfigPtr->MemBaseAddress;
		     Addr < ConfigPtr->MemHighAddress; Addr+=4) {
			Data = XBram_In32(Addr);
			XBram_Out32(Addr, Data);
		}
		XBram_WriteReg(EffectiveAddr, XBRAM_ECC_ON_OFF_OFFSET, 1);
	}
}
