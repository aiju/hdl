`default_nettype none

module j11int(
	input wire clk,
	
	inout wire [15:0] j11f,
	output reg j11clk,
	output wire j11dmr,
	output reg j11cont,
	output reg j11dv,
	output wire j11miss,
	input wire j11ale,
	input wire j11strb,
	input wire j11sctl,
	input wire j11map,
	output wire j11abortout,
	output reg [3:0] j11dsel,

	output reg busreq,
	output reg buswr,
	output reg busgp,
	output reg [21:0] busaddr,
	output reg [15:0] buswdata,
	input wire busack,
	input wire [15:0] busrdata,
	
	input wire j11init,
	input wire j11halt,
	input wire j11parity,
	input wire j11event,
	input wire [3:0] j11irq,
	input wire j11pwrf,
	input wire j11fpe,
	
	output wire [4:0] j11state,
	output wire [15:0] j11fout0
);

	localparam NONE = 4'b1100;
	localparam INHI = 4'b0100;
	localparam INLO = 4'b1000;
	localparam OUTHI = 4'b1110;
	localparam OUTLO = 4'b1101;

	assign j11abortout = 0;
	assign j11dmr = 1;
	assign j11miss = 0;

	wire ale, strb, sctl, map;
	reg strb0;
	sync #(.INIT(1))
		syncale(clk, j11ale, ale),
		syncstrb(clk, j11strb, strb),
		syncsctl(clk, j11sctl, sctl),
		syncmaps(clk, j11map, map);
	
	reg [3:0] ctr = 0;
	initial j11clk = 1'b0;
	always @(posedge clk)
		if(ctr == 4) begin
			j11clk <= !j11clk;
			ctr <= 0;
		end else
			ctr <= ctr + 1;

	localparam IDLE = 0;
	localparam WAITALE = 1;
	localparam FETCHADDR0 = 2;
	localparam FETCHADDR1 = 3;
	localparam FETCHADDR2 = 4;
	localparam FETCHADDR3 = 5;
	localparam FETCHADDR4 = 6;
	localparam RDREQ = 7;
	localparam RDOUT0 = 8;
	localparam RDOUT1 = 9;
	localparam RDOUT2 = 10;
	localparam RDOUT3 = 11;
	localparam RDEND0 = 12;
	localparam RDEND1 = 13;
	localparam WRWAIT = 14;
	localparam WRREQ = 15;
	localparam WREND = 16;
	localparam IRQACK = 17;
	localparam OUTHI0 = 18;
	localparam OUTHI1 = 19;
	localparam OUTHI2 = 20;
	localparam OUTHI3 = 21;
	localparam INIT = 22;

	reg [4:0] state = INIT;
	reg [4:0] state_;
	assign j11state = state;
	
	reg [3:0] aio;
	reg [1:0] bs;
	reg abort;
	reg [15:0] fout;
	reg fdrive;
	
	assign j11f = fdrive ? fout : 16'bz;
	wire [15:0] hi = {6'b0, j11parity, j11event, j11fpe, j11init, j11halt, j11pwrf, j11irq};
	reg [15:0] hi0 = 16'b0;
	
	always @(posedge clk) begin
		state <= state_;
		strb0 <= strb;
		
		if(busack)
			fout <= busrdata;
		if(state_ == OUTHI0) begin
			fout <= hi;
			hi0 <= hi;
		end
	end
	
	always @* begin
		state_ = state;
	
		case(state)
		IDLE: begin
			if(hi != hi0)
				state_ = OUTHI0;
			if(strb && !strb0)
				state_ = WAITALE;
		end
		WAITALE:
			if(!ale)
				state_ = FETCHADDR0;
		FETCHADDR0:
			state_ = FETCHADDR1;
		FETCHADDR1:
			state_ = FETCHADDR2;
		FETCHADDR2:
			state_ = FETCHADDR3;
		FETCHADDR3:
			state_ = FETCHADDR4;
		FETCHADDR4: begin
			casez(aio)
			4'b1111:
				state_ = WREND;
			4'b1101:
				state_ = IRQACK;
			4'b1zzz:
				state_ = 0&&!abort ? RDEND1 : RDREQ;
			default:
				state_ = 0&&!abort ? WREND : WRWAIT;
			endcase
		end
		RDREQ:
			if(busack)
				state_ = RDOUT0;
		RDOUT0:
			state_ = RDOUT1;
		RDOUT1:
			state_ = RDOUT2;
		RDOUT2:
			state_ = RDOUT3;
		RDOUT3:
			state_ = RDEND0;
		RDEND0:
			if(!sctl)
				state_ = RDEND1;
		RDEND1:
			if(sctl)
				if(hi != hi0)
					state_ = OUTHI0;
				else
					state_ = IDLE;
		WRWAIT:
			if(!sctl)
				state_ = WRREQ;
		WRREQ:
			if(busack)
				state_ = WREND;
		WREND:
			if(sctl)
				if(hi != hi0)
					state_ = OUTHI0;
				else
					state_ = IDLE;
		IRQACK:
			if(sctl)
				state_ = IDLE;
		OUTHI0:
			state_ = OUTHI1;
		OUTHI1:
			state_ = OUTHI2;
		OUTHI2:
			state_ = OUTHI3;
		OUTHI3:
			state_ = j11init == 1'b0 ? INIT : IDLE;
		INIT:
			if(hi != hi0)
				state_ = OUTHI0;
		endcase
	end
	
	initial busreq = 1'b0;
	always @(posedge clk) begin
		fdrive <= 1'b0;
		j11dsel <= NONE;
		j11dv <= 1'b0;
		j11cont <= 1'b1;
		busreq <= 1'b0;
		buswr <= 1'bx;
		
		case(state_)
		FETCHADDR0, FETCHADDR1:
			j11dsel <= INHI;
		FETCHADDR2: begin
			j11dsel <= INLO;
			busaddr[21:16] <= j11f[5:0];
			aio <= j11f[11:8];
			busgp <= j11f[11:8] == 4'b1110 || j11f[11:8] == 4'b0101;
			bs <= j11f[7:6];
			abort <= j11f[13];
		end
		FETCHADDR3:
			j11dsel <= INLO;
		FETCHADDR4: begin
			j11dsel <= INLO;
			busaddr[15:0] <= j11f;
		end
		
		RDREQ: begin
			busreq <= state != RDREQ;
			buswr <= 1'b0;
		end
		RDOUT0, RDOUT1, RDOUT2, RDOUT3: begin
			fdrive <= 1'b1;
			if(state_ == RDOUT1 || state_ == RDOUT2)
				j11dsel <= OUTLO;
		end
		RDEND1: begin
			j11dv <= 1'b1;
			j11cont <= 1'b0;
		end

		WRWAIT:
			j11dsel <= INLO;
		WRREQ: begin
			busreq <= state != WRREQ;
			buswr <= 1'b1;
			buswdata <= j11f;
		end
		WREND:
			j11cont <= 1'b0;
		OUTHI0, OUTHI1, OUTHI2, OUTHI3: begin
			fdrive <= 1'b1;
			if(state_ == OUTHI1 || state_ == OUTHI2)
				j11dsel <= OUTHI;
		end
		endcase
	end
	
	assign j11fout0 = fout;

endmodule