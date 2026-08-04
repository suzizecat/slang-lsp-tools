interface foo_if () ;
	logic net;

	modport in (
		input net
	);

	modport out (
		output net
	);
endinterface : foo_if;


module top;

	foo_if myitf();


	s1 u_s2 (.in_if(myitf));

endmodule;


module s1 (
	foo_if.in in_if
);

	s2 u_s2 (.bar_if(in_if));

endmodule;

module s2 (
	foo_if.in bar_if
);

logic test;

assign test = bar_if.net;
endmodule;
