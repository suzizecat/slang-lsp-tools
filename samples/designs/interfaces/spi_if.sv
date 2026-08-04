interface if_spi #(parameter int N_SLAVE);
    logic clk;
    logic [N_SLAVE-1:0] csn;
    logic miso;
    logic mosi;

    modport slave (
        input clk,
        input csn,
        input mosi,
        output miso
    );

    modport master (
        output clk,
        output csn,
        output mosi,
        input miso
    );
endinterface //if_spi