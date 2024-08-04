module mod_decl #(parameter p1    = 3, parameter int unsigned  K_P2 =    3) (
input  logic  i_clk, //! Blep !
      input logic       [3:0] i_data  ,    //! Far away !
      input logic [3:0][K_DWIDTH-1:0] i_toto_blep_mem //! Miaou
);
localparam int K_DIM2=5;
localparam logic [15:0] K_DIM2=5;
parameter int toto = 2; 

static const logic  multi_unpacked [3:0][31:0];
const static  logic  multi_unpacked [3:0][31:0];

endmodule;