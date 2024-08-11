module bram #(
		parameter PORT_WIDTH        = 32,
		parameter RAM_ADDR_BITS 	= 12
	) (
        input							clock,
        
        input							ram_enable_a,
        input							write_enable_a,
        input 		[RAM_ADDR_BITS-1:0]	address_a,
        input 		[PORT_WIDTH-1:0] 	input_data_a,
        output reg 	[PORT_WIDTH-1:0] 	output_data_a,
        
        input							ram_enable_b,
        input							write_enable_b,
        input 		[RAM_ADDR_BITS-1:0]	address_b,
        input 		[PORT_WIDTH-1:0] 	input_data_b,
        output reg 	[PORT_WIDTH-1:0] 	output_data_b
	);
	
    (* RAM_STYLE="BLOCK" *)
    reg [PORT_WIDTH-1:0] ram [2**RAM_ADDR_BITS-1:0];
    
    always @(posedge clock) begin
        if (ram_enable_a) begin
            if (write_enable_a) begin
                ram[address_a] <= input_data_a;
            end
            output_data_a <= ram[address_a];
        end
    end
    
    always @(posedge clock) begin
        if (ram_enable_b) begin
            if (write_enable_b) begin
                ram[address_b] <= input_data_b;
            end            
            output_data_b <= ram[address_b];
        end
    end

endmodule
