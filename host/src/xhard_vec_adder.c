#include "Includes/xhard_vec_adder.h"
#include "xiltimer.h"
#include <xil_io.h>

unsigned hvadder_test_axil(XHVADD hvadder) {
    s32 data_read;

    Xil_Out32(hvadder.base_addr + 4, 0xcafe);
    data_read = Xil_In32(hvadder.base_addr + 4);

    return !(data_read == 0xcafe);
}

unsigned hvadder_sanity_test(XHVADD hvadder) {
    const u32 addra = 2, addrb = 4, addrr = 8, len = 1 * 4;
    const s32 vala = 16, valb = 32;
    s32 data_read;

    hvadder_write_to(hvadder, addra, vala);
    hvadder_write_to(hvadder, addrb, valb);

    data_read = hvadder_read_from(hvadder, addra);
    data_read = hvadder_read_from(hvadder, addrb);

    hvadder_add_vectors(hvadder, addra, addrb, addrr, len);

    data_read = hvadder_read_from(hvadder, addrr);

    return !(data_read == vala + valb);
}

void hvadder_timing_tests(XHVADD hvadder) {
    const double twoticksns = (XPAR_CPU_CORE_CLOCK_FREQ_HZ * 1e-9) / 2.0;
    const u32 max_vec_l = (1 << hvadder.mem_bits) / 3 - 4;
    const u32 addra = 0 * max_vec_l,
              addrb = 1 * max_vec_l,
              addrr = 2 * max_vec_l;

    u32 cell_pairs;
    double time_diff;
    XTime startt, endt;
    
    xil_printf("timing tests:\r\n");
    
    for (u32 i = 0; i < hvadder.mem_bits - 1 - 2 - 2; i ++) {
        cell_pairs = 1 << (i + 2);
    
        XTime_GetTime(&startt);
        hvadder_add_vectors(hvadder, addra, addrb, addrr, cell_pairs);
        XTime_GetTime(&endt);

        time_diff = endt - startt;
        xil_printf("add\t%u pairs took\t%lu ns\r\n", cell_pairs, (u64) (time_diff / twoticksns));
    }
    
    xil_printf("timing concluded");
}

unsigned hvadder_random_tests(XHVADD hvadder) {
    const int test_qtty = 16, max_rd_len = 16, addr_range = 16;
    u32 addra, addrb, addrr, len;
    s32 srca[max_rd_len], srcb[max_rd_len], srcr[max_rd_len];
    s32 data_read;
    unsigned error_count = 0;
    
    for (int i = 0; i < test_qtty; i ++) {
        len   = rand() % max_rd_len * 4 + 4;
        addra = rand() % addr_range * 4 + 000;
        addrb = rand() % addr_range * 4 + 200;
        addrr = rand() % addr_range * 4 + 400;
        
        hvadder_randomize_srcs_content_and_write_to_mem(hvadder, addra, addrb, srca, srcb, len);
        compute_reference(srca, srcb, srcr, len / 4);
        
        hvadder_add_vectors(hvadder, addra, addrb, addrr, len);
        
        for (u32 j = 0; j < len; j += 4 ) {
            data_read = hvadder_read_from(hvadder, addrr + j);

            if (!(data_read == srcr[j / 4])) {
                error_count ++;
            }
        }
    }

    return error_count;
}

void hvadder_randomize_srcs_content_and_write_to_mem(XHVADD hvadder, u32 addra, u32 addrb, s32* srca, s32* srcb, u32 len) {
    const s32 range = 128;
    s32 rd_write_val;

    for (u32 i = 0; i < len; i += 4) {
        rd_write_val = rand() % range - range / 2;
        srca[i / 4] = rd_write_val;
        hvadder_write_to(hvadder, addra + i, rd_write_val);
        
        rd_write_val = rand() % range - range / 2;
        srcb[i / 4] = rd_write_val;
        hvadder_write_to(hvadder, addrb + i, rd_write_val);
    }
}

void hvadder_wait_for_completion(XHVADD hvadder) {
    s32 reg_data;

    do {
        reg_data = Xil_In32(hvadder.base_addr + status_reg);
    } while(!(reg_data == 1));
}

void hvadder_set_wait(XHVADD hvadder) {
    Xil_Out32(hvadder.base_addr + op_reg, wait_op);
}

s32 hvadder_read_from(XHVADD hvadder,u32 addr) {
    s32 data_read;

    Xil_Out32(hvadder.base_addr + addr_reg, addr);
    Xil_Out32(hvadder.base_addr + op_reg, read_op);
    
    hvadder_wait_for_completion(hvadder);
    
    data_read = Xil_In32(hvadder.base_addr + data_read_reg);

    hvadder_set_wait(hvadder);

    return data_read;
}

void hvadder_write_to(XHVADD hvadder,u32 addr, u32 data) {
    Xil_Out32(hvadder.base_addr + addr_reg, addr);
    Xil_Out32(hvadder.base_addr + data_write_reg, data);
    Xil_Out32(hvadder.base_addr + op_reg, write_op);
    
    hvadder_wait_for_completion(hvadder);
    hvadder_set_wait(hvadder);
}

void hvadder_add_vectors(XHVADD hvadder, u32 addra, u32 addrb, u32 addrr, u32 len) {
    Xil_Out32(hvadder.base_addr + veca_addr_reg, addra);
    Xil_Out32(hvadder.base_addr + vecb_addr_reg, addrb);
    Xil_Out32(hvadder.base_addr + vecr_addr_reg, addrr);
    Xil_Out32(hvadder.base_addr + vec_len_reg, len);
    Xil_Out32(hvadder.base_addr + op_reg, add_op);
    
    hvadder_wait_for_completion(hvadder);
    hvadder_set_wait(hvadder);
}

void compute_reference(s32* srca, s32* srcb, s32* srcc, const u32 len) {
    for (u32 i = 0; i < len; i ++) {
        srcc[i] = srca[i] + srcb[i];
    }
}
