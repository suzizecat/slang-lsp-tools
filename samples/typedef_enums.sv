module typedef_enum();


typedef enum
{
  UVM_NONE   = 0,
  UVM_LOW    = 100,
  UVM_MEDIUM = 200,
  UVM_HIGH   = 300,
  UVM_FULL   = 400,
  UVM_DEBUG  = 500
} uvm_verbosity;

uvm_verbosity v;
initial begin
 v = UVM_HIGH;
end

endmodule

