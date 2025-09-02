//import td_pkg::* ;
import td_pkg::uvm_verbosity ;
import td_pkg::UVM_HIGH ;
module typedef_enum();


	typedef enum logic[4:0] { IDLE, CALIBRATING, ACQUIRING, DONE } fsm_state_t;

	fsm_state_t state     ;
	fsm_state_t state_next;

uvm_verbosity v;
initial begin
 v = UVM_HIGH;
 state = IDLE;
end

endmodule

