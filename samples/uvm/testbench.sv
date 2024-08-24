//----------------------------------------------------------------------
//  Copyright (c) 2011-2012 by Doulos Ltd.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//----------------------------------------------------------------------

// First Steps with UVM - Hello World
// See https://youtu.be/qLr8ayWM_Ww

// Author: John Aynsley, Doulos
// Date:   1-May-2012


`include "uvm_macros.svh"

package my_pkg;

  import uvm_pkg::*;

  class my_env extends uvm_env;

    `uvm_component_utils(my_env)

    int my_var;
    
    function new(string name, uvm_component parent);
      super.new(name, parent);
      this.my_var = 0;
    endfunction

    function update_var(int newval);
      this.my_var = newval;
    endfunction
 
  endclass: my_env
  
  
  class my_test extends uvm_test;
  
    `uvm_component_utils(my_test)
    
    my_env m_env;
    
    function new(string name, uvm_component parent);
      super.new(name, parent);
    endfunction
    
    function void build_phase(uvm_phase phase);
      m_env = my_env::type_id::create("m_env", this);
    endfunction
    
    task run_phase(uvm_phase phase);
      phase.raise_objection(this);
      #10;
      `uvm_info("", "Hello World", UVM_MEDIUM)
      phase.drop_objection(this);

      this.m_env.my_var = 2;
      this.m_env.update_var(3);
    endtask
     
  endclass: my_test
  
  
endpackage: my_pkg


module top;

  import uvm_pkg::*;
  import my_pkg::*;
  
  dut_if dut_if1 ();
  
  dut    dut1 ( .dif(dut_if1) );

  initial
  begin
    run_test("my_test");
  end

endmodule: top
