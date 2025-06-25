import td_pkg::* ;
//import td_pkg::uvm_verbosity ;
module typedef_enum();

uvm_verbosity v;
initial begin
    calc_parity();
 v = UVM_HIGH;
end

endmodule

