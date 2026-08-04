interface foo_if () ;
	logic net;

	modport in (
		input net
	);

	modport out (
		output net
	);
endinterface : foo_if;
