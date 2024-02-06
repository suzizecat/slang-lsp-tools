module a (
    input  logic  i_clk, //! Blep !
      input logic       [3:0] i_data,
      input logic [3:0][K_DWIDTH-1:0] i_toto_blep_mem //! Miaou
);

localparam K_DIM=3;

typedef logic [10:2] my_type_t;

// This one is not aligned
    // This is a silly comment
static const logic  multi_unpacked [3:0][31:0];
const static  logic  multi_unpacked [3:0][31:0];
 const logic  multi_unpacked_const [3:0][31:0];
 static logic  multi_unpacked_static [3:0][31:0];

logic multi_unpacked_single [3][321]; 
logic multi_unpacked_single [3][31]; 
logic multi_unpacked_single [3][3112]; 

logic single_unpacked [3:0];
logic [1:0][K_DIM-1:0] multi_packed_param_and_more;
logic [1:THE_FOO][3:0] multi_packed;

logic multi_unpacked_weird [3][K_DIMENSION];
logic [1:0][15:0] full_mix [3][31];

logic simple;
logic [THE_LONG_BAR:0] single_packed; //! A comment



// Another signal
logic [1:0][K_DIM-1:0] multi_packed_param; //! Commented !

my_type_t custom_type;
my_type_t [1:0][3:0] custom_type;
my_type_t [K_DWIDTH-1:0] custom_type [3:0];

    logic a, b;

    assign a = b;


logic [31:0] an_array;

endmodule