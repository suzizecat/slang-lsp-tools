module sub #(
	parameter int K_NOUT = 1
)  (
	input logic i_clk,
	input logic i_rst_n,
	output logic [K_NOUT-1:0] o_out
);

logic [K_NOUT-1:0] toggle;

assign o_out = toggle;

generate
    genvar t;
	for(genvar i = 0; i < K_NOUT; i++) begin : g_loop

		always_ff @(posedge i_clk or negedge i_rst_n) begin : p_ff_process
			if(~i_rst_n) begin
				toggle[i] <= 0;
			end else begin
				toggle[i] <= ~toggle[i];
			end
		end

	end
endgenerate
endmodule 
