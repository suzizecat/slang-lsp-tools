// Code your design here

interface dut_if;

endinterface


module dut(
    input logic  i_clk, //!
    output logic  o_out, //!
    
    dut_if dif);

    assign o_out = i_clk;

endmodule
