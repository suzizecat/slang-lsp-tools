module top (
	input logic i_clk,
	input logic i_rst_n,
	output logic [3:0] o_pulse1,
	output logic [1:0] o_pulse2
);


	sub #(.K_NOUT(4)) u_sub (
		.i_clk(i_clk),
		.i_rst_n(i_rst_n),
		.o_out(o_pulse1)
	);

	sub #(.K_NOUT(2)) u_sub2 (
		.i_clk(i_clk),
		.i_rst_n(i_rst_n),
		.o_out(o_pulse2)
	);

endmodule