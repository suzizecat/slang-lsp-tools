module a ()

localparam K_DIM=3;

typedef logic [10:2] my_type_t;

logic single_unpacked [3:0];
logic [1:0][K_DIIM-1:0] multi_packed_param_and_more;
logic [1:TRUC][3:0] multi_packed;

logic multi_unpacked_weird [3][K_DIMENSION];
logic [1:0][15:0] full_mix [3][31];

logic simple;
logic [TOTTOslkjh:0] single_packed; //! A comment
logic [1:0][K_DIImdspoijfkgIM-1:0] multi_packed_param;

    // This is a dumb comment
static const logic  multi_unpacked [3:0][31:0];

logic multi_unpacked_single [3][31]; 

my_type_t custom_type;
logic [31:0] an_array;

//  logic [1:0   ][K_DIIM-1:0] multi_packed_param_and_more      ; 
//  logic [1:TRUC][3       :0] multi_packed               ;

endmodule