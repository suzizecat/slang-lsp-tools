`define ADD(x,y) (x + y)


class my_item
int number[5];
function new();
    foreach (this.number[i]) begin
        this.number[i] = 0;
    end
endfunction;

function increment(int pos) begin
    this.number[pos] = `ADD(pos,1);
end 

endclass;

class my_obj;

my_item item[5];

function new();
    foreach (this.item[i]) begin
        this.item[i] = new();
    end
endfunction;

function obj_incr(int pos) begin
    my_item local_it[2];
    local_it[0] = new() 
    // This is a damn comment...
    local_it[pos].increment(0);
    int tst = 0;
    `ADD(tst,2);
end

endclass;

module top ();
    my_obj obj = new();
endmodule;