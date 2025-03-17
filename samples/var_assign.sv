module a ();
typedef logic [10:2] my_type_t;
my_type_t  custom_type;

    logic a, b;

// Here, quite a bit of stuff
logic c,d /*test*/,etoto,//Foireux !
f;

logic g,h;
logic [1:0] i;

    assign a = b;
     assign c = d,
    etoto = f;
    assign {g, h} = i;

    always_ff @( posedge i_clk or negedge i_rst_n ) begin : p_seq_
        if (~i_rst_n) begin
            i <= 2'b0;
        end else begin
                    i <=    ~i;
        end
    end

endmodule