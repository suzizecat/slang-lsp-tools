module a ()

localparam K_DIM=3;

logic simple;
logic [1:0] single_packed;
logic [1:0][3:0] multi_packed;
logic [1:0][K_DIIM-1:0] multi_packed_param;
logic single_unpacked [3:0];
    // This is a dumb comment
static const logic  multi_unpacked [3:0][31:0];
logic multi_unpacked_single [3][31];
logic multi_unpacked_weird [3][K_DIMENSION];

logic [1:0][15:0] full_mix [3][31];




endmodule