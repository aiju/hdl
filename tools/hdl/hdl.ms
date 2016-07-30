.NH
Types and Variables
.NH 2
Simple types
.TS
r1W12 l.
\fItype\fR ←	\fItype-word\fR {\fItype-word\fR} {\fB[ \fIcomma-expr \fB]\fR}
\fItype-word\fR ←	\fBbit\fR ∨ \fBsigned\fR ∨ \fBclock\fR ∨ \fBinteger\fR ∨ \fBreal\fR ∨ \fBstring\fR ∨ \fBinput\fR ∨ \fBoutput\fR ∨ \fBconst\fR ∨ \fBtypedef\fR
∨	\fBwire\fR ∨ \fBreg\fR
∨	\fIdefined-type\fR
.TE
.LP
A simple type is defined by one or more type words.
Type words can appear in any order but not all combinations are a legal type.
.IP \(bu
\fBbit\fR, \fBclock\fR and \fBsigned\fR are invalid except as part of a \fBbit\fR type.
.IP \(bu
Only one of the words that declare a type (\fBinteger\fR, \fBstring\fR, \fBreal\fR, a defined type) may be used.
.IP \(bu
Only one of \fBwire\fR, \fBreg\fR, \fBtypedef\fR and \fBconst\fR may be used.
.IP \(bu
Only one of \fBinput\fR, \fBoutput\fR, \fBtypedef\fR and \fBconst\fR may be used.
.LP
The \fBbit\fR type defines bitvectors.
They are 4-valued in simulation, with each bit having the possible values of \f(CW0\fR, \f(CW1\fR, \f(CWx\fR and \f(CWz\fR.
\fBbit\fR types are unsigned unless the word \fBsigned\fR is specified.
Note that \fBbit\fR is implicit and can always be elided from a list of typewords if the result is syntactically valid.
\fBbit\fR types can also be specified to be clocks, using the \fBclock\fR word.
.br
The \fBinteger\fR type defines signed arbitrary size integers.
It is not permitted for an implementation to restrict the range.
.br
The \fBreal\fR type defines IEEE 754 "double precision" floating point numbers.
.br
The \fBstring\fR type defines (conventionally UTF-8 encoded) strings of arbitrary length.
.br
\fBinteger\fR, \fBreal\fR or \fBstring\fR are for simulation and compile-time use only.
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
\fIvar-def\fR ←	\fItype\fR [\fB< \fR[\fIclock\fR] \fB>\fR] \fIvar\fR { \fB, \fIvar \fR} [ \fB, \fR]
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
\fBinteger\fR, if both operands are \fBinteger\fR; else
.IP \(bu
\fBsigned bit[∞]\fR, if either operand is signed (or \fBinteger\fR); else
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
#	Delay	R	\fIa\fR	\fIb\fR must be a constant non-negative integer.
@	Prior value	R	\fIa\fR	\fIb\fR must be \fBbit\fR.
_
**	Exponentiation	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
_
*	Multiplication	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
/	Division	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
%	Modulo	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
_
+	Addition	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
-	Subtraction	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
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
\fIn\fR(\fIm\fR)	Replication	Si	\fBbit[\fIn\fR·\fIb\fR]\fR or \fBstring\fR
_
,	Concatenation	Si	\fBbit[\fIa\fR+\fIb\fR]\fR or \fBstring\fR
.T&
lFB s s s s
lFCW l l l l.
Unary operators
+	Unary plus	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
-	Unary minus	R	\fBreal\fR, \fBinteger\fR or \fBbit[∞]\fR
~	Bitwise not		\fIa\fR
&	Reduction and		\fBbit\fR
|	Reduction or		\fBbit\fR
^	Reduction xor		\fBbit\fR
!	Logical not	b	\fBbit\fR
.TE
For bitvectors all operations from the operator table are supported.
.br
Real numbers support only the operations marked \fBR\fR.
.br
Strings support only the operations marked \fBS\fR.
.br
The operations marked \fBb\fR support only the type \fBbit\fR (but not as a vector!).
.br
The operations marked \fBi\fR do not support the type \fBinteger\fR or any other unsized type.
.bp
.LP
The result of a bitwise operation is the largest type that can fit either operand. More specifically it is \fBinteger\fR only if both arguments are \fBinteger\fR. Otherwise it's a bitvector of size max(\fIa\fR, \fIb\fR).
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
.NH
Statements
.TS
r1W12 l.
\fIstat1\fR ←	ε
∨	\fIlval \fB= \fIcomma-expr \fR
∨	\fIlval \fIassign-op\fB= \fIcomma-expr \fR
∨	\fIlval \fB++\fR
∨	\fIrval \fB--\fR

\fIstat\fR ←	\fIstat1\fB;\fR
∨	\fBif(\fIcomma-expr\fB) \fIstat
∨	\fBwhile(\fIcomma-expr\fB) \fIstat
∨	\fBdo \fIstat \fBwhile(\fIcomma-expr\fB);
∨	\fBfor(\fIstat1\fB; \fR[\fIcomma-expr\fR]\fB; \fIstat1\fB) \fIstat
∨	\fR[ \fIsymbol \fR] \fB{ \fR{\fIstat\fR} \fB}
∨	\fBbreak \fR[\fIblock\fR]\fB;\fR
∨	\fBcontinue \fR[\fIblock\fR]\fB;\fR
∨	\fIvar-def\fB;\fR
∨	\fBswitch(\fIcomma-expr\fB) { \fR{\fIstat\fR} \fB}\fR
∨	\fBcase \fIexpr \fR{ \fB, \fIexpr \fR} \fB:\fR
.TE
.NH
Finite state machines (FSM)
.TS
r1W12 l.
\fIfsm\fR ←	\fBfsm \fIsymbol \fR[\fB< \fIexpr \fR[\fB, \fIexpr\fR] \fB>\fR] \fB{ \fR{\fBstat\fR} \fB}\fR

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
During each cycle of execution the statements from first the \fBdefault\fR case and then the current state are executed.
If a \fBgoto\fR statement is executed, execution is halted and the next state is set to the label given.
If no label is given, the current one is assumed.
.LP
It is legal to reference the \fIsymbol\fR variable.
Assigning to \fIsymbol\fR or \fIsymbol\fB'\fR is illegal.
.bp
.NH
Modules
.TS
r1W12 l.
\fImodule-def\fR ←	\fBmodule \fImodule \fB( \fR{\fIport\fB,\fR} [\fIport\fR] \fB) { \fR{ \fIdef \fR} \fB}

\fIdef\fR ←	\fIstat\fR
∨	\fIfsm\fR
∨	\fBinitial( \fR{ \fItrigger \fB, \fR} [ \fItrigger \fR] \fB) \fIstat

\fItrigger\fR ←	\fBdefault\fR ∨ \fIexpr\fR
.TE
.LP
During one cycle of execution all the definitions of a module are executed repeatedly until the results converge.
Finally, all assignments to primed variables are copied over to become the main variables.
.LP
It is illegal
[TODO: Make sense.]
.IP \(bu
for signals to have multiple drivers.
.IP \(bu
for signals to be cyclically dependent on each other.
.LP
\fBInitial\fR statements allow the values of registers to be overridden.
They can be triggered by both synchronous and asynchronous signals, as well as the \fBdefault\fR keyword which indicates that the block should be executed on startup.
In an \fBinitial\fR statement values should be assigned to unprimed variables.
Initial blocks are run before all other blocks and in case of synchronous signals they use the value from the \fIlast\fR cycle.
