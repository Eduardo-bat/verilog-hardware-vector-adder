`timescale 1ns / 1ps

module hvadd_tb();

`define PS7 hvadd_tb.add_inst.vec_adder_i.processing_system7_0.inst

localparam logic [31:0] base_addr = 32'h4000_4000;

localparam logic [31:0] op_reg         = 0 * 4,
                        addr_reg       = 1 * 4,
                        status_reg     = 2 * 4,
                        data_read_reg  = 3 * 4,
                        data_write_reg = 4 * 4,
                        veca_addr_reg  = 5 * 4,
                        vecb_addr_reg  = 6 * 4,
                        vecr_addr_reg  = 7 * 4,
                        vec_len_reg    = 8 * 4;
                        
localparam logic [31:0] wait_op  = 0,
                        read_op  = 1,
                        write_op = 2,
                        add_op   = 3;
                        
localparam logic [31:0] status_waiting    = 0,
                        status_done       = 1,
                        status_other_busy = 2,
                        status_reading    = 3,
                        status_writing    = 4,
                        status_adding     = 5;
                        
localparam int mem_bits = 14;
localparam int mem_size = 2**mem_bits;
localparam int max_vec_l = mem_size / 3 - 4 ;

int output_fd;

logic [31:0] rd_srca [max_vec_l-1:0];
logic [31:0] rd_srcb [max_vec_l-1:0];
logic [31:0] rd_srcr [max_vec_l-1:0];

logic tb_ACLK;
logic tb_ARESETn;

logic [31:0] reg_addr, reg_data;

logic [31:0] data_read;
logic resp;

wire temp_clk;
wire temp_rstn;

initial begin
    tb_ACLK = 1'b0;
end

always #5 tb_ACLK = !tb_ACLK;

assign temp_clk = tb_ACLK;
assign temp_rstn = tb_ARESETn;

initial begin

    $timeformat(-9, 0, " ns", 10);
    
    output_fd = $fopen("./output_file.txt", "w");
    if(output_fd == 0) begin
        $display("couldn't open file"); 
        $finish;
    end
    
    clock_cycles(10);
    tb_ARESETn = 0;
    clock_cycles(10);
    tb_ARESETn = 1;
    clock_cycles(10);
    
    `PS7.fpga_soft_reset(32'h1);
    clock_cycles(20);
    `PS7.fpga_soft_reset(32'h0);
    clock_cycles(20);
    
    `PS7.set_debug_level_info(1'b0);
    
    axil_tests();
    sanity_tests();
    random_tests();
    timing_tests();
    
    $fclose(output_fd);
    $finish;
    
end

vec_adder_wrapper add_inst
   (.DDR_addr(),
    .DDR_ba(),
    .DDR_cas_n(),
    .DDR_ck_n(),
    .DDR_ck_p(),
    .DDR_cke(),
    .DDR_cs_n(),
    .DDR_dm(),
    .DDR_dq(),
    .DDR_dqs_n(),
    .DDR_dqs_p(),
    .DDR_odt(),
    .DDR_ras_n(),
    .DDR_reset_n(),
    .DDR_we_n(),
    .FIXED_IO_ddr_vrn(),
    .FIXED_IO_ddr_vrp(),
    .FIXED_IO_mio(),
    .FIXED_IO_ps_clk(temp_clk),
    .FIXED_IO_ps_porb(temp_rstn),
    .FIXED_IO_ps_srstb(temp_rstn));
 
task axil_tests();

    $fwrite(output_fd, "axil tests: ");
    
    `PS7.write_data(base_addr, 4, 32'h1111, resp);
    
    `PS7.read_data(base_addr, 4, data_read, resp);
    
    $fwrite(output_fd, "axil enviroment validated\n");
    
    $display("axil passed");

endtask
    
task sanity_tests();
    static int addra = 4, addrb = 8, addrr = 16, len = 1 * 4, vala = 16, valb = 32;
        
    $fwrite(output_fd, "sanity tests: ");
    
    write_to(addra, vala);
    read_from(addra);
    
    write_to(addrb, valb);
    read_from(addrb);

    add_vectors(addra, addrb, addrr, len);
    
    read_from(addrr);
    
    assert (data_read == vala + valb)
    else $error("sanity failed");
    
    $fwrite(output_fd, "passed\n");
    
    $display("sanity passed");
    
endtask
    
task timing_tests();
    static int addra = 0, addrb = max_vec_l + 4, addrr = 2 * max_vec_l + 8;
    int cell_pairs;
    time start_t, end_t;
    
    $fwrite(output_fd, "timing tests:\n");
    
    for (int i = 0; i < mem_bits - 1 - 2 /*divide by 4 so its words*/ - 2 /*divide by 4 to compensate byte off*/; i ++) begin
    
        cell_pairs = 2**(i + 2);
        
        start_t = $time;
        add_vectors(addra, addrb, addrr, cell_pairs);
        end_t = $time;
        
        $fwrite(output_fd, "timing: add %d cell pairs took %t\n", cell_pairs, end_t - start_t);
        
        $display("timing: add %d cell pairs took %t", cell_pairs, end_t - start_t);
        
    end
    
    $display("timing concluded");
    
endtask

task print_src(input logic [31:0] src [max_vec_l-1:0], input int len, input string name);
    $write("%s: ", name);
    for (int k = 0; k < len; k ++) begin
        $write("%d ", src[k]);
    end
    $display("");
endtask

task print_vector(input int addr, input int len, input string name);
    $write("%s: ", name);
    for (int k = 0; k < len; k += 4) begin
        read_from(addr + k);
        $write("%d ", data_read);
    end
    $display("");
endtask

task random_tests();
    static int test_qtty = 16, max_rd_len = 16 * 4;
    int addra, addrb, addrr, len;

    $fwrite(output_fd, "random tests: ");
    
    for (int i = 0; i < test_qtty; i ++) begin
    
        len   = $urandom_range(max_rd_len) & 2'b00 + 4;
        addra = $urandom_range(10) * 4 + 000;
        addrb = $urandom_range(10) * 4 + 200;
        addrr = $urandom_range(10) * 4 + 400;
        
        randomize_srcs_content_and_write_to_mem(addra, addrb, len);
        compute_reference(len / 4);
        
        add_vectors(addra, addrb, addrr, len);
        
        for (int j = 0; j < len; j += 4) begin
            
            read_from(addrr + j);
            
            assert (data_read == rd_srcr[j / 4])
            else begin
                $display("error! reference: %d, read: %d", rd_srcr[j], data_read);
                $stop;
            end
        end
    
    end
    
    $fwrite(output_fd, "passed\n");
    
    $display("random tests passed");

endtask

task compute_reference(input int len);

    for (int i = 0; i < len; i ++) begin
    
        rd_srcr[i] = rd_srca[i] + rd_srcb[i];
        
    end
    
endtask

task randomize_srcs_content_and_write_to_mem(input int addra, input int addrb, input int len);
    static int range = 128;
    int rd_write_val;

    for (int i = 0; i < len; i += 4) begin
        
        rd_write_val = $urandom_range(range) - range / 2;
        rd_srca[i / 4] = rd_write_val;
        write_to(addra + i, rd_write_val);
        
        rd_write_val = $urandom_range(range) - range / 2;
        rd_srcb[i / 4] = rd_write_val;
        write_to(addrb + i, rd_write_val);
        
    end

endtask

task clock_cycles(input int n);
    repeat (n)
        @(posedge tb_ACLK);
endtask

task wait_for_completion();

    reg_addr = status_reg;
    
    do begin
        `PS7.read_data(base_addr + reg_addr, 4, reg_data, resp);
    end while (reg_data != status_done);

endtask

task set_wait();

    reg_addr = op_reg;
    reg_data = wait_op;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);

endtask

task read_from(input logic [31:0] addr);

    reg_addr = addr_reg;
    reg_data = addr;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = op_reg;
    reg_data = read_op;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    wait_for_completion();
    
    reg_addr = data_read_reg;
    `PS7.read_data(base_addr + reg_addr, 4, data_read, resp);
    
    set_wait();

endtask

task write_to(input logic [31:0] addr, input logic [31:0] data);

    reg_addr = addr_reg;
    reg_data = addr;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = data_write_reg;
    reg_data = data;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = op_reg;
    reg_data = write_op;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    wait_for_completion();
    set_wait();

endtask

task add_vectors(input logic [31:0] addra, input logic [31:0] addrb, input logic [31:0] addrr, input int len);

    reg_addr = veca_addr_reg;
    reg_data = addra;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = vecb_addr_reg;
    reg_data = addrb;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = vecr_addr_reg;
    reg_data = addrr;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);

    reg_addr = vec_len_reg;
    reg_data = len;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    reg_addr = op_reg;
    reg_data = add_op;
    `PS7.write_data(base_addr + reg_addr, 4, reg_data, resp);
    
    wait_for_completion();
    set_wait();
    
endtask

endmodule
