.NH
Types and Variables
.NH 2
Simple types
.LP
[TODO: Update grammar to match code]
.TS
r1W12 l.
\fItype\fR ←	\fBbit\fR [\fBsigned\fR] {\fB[ \fIcomma-expr \fB]\fR}
∨	\fBclock\fR
∨	\fBinteger\fR
∨	\fBreal\fR
∨	\fBstring\fR
∨	\fBenum { \fR{\fIenum-val \fB,\fR } [\fIenum-val \fR[\fB , \fR] ] \fB}

\fItype-def\fR ←	\fBtypedef \fItype \fIsymbol

\fIenum-val\fR ←	\fIsymbol\fR [ \fB= \fIexpr\fR ]
.TE
.LP
The \fBbit\fR type defines signed and unsigned bitvectors.
They are 4-valued in simulation, with each bit having the possible values of \f(CW0\fR, \f(CW1\fR, \f(CWx\fR and \f(CWz\fR.
.br
The \fBclock\fR type defines a single bit signal used as a clock.
When not used as a clock it is treated identical to an unsigned 1 bit wide \fBbit\fR type.
.br
The \fBinteger\fR type defines signed arbitrary size integers.
It is not permitted for an implementation to restrict the range.
.br
The \fBreal\fR type defines IEEE 754 "double precision" floating point numbers.
.br
The \fBstring\fR type defines strings of arbitrary length in UTF-8.
.br
\fBinteger\fR, \fBreal\fR or \fBstring\fR are for simulation and compile-time use only.
.br
\fBenum\fR declares an enumeration type.
.NH 2
Struct types
.TS
r1W12 l.
\fItype\fR ←	\fBstruct { \fR{\fImember-def\fR} \fB}\fR
\fIstruct-def\fR ←	\fBstruct\fR \fIsymbol \fR[\fB(\fIargs\fB)\fR] \fR[\fB{ \fR{\fImember-def\fR} \fB}\fR]
\fImember-def\fR ←	[\fBinput\fR∨\fBoutput\fR] \fIvar-def\fB \fR[\fIpack-def\fR]\fB;\fR
∨	\fIpack-def\fB;\fR
∨	\fIstruct \fR[ \fB(\fIargs\fB) \fR] \fB;\fR
\fIpack-def\fR ←	\fB@@ \fIsymbol \fR[ \fB[ \fR(\fIcomma-expr\fB:\fIcomma-expr\fR) \fB] \fR]
.TE
A \fBstruct\fR defines a type composed of multiple non-overlapping subfields.
Each field has a type (which can be either simple or composite) and supports the usual variable options, which may or may not be meaningful depending on the context in which the struct is used.
For instance \fIinput\fR and \fIoutput\fR are only meaningful for a struct declared as a port and all options can be overriden when the struct is instantiated.
.PP
A \fIpack-def\fR allows more explicitly control over the packing of fields in a struct.
Assigning a \fIpack-def\fR to one or more fields declares that thes fields shall be mapped to the \fIsymbol\fR given in the \fIpack-def\fR, with the sequence \f(CW$0\fR replaced by the name of the struct variable and \f(CW$1\fR replaced by the name of the field.
If a range is given that specific range is used, otherwise fields assigned to the same symbol are mapped in increasing order.
A lone \fIpack-def\fR with no preceding field declaration applies to all following fields.
The default \fIpack-def\fR is \f(CW$0_$1\fR.
.PP
A lone reference to a struct type causes that type to be \fIembedded\fR.
All fields of the embedded type can be accessed directly and the embedded struct as a whole can be accessed using the type as a field.
A \fIstruct\fR can be implicitly converted into any of its embedded types.
.PP
A struct can have arguments which use the same syntax as function arguments.
Two instances with different arguments are considered of different type.
.NH 2
Variables
.TS
r1W12 l.
\fIvar-def\fR ←	[\fBwire\fR∨\fBreg\fR] \fItype\fR [\fB< \fR[\fIclock\fR] \fB>\fR] \fIvar\fR { \fB, \fIvar \fR} [ \fB, \fR]
\fIvar\fR ←	\fIsymbol\fR [ \fB= \fIexpr \fR]
.TE
.LP
Variables declared as \fBreg\fR shall be implemented as registers.
Note that assignments to the current value of a register are explicitly permitted, however the implementation may reject any code that would be impossible to implement.
.LP
Variables declared as \fBwire\fR shall have no storage associated with them.
It is illegal to assign to the primed version of a wire.
.LP
It is up to the implementation to decide whether variables declared as neither \fBwire\fR nor \fBreg\fR have any storage associated with them.
Note that it is possible to define variables which would be legal as neither.
.LP
All variables must either have an associated clock defined or be declared to be \fIasynchronous\fR.
If no clock is specified and there is only one clock visible in the scope, that clock is implicitly assumed for all variables.
To mark a variable as \fIasynchronous\fR an empty clock declaration \fB<>\fR is used.
.bp
.NH
Expressions
.PP
.TS
r1W12 l.
\fIlval\fR ←	\fIsymbol\fR { \fB. \fIsymbol \fR}
∨	\fIlval\fB'\fR
∨	\fIlval\fB[\fIcomma-expr\fB]\fR
∨	\fIlval\fB[\fIcomma-expr\fB:\fIcomma-expr\fB\fR]
∨	\fIlval\fB[\fIcomma-expr\fB+:\fIcomma-expr\fB\fR]
∨	\fIlval\fB[\fIcomma-expr\fB-:\fIcomma-expr\fB\fR]

\fIcomma-lval\fR ←	\fIlval\fR { \fB, \fIlval \fR}

\fIprimary\fR ←	\fIlval\fR
∨	\fInumber\fR
∨	\fB( \fIcomma-expr \fB)\fR

\fIexpr\fR ←	\fIprimary\fR
∨	\fIexpr binary-op expr\fR
∨	\fIunary-op expr\fR

\fIcomma-expr\fR ←	\fIexpr\fR { \fB, \fIexpr \fR}
.TE
.bp
.TS
allbox ;
lFCW l l.
#	Delay	R
@	Prior value	R
_
**	Exponentiation	R
_
*	Multiplication	R
/	Division	R
%	Modulo	R
_
+	Addition	R
-	Subtraction	R
_
<<	Left shift
>>	Logical right shift
>>>	Arithmetic right shift
_
&	Bitwise and
_
^	Bitwise xor	
_
|	Bitwise or
_
<	Less than	RS
<=	Less or equal than	RS
>	Greater than	RS
>=	Greater or equal than	RS
_
==	Equal to	RS
!=	Not equal to	RS
===	Exactly equal to
!==	Not exactly equal to
_
&&	Logical and	b
_
||	Logical or	b
_
?:	Ternary operator	R
_
\fIn\fR(\fIm\fR)	Replication	Si
_
,	Concatenation	Si
.TE
.TS
allbox;
lFCW l l.
+	Unary plus	R
-	Unary minus	R
~	Bitwise not
&	Reduction and
|	Reduction or
^	Reduction xor
!	Logical not	b
.TE
For bitvectors all operations from the operator table are supported.
.br
Real numbers support only the operations marked \fBR\fR.
.br
Strings support only the operations marked \fBS\fR.
.br
The operations marked \fBb\fR support only the type \fBbit\fR.
.br
The operations marked \fBi\fR do not support the type \fBinteger\fR or any other unsized type.
.bp
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
.PP
An \fBfsm\fR statement defines a signal \fIsymbol\fR as an \fBenum\fR.
Each \fIstate\fR declared in the body defines one of the possible values.
A lone \fB:\fR declares another state, incrementing a number at the end of the last state, or appending \f(CW0\fR if there is no number.
The \fBdefault\fR keyword declares no state.
.PP
During each cycle of execution the statements from first the \fBdefault\fR case and then the current state are executed.
If a \fBgoto\fR statement is executed, execution is halted and the next state is set to the label given.
If no label is given, the current one is assumed.
.PP
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
for two signals to have multiple drivers.
.IP \(bu
for signals to be cyclically dependent on each other.
.LP
\fBInitial\fR statements allow the values of registers to be overridden.
They can be triggered by both synchronous and asynchronous signals, as well as the \fBdefault\fR keyword which indicates that the block should be executed on startup.
In an \fBinitial\fR statement values should be assigned to unprimed variables.
Initial blocks are run before all other blocks and in case of synchronous signals they use the value from the \fIlast\fR cycle.
.NH
Future ideas
.IP
\(bu
Doing element-wise arithmetic on arrays
.IP
\(bu
Fixed-point arithmetic
.IP
\(bu
Operator overloading.
