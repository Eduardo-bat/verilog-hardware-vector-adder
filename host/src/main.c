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

#define BRAM_DEVICE_ID	XPAR_XBRAM_0_BASEADDR
#define HVADD_DEVICE_ID XPAR_VEC_ADDER_0_BASEADDR

unsigned validate_results(u32 mem_base_addr, u32 addrc, s32* srcc, const u32 len);
void fill_vector_from_src(u32 mem_base_addr, u32 addr, s32* src, const u32 len);
void add_vectors(u32 mem_base_addr, u32 addra, u32 addrb, u32 addrc, const u32 len);
void read_vector(u32 mem_base_addr, u32 addr, const u32 len);
void timing_tests();

unsigned run_test(unsigned (*test_function)(XHVADD hvadder), XHVADD hvadder, const char* msg);

void software_add_over_ps_memory();

int main(void) {
    xil_printf("********************************\n\r");
    xil_printf("*********HARD ADDER TEST********\n\r");
    xil_printf("********************************\n\r");

    sleep(1);
    usleep(10);

    XBram Bram;

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
