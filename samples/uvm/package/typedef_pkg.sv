
`include "typedef_include.svh"
package td_pkg;

typedef enum
{
  UVM_NONE   = 0,
  UVM_LOW    = 100,
  UVM_MEDIUM = 200,
  UVM_HIGH   = 300,
  UVM_FULL   = 400,
  UVM_DEBUG  = 500
} uvm_verbosity;

	typedef struct { bit [3:0]   signal_id;
                     bit         active;
                     bit [1:0]   timeout;
                   } e_sig_param;

    // Create function and task defintions that can be reused
    // Note that it will be a 'static' method if the keyword 'automatic' is not used
	function int calc_parity ();
      $display("Toto"); //`PRINT("Called !");
  endfunction


endpackage