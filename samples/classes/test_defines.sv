// Test comment
`define FOO(x) x+1
`define DECLARE(x, y) int ``x = y

class bar;
    function new()
    // Test
        `DECLARE(yay,`FOO(2 + 3));
        int i = 0;
        `FOO(i);
    endfunction;
endclass