#ifndef XHVADD_H
#define XHVADD_H

#include "xil_types.h"
#include "xiltimer.h"
#include "xil_io.h"
#include "xparameters.h"
#include <stdlib.h>

typedef enum {  
    op_reg         = 0 * 4,
    addr_reg       = 1 * 4,
    status_reg     = 2 * 4,
    data_read_reg  = 3 * 4,
    data_write_reg = 4 * 4,
    veca_addr_reg  = 5 * 4,
    vecb_addr_reg  = 6 * 4,
    vecr_addr_reg  = 7 * 4,
    vec_len_reg    = 8 * 4 
} regAddr;

typedef enum {  
    wait_op  = 0,
    read_op  = 1,
    write_op = 2,
    add_op   = 3 
} opValue;

typedef struct XHVADD {
    u32 base_addr;
    u32 mem_bits;
} XHVADD;

unsigned hvadd_test_axil(XHVADD hvadd);
unsigned hvadd_sanity_test(XHVADD hvadd);
void     hvadd_timing_tests(XHVADD hvadd);
unsigned hvadd_random_tests(XHVADD hvadd);
void     hvadd_randomize_srcs_content_and_write_to_mem(XHVADD hvadd, u32 addra, u32 addrb, s32* srca, s32* srcb, u32 len);
void     hvadd_wait_for_completion(XHVADD hvadd);
void     hvadd_set_wait(XHVADD hvadd);
s32      hvadd_read_from(XHVADD hvadd,u32 addr);
void     hvadd_write_to(XHVADD hvadd,u32 addr, u32 data);
void     hvadd_add_vectors(XHVADD hvadd,u32 addra, u32 addrb, u32 addrr, u32 len);

void compute_reference(s32* srca, s32* srcb, s32* srcc, const u32 len);

#endif
