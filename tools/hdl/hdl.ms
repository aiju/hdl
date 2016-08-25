.NH
Types and Variables
.NH 2
Simple types
.TS
r1W12 l.
\fItype\fR ←	\fItype-word\fR {\fItype-word\fR} {\fB[ \fIcomma-expr \fB]\fR}
\fItype-word\fR ←	\fBbit\fR ∨ \fBsigned\fR ∨ \fBclock\fR ∨ \fBint\fR ∨ \fBreal\fR ∨ \fBstring\fR ∨ \fBinput\fR ∨ \fBoutput\fR ∨ \fBconst\fR ∨ \fBtypedef\fR
∨	\fBwire\fR ∨ \fBreg\fR
∨	\fIdefined-type\fR
.TE
.LP
A simple type is defined by one or more type words.
Type words can appear in any order but not all combinations are a legal type.
.IP \(bu
\fBbit\fR, \fBclock\fR and \fBsigned\fR are invalid except as part of a \fBbit\fR type.
.IP \(bu
Only one of the words that declare a type (\fBint\fR, \fBstring\fR, \fBreal\fR, a defined type) may be used.
.IP \(bu
Only one of \fBwire\fR, \fBreg\fR, \fBinput\fR, \fBtypedef\fR and \fBconst\fR may be used.
.IP \(bu
Only one of \fBinput\fR, \fBoutput\fR, \fBtypedef\fR and \fBconst\fR may be used.
.LP
The \fBbit\fR type defines bitvectors.
They are 4-valued in simulation, with each bit having the possible values of \f(CW0\fR, \f(CW1\fR, \f(CWx\fR and \f(CWz\fR.
\fBbit\fR types are unsigned unless the word \fBsigned\fR is specified.
Note that \fBbit\fR is implicit and can always be elided from a list of typewords if the result is syntactically valid.
\fBbit\fR types can also be specified to be clocks, using the \fBclock\fR word.
.br
The \fBint\fR type defines signed arbitrary size integers.
It is not permitted for an implementation to restrict the range.
.br
The \fBreal\fR type defines IEEE 754 "double precision" floating point numbers.
.br
The \fBstring\fR type defines (conventionally UTF-8 encoded) strings of arbitrary length.
.br
\fBint\fR, \fBreal\fR or \fBstring\fR are for simulation and compile-time use only.
.br
A vector type can be made from any other type (including other vector types) by following the type with \fB[\ \fIsize\ \fB]\fR.
Similarly, a memory type can be defined by placing the \fB[\fR\ ...\ \fB]\fR specifier after the variable name.
Memory types are very similar to vector types and only differ in the implementation.
Unbounded bitvectors occur internal in expression; there is currently no way to declare a variable to be unbounded.
.br
The \fBconst\fR word declares a compile-time constant.
If nothing else is specified, the type is inferred from context (rather than assumed \fBbit\fR).
.br
The \fBtypedef\fR word declares a new type.
The new type can then be used as a \fIdefined-type\fR.
Note that type definitions can be shadowed by variable definitions.
.br
The meaning of \fBwire\fR and \fBreg\fR is defined below (under "Variables").
.NH 2
Enumeration types
.TS
r1W12 l.
\fItype\fR ←	{\fItype-word\fR} \fBenum { \fR{\fIenum-val \fB,\fR } [\fIenum-val \fR[\fB , \fR] ] \fB}
\fIenum-val\fR ←	\fIsymbol\fR [ \fB= \fIexpr\fR ]
.TE
\fBenum\fR declares an enumeration type.
An enumeration type consists of an underlying base type and a set of possible values.
The base type may be specified before the \fBenum\fR keyword (it may be either a \fBbit\fR or a bit vector type).
If it is not specified, a base type is chosen to cover all possible values.
.LP
Values can be specified with or without an expression.
If an expression is not specified, the previous value's expression is used, incremented by one.
The first value uses 0 if no expression is specified.
.LP
Note that an implementation need not restrict the type to only take on the defined values.
.NH 2
Struct types
.TS
r1W12 l.
\fItype\fR ←	\fBstruct { \fR{\fImember-def\fR} \fB}\fR
\fIstruct-def\fR ←	\fBstruct\fR \fIsymbol \fR[\fB(\fIargs\fB)\fR] \fR[\fB{ \fR{\fImember-def\fR} \fB}\fR]
\fImember-def\fR ←	\fIvar-def\fB \fR[ \fB= \fIexpr \fR] \fR[\fIpack-def\fR]\fB;\fR
∨	\fIpack-def\fB;\fR
∨	\fIstruct \fR[ \fB(\fIargs\fB) \fR] \fB;\fR
\fIpack-def\fR ←	\fB@@ \fIsymbol \fR[ \fB[ \fR(\fIcomma-expr\fB:\fIcomma-expr\fR) \fB] \fR]
.TE
A \fBstruct\fR defines a type composed of multiple non-overlapping subfields.
Each field has a type (which can be either simple or composite) and supports the usual variable options, which may or may not be meaningful depending on the context in which the struct is used.
For instance \fIinput\fR and \fIoutput\fR are only meaningful for a struct declared as a port and all options can be overriden when the struct is instantiated.
.LP
A \fIpack-def\fR allows more explicitly control over the packing of fields in a struct.
Assigning a \fIpack-def\fR to one or more fields declares that thes fields shall be mapped to the \fIsymbol\fR given in the \fIpack-def\fR, with the sequence \f(CW$0\fR replaced by the name of the struct variable and \f(CW$1\fR replaced by the name of the field.
If a range is given that specific range is used, otherwise fields assigned to the same symbol are mapped in increasing order.
A lone \fIpack-def\fR with no preceding field declaration applies to all following fields.
The default \fIpack-def\fR is \f(CW$0_$1\fR.
.LP
Struct members can have default values specified.
Default values are only ever used with struct literals.
.LP
A lone reference to a struct type causes that type to be \fIembedded\fR.
All fields of the embedded type can be accessed directly and the embedded struct as a whole can be accessed using the type as a field.
A \fIstruct\fR can be implicitly converted into any of its embedded types.
.LP
A struct can have arguments which use the same syntax as function arguments.
Two instances with different arguments are considered of different type.
.NH 2
Variables
.TS
r1W12 l.
\fIvar-def\fR ←	\fItype\fR [\fIclock\fR] \fIvar\fR { \fB, \fIvar \fR} [ \fB, \fR]
\fIclock\fR ←	\fB< \fR[\fIclock\fR] \fB>\fR
\fIvar\fR ←	\fIsymbol\fR { \fB[ \fIcomma-expr \fB] \fR } [ \fB= \fIexpr \fR]
.TE
.LP
With all variables there is associated a "next value" denoted by \fIsymbol\fB'\fR and known as the primed version of that variable.
.LP
Variables declared as \fBreg\fR shall be implemented as registers.
Note that assignments to the current value of a register are explicitly permitted, however the implementation may reject any code that would be impossible to implement.
.LP
Variables declared as \fBwire\fR shall have no storage associated with them.
It is illegal to assign to the primed version of a wire.
.LP
It is up to the implementation to decide whether variables declared as neither \fBwire\fR nor \fBreg\fR have any storage associated with them.
Note that it is possible to define variables which do not fit either category.
.LP
All variables must either have an associated clock defined or be declared to be \fIasynchronous\fR.
If no clock is specified and there is only one clock visible in the scope, that clock is implicitly assumed for all variables.
To mark a variable as \fIasynchronous\fR an empty clock declaration \fB<>\fR is used.
.LP
A clock declaration can be shared between multiple variable definitions using the notation \fB< \fIclock \fB> { ... \fB}\fR.
.LP
Only ports (see "modules") can use the \fBinput\fR and \fBoutput\fR type words.
.bp
.NH
Expressions
.LP
.TS
r1W12 l.
\fIprimary\fR ←	\fIlval\fR
∨	\fInumber\fR
∨	\fB( \fIcomma-expr \fB)\fR

\fIexpr\fR ←	\fIprimary\fR
∨	\fIexpr binary-op expr\fR
∨	\fIunary-op expr\fR
∨	\fIexpr \fB( \fIcomma-expr \fB)\fR
∨	\fIexpr \fB? \fIexpr \fB: \fIexpr\fR

\fIcomma-expr\fR ←	\fIexpr\fR { \fB, \fIexpr \fR}
.TE
.NH 2
L-values
.LP
.TS
r1W12 l.
\fIlval\fR ←	\fIsymbol\fR { \fB. \fIsymbol \fR}
∨	\fIlval\fB'\fR
∨	\fIlval\fB[\fIcomma-expr\fB]\fR
∨	\fIlval\fB[\fIcomma-expr\fB:\fIcomma-expr\fB]\fR
∨	\fIlval\fB[\fIcomma-expr\fB+:\fIcomma-expr\fB]\fR
∨	\fIlval\fB[\fIcomma-expr\fB-:\fIcomma-expr\fB]\fR

\fIcomma-lval\fR ←	\fIlval\fR { \fB, \fIlval \fR}
.TE
L-values are expressions that may appear both on the left-hand side of assignments as well as operands in other expressions.
L-values are constructed by starting with a symbol and then (if the symbol refers to a struct) optionally selecting a member with the \fB.\fR operator.
.LP
If an l-value has a vector type (or \fBstring\fR), the \fB[]\fR operation can be used to select one or more elements.
If only one number is specified inside the brackets, then only that element will be used.
If two numbers are specified by colons, then all elements between and including the two numbers will be selected (if the second number is lower than the first, no items are selected).
.br
\fB[\fIa\fB+:\fIb\fB]\fR is equivalent to \fB[\fIa\fR + \fIb\fR - 1\fB:\fIb\fB]\fR.
.br
\fB[\fIa\fB-:\fIb\fB]\fR is equivalent to \fB[\fIa\fB:\fIa\fR - \fIb\fR + 1\fB]\fR.
.br
If the width of a selection cannot be shown to be a constant, the type is an unbounded vector.
.LP
The comma operator concatenates two vector or string operands.
Unbounded vector cannot be concatenated.
.NH 2
Operators
.LP
The actual operation behind almost all operators follows the standard definitions used in computing.
Of note are only how the result type is to be determined.
A main difference between this language and other languages (C, Verilog) is that results are always mathematically correct (floating-point excepted); "overflow" never occurs.
This is, of course, only true for expressions by themselves; assigning the result to a finite size variable will result in truncation.
.LP
The result of an arithmetic operation is
.IP \(bu
\fBreal\fR, if either operand is \fBreal\fR; else
.IP \(bu
\fBint\fR, if both operands are \fBint\fR; else
.IP \(bu
\fBsigned bit[∞]\fR, if either operand is signed (or \fBint\fR); else
.IP \(bu
\fBbit[∞]\fR.
.bp
.TS
allbox ;
lFB lFB lFB lFB lFB
lFB s s s s
lFCW l l l l.
Symbol	Operation		Result type	Notes
Binary operators (in decreasing order of precedence)
\fIn\fR(\fIm\fR)	Replication	Si	\fBbit[\fIn\fR·\fIb\fR]\fR or \fBstring\fR
_
()	Cast
_
#	Delay	R	\fIa\fR	\fIb\fR must be a constant non-negative integer.
@	Prior value	R	\fIa\fR	\fIb\fR must be \fBbit\fR.
_
**	Exponentiation	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
_
	Unary operators
_
*	Multiplication	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
/	Division	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
%	Modulo	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
_
+	Addition	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
-	Subtraction	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
_
<<	Left shift		\fBbit[∞]\fR
>>	Logical right shift		\fBbit[∞]\fR
>>>	Arithmetic right shift		\fBbit[∞]\fR
_
&	Bitwise and		max(\fIa\fR, \fIb\fR)
_
^	Bitwise xor		max(\fIa\fR, \fIb\fR)	
_
|	Bitwise or		max(\fIa\fR, \fIb\fR)	
_
<	Less than	RS	\fBbit\fR
<=	Less or equal than	RS	\fBbit\fR
>	Greater than	RS	\fBbit\fR
>=	Greater or equal than	RS	\fBbit\fR
_
==	Equal to	RS	\fBbit\fR
!=	Not equal to	RS	\fBbit\fR
===	Exactly equal to		\fBbit\fR
!==	Not exactly equal to		\fBbit\fR
_
&&	Logical and	b	\fBbit\fR
_
||	Logical or	b	\fBbit\fR
_
?:	Ternary operator	R	max(\fIb\fR, \fIc\fR)	\fIa\fR must be \fBbit\fR.
_
,	Concatenation	Si	\fBbit[\fIa\fR+\fIb\fR]\fR or \fBstring\fR
.T&
lFB s s s s
lFCW l l l l.
Unary operators
+	Unary plus	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
-	Unary minus	R	\fBreal\fR, \fBint\fR or \fBbit[∞]\fR
~	Bitwise not		\fIa\fR
&	Reduction and		\fBbit\fR
|	Reduction or		\fBbit\fR
^	Reduction xor		\fBbit\fR
!	Logical not	b	\fBbit\fR
%	Reverse bit order	i	\fIa\fR
.TE
For bitvectors all operations from the operator table are supported.
.br
Real numbers support only the operations marked \fBR\fR.
.br
Strings support only the operations marked \fBS\fR.
.br
The operations marked \fBb\fR support only the type \fBbit\fR (but not as a vector!).
.br
The operations marked \fBi\fR do not support the type \fBint\fR or any other unsized type.
.bp
.LP
The result of a bitwise operation is the largest type that can fit either operand. More specifically it is \fBint\fR only if both arguments are \fBint\fR. Otherwise it's a bitvector of size max(\fIa\fR, \fIb\fR).
The result is signed if either operand is signed.
.LP
The \fB#\fR and \fB@\fR operators are unusual because they depend not only on the current values, but also on the history.
The \fB#\fR operator encodes a delay of a constant number of clock cycles given by the second argument.
The \fB@\fR operator returns the value of the first expression, when the second expression was last true, i.e. \fCy@z\fR is given by the value of the variable \fCx\fR which is defined by
.P1
if(z)
	x = y;
.P2
.NH 2
Literals
.TS
r1W12 l.
\fIexpr\fR ←	\fB{ \fR{ \fIliteral-expr \fB, \fR} \fR[ \fIliteral-expr \fR] \fB}
\fIliteral-expr\fR ←	\fIexpr\fR
∨	\fB[] \fIexpr\fR
∨	\fB[\fIcomma-expr\fB]\fR \fIexpr\fR
∨	\fB[\fIcomma-expr\fB:\fIcomma-expr\fB]\fR \fIexpr\fR
∨	\fB[\fIcomma-expr\fB+:\fIcomma-expr\fB]\fR \fIexpr\fR
∨	\fB[\fIcomma-expr\fB-:\fIcomma-expr\fB]\fR \fIexpr\fR
∨	\fB. \fIsymbol\fR \fIexpr\fR
.TE
Literals define a value of a vector or \fBstruct\fR type.
The type of a literal is deduced from the context in which it appears.
Literals consists of expressions with optional position indicators.
.LP
In a \fBstruct\fR literal the position indicator takes the simple form \fB. \fIsymbol\fR and defines which field is being specified.
If the position indicator is missing in a \fBstruct\fR literal, the fields are stepped through in the order in which they appear in the \fBstruct\fR definition.
Fields that are missing from a literal take on the default values from the \fBstruct\fR definition.
.LP
In a vector literal the position indicator takes on the same form as the selection operators previously discussed.
Note that if multiple elements are to be selected, the expression specified must be a vector itself.
If no position indicator is specified, the elements are stepped through in incrementing order.
The \fB[]\fR indicator is special and defines a default value to be used for all elements that are not explicitly defined.
.LP
It is worth noting that bitvectors can also be described by literals.
.NH 2
Casts
.LP
If the type of an expression does not match the type that is expected by context (e.g. assigning to a variable of a different type), an implicit cast is attempted.
Implicit casts follow the following rules
.IP \(bu
Bitvectors, \fBbit\fR, \fBint\fR and \fBreal\fR types can be freely interconverted.
.IP \(bu
\fBenum\fR can be converted to and from bitvectors, \fBbit\fR and \fBint\fR and to \fBreal\fR.
Different \fBenum\fR types may not be implicitly casted.
.IP \(bu
No other casts are allowed.
.LP
.TS
r1W12 l.
\fIexpr\fR ←	\fItype \fB( \fIexpr\fR \fB)
.TE
.LP
An explicit cast can be performed with the above notation.
It behaves like an implicit cast but also supports the following cases
.IP \(bu
\fBenum\fR can be converted to and from \fBreal\fR and other \fBenum\fR types.
.IP \(bu
\fBstring\fR can be converted to and from a bitvector.
.bp
.NH
Statements
.NH 2
Assignments
.TS
r1W12 l.
\fIstat\fR ←	\fIstat1\fB;\fR
\fIstat1\fR ←	ε
∨	\fIlval \fB= \fIcomma-expr \fR
∨	\fIlval \fIassign-op\fB= \fIcomma-expr \fR
∨	\fIlval \fB++\fR
∨	\fIlval \fB--\fR
.TE
.LP
Assignments update the value of a variable.
The exact semantics depend on the context, but generally speaking the \fIlval\fR will refer to the value of right-hand side.
.
.LP
\fIlval \fIassign-op\fB= \fIcomma-expr\fR is equivalent to \fIlval \fB= \fIlval \fIassign-op \fIcomma-expr\fR. The only difference is that \fIlval\fR is evaluated only once.
.br
\fIlval\fB++\fR is equivalent to \fIlval \fB+= \fR1.
.br
\fIlval\fB--\fR is equivalent to \fIlval \fB-= \fR1.
.NH 2
Blocks
.TS
r1W12 l.
\fIstat\fR ←	\fR[ \fIsymbol \fR] \fB{ \fR{\fIstat\fR} \fB}
∨	\fIvar-def\fB;\fR
.TE
.LP
Blocks serve to group statements and to create a new scope for variable definitions.
Blocks can be named, this allows the \fBbreak\fR and \fBcontinue\fR statements to refer to the block.
Names also allow a debugger to reference signals defined inside the block.
Note that unlike Verilog a name is not needed to define variables inside the block.
.NH 2
Control statements
.TS
r1W12 l.
\fIstat\fR ←	\fBif(\fIcomma-expr\fB) \fIstat
∨	\fBwhile(\fIcomma-expr\fB) \fIstat
∨	\fBdo \fIstat \fBwhile(\fIcomma-expr\fB);
∨	\fBfor(\fIstat1\fB; \fR[\fIcomma-expr\fR]\fB; \fIstat1\fB) \fIstat
∨	\fBbreak \fR[\fIblock\fR]\fB;\fR
∨	\fBcontinue \fR[\fIblock\fR]\fB;\fR
.TE
.LP
The control statements statements function identically to C.
The only difference is that if \fIstat\fR is block named \fIblock\fR, then \fBbreak \fIblock\fR and \fBcontinue \fIblock\fR can be used to apply \fBbreak\fR and \fBcontinue\fR to that loop specifically.
.NH 2
Switch statements
.TS
r1W12 l.
\fIstat\fR ←	\fBswitch(\fIcomma-expr\fB) { \fR{\fIstat\fR} \fB}\fR
∨	\fBswitchz(\fIcomma-expr\fB) { \fR{\fIstat\fR} \fB}\fR
∨	\fBcase \fIexpr \fR{ \fB, \fIexpr \fR} \fB:\fR
∨	\fBdefault:\fR
.TE
.LP
The \fBswitch\fR statement compares \fIcomma-expr\fR with each of the \fBcase\fR values defined inside it and jumps to the first value that matches.
Each \fBcase\fR statement has one or more expressions and they need not be constant.
If no value matches, the \fBdefault\fR statement is jumped to.
If there is no \fBdefault\fR statement, execution proceeds after the \fBswitch\fR body.
Execution jumps out of the \fBswitch\fR statement at the next \fBcase\fR statement.
.LP
\fBswitchz\fR functions like \fBswitch\fR but \fBz\fR bits in the \fBcase\fR statements are treated as wildcards.
.NH
Modules
.TS
r1W12 l.
\fImodule-def\fR ←	\fBmodule \fImodule \fR[ \fB( \fR{\fIparam\fB,\fR} [\fIparam\fR] \fB) \fR] { \fR{ \fIdef \fR} \fB}
\fIparam\fR ←	[\fItype\fR] \fIsymbol\fR {\fB[\fIcomma-expr\fB]\fR}
\fIdef\fR ←	\fIstat\fR ⋁ \fImodule-def\fR ∨ ...
.TE
.LP
Modules serve to group logic.
A module presents an interface defined by a set of ports.
Ports are defined (and function like) top level variables in the module with a direction (\fBinput\fR or \fBoutput\fR) defined for them.
.LP
Parameters are compile-time values that can be used to adapt a module for different contexts it might be used in.
.LP
During one cycle of execution of a module all the definitions are executed repeatedly until the results converge.
Finally, all assignments to primed variables are copied over to become the main variables.
.LP
It is illegal
[TODO: Make sense.]
.IP \(bu
for signals to have multiple drivers.
.IP \(bu
for signals to be cyclically dependent on each other.
.LP
.NH 2
Initial statements
.TS
r1W12 l.
\fIdef\fR ←	\fBinitial( \fR{ \fItrigger \fB, \fR} [ \fItrigger \fR] \fB) \fIstat
\fItrigger\fR ←	\fBdefault\fR ∨ \fIexpr\fR
.TE
\fBInitial\fR statements allow the values of registers to be overridden.
They can be triggered by both synchronous and asynchronous signals, as well as the \fBdefault\fR keyword which indicates that the block should be executed on startup.
In an \fBinitial\fR statement values should be assigned to unprimed variables.
Initial blocks are run before all other blocks and in case of synchronous signals they depend on the value during the \fIprevious\fR cycle.
.NH 2
Finite state machines (FSM)
.TS
r1W12 l.
\fIfsm\fR ←	\fBfsm \fIsymbol \fR[\fB< \fIexpr \fR[\fB, \fIexpr\fR] \fB>\fR] \fB{ \fR{\fIstat\fR} \fB}\fR
\fIstat\fR ←	\fBgoto \fR[\fIstate\fR]\fB;\fR
∨	\fR[\fIstate\fR]\fB:\fR
∨	\fBdefault:\fR
.TE
.LP
An \fBfsm\fR statement defines a signal \fIsymbol\fR as an \fBenum\fR.
Each \fIstate\fR declared in the body defines one of the possible values.
A lone \fB:\fR declares another state, incrementing a number at the end of the last state, or appending \f(CW0\fR if there is no number.
The \fBdefault\fR keyword declares no state.
.LP
Control statements can be used in a fsm; by including state definitions in the body their execution can span several states.
.LP
During each cycle of execution the statements from first the \fBdefault\fR case and then the current state are executed.
If a \fBgoto\fR statement is executed, execution is halted and the next state is set to the label given.
If no label is given, the current one is assumed.
.LP
It is legal to reference the \fIsymbol\fR variable.
Assigning to \fIsymbol\fR or \fIsymbol\fB'\fR is illegal.
.NH 2
Pipelines
.TS
r1W12 l.
\fIpipe\fR ←	\fBpipeline \fIsymbol \fR[ \fB< \fIsymbol \fR[ \fB, \fIsymbol \fR] \fB> \fR] \fB{ \fR{\fIstat\fR} \fB} \fR
\fIstat\fR ←	\fR[\fIstage\fR]\fB:\fR
.TE
.LP
Pipelines are defined with similar syntax to finite state machines, although the semantics are quite different.
Variables defined inside the pipeline block are duplicated in all stages and variables that are not explicitly assigned to in one stage take on the value they had in the previous stage, with a one cycle delay.
.LP
Assignments to primed versions of pipeline variables defines their value in the next stage without affecting their value in the current one.
.LP
The first symbol in angled brackets specifies a clock for the pipeline block.
The second symbol defines a "go" variable which is a signal of type \fBbit\fR[\fIn\fR] where \fIn\fR is the number of pipeline stages.
If the "go" variable is defined, pipeline stage \fIi\fR will only move ahead if \fIgo\fR[\fIi\fR] is set in a given cycle.
.bp
.NH
Lexical conventions
.NH 2
Symbols
.LP
The character set for symbols currently consists of
.IP \(bu
Letters
.IP \(bu
Numbers
.IP \(bu
The underscore symbol \fB_\fR
.LP
Note that a symbol may not start with a digit.
.LP
The following words are keywords and must not be used as symbols.
.TS
lFB lFB lFB lFB lFB lFB .
bit	break	case	clock	const	continue
default	do	else	enum	for	fsm
goto	if	initial	input	int	module
output	real	reg	signed	string	struct
switch	switchz	typedef	while	wire
.TE
.NH 2
Numbers
.LP
A numerical literal has the format
.QP
[[\fIlength\fR] \fB'\fR [\fBs\fR] \fIradix\fR] \fIvalue\fR
.QE
.LP
Note that all characters in a numerical literal must be consecutive with no spaces allowed.
\fB_\fR is however allowed to be added anywhere and has no effect.
A number must not begin in \fB_\fR (lest it be mistaken for a symbol).
No parts of a number are case sensitive.
.LP
\fIradix\fR must be one of \fBd\fR (decimal), \fBh\fR (hexadecimal), \fBo\fR (octal) or \fBb\fR (binary).
\fBs\fR can be used to specify a signed number.
.LP
\fIlength\fR specifies the number of bits in the final constant. If it is not specified, the number is unbounded.
.LP
The valid digits in \fIvalue\fR are a subset of 0123456789abcdefzx? (\fB?\fR is equivalent to \fBz\fR), depending on the base.
.IP \(bu
The digits ≥ the base value are excluded.
.IP \(bu
\fBz\fR and \fBx\fR are not permitted for decimal numbers.
.LP
If the first specified digit is \fBz\fR or \fBx\fR, this digit will be repeated to fill the length of the number, if necessary.
.NH 2
Comments
.LP
\fB//\fR will cause the rest of the line to be ignored.
.LP
\fB/*\fR will cause everything until \fB*/\fR to be ignored. It may not be nested.
