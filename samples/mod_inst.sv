module test();

mymod #(.TEST(3),.SEC(4)) u_instance 
(
    .i_clk(clk),
    .i_rst_n(rst_n),
    .o_toto()
);


mymod #(
.TEST(3),
.SEC(K_DIMENSION)) 
u_instance2
(
    .i_clk(clk), //! System clock
    .i_rst_n(rst_n) ,    //! Ze reset
    .o_toto()//! The output
);

endmodule