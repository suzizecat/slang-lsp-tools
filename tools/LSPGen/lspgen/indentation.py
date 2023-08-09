class IndentHandler:
	def __init__(self, base_indent = "  ", base_level = 0):
		self.level = base_level
		self.idt = base_indent
		self.tab_eq_size = 4

	def __str__(self):
		return self.idt*self.level

	def __len__(self):
		return len(str(self)) + ((self.tab_eq_size-1) * str(self).count("\t"))

	def add_indent_level(self, ammount = 1):
		self.level += ammount

	def sub_indent_level(self, ammount = 1):
		self.level -= ammount

	def __add__(self, other):
		if isinstance(other,int) :
			return IndentHandler(self.idt,self.level + other)
		else :
			raise TypeError("Shall be int")

	def __iadd__(self, other):
		if not isinstance(other,int) :
			raise TypeError("Shall be int")
		self.level += other

	def __sub__(self, other):
		if isinstance(other,int) :
			return self + (-other)
		else :
			raise TypeError("Shall be int")

	def __isub__(self, other):
		if not isinstance(other,int) :
			raise TypeError("Shall be int")
		self.level -= other