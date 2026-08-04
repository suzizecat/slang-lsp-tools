module myMod();
	logic a;
	logic [2:0] b;
	generate
		for(genvar i = 0; i<3; i++) begin : g_test
			assign b[i] = a;
		end
	endgenerate;
endmodule;