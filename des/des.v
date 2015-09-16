`default_nettype none

module des(
	input wire clk,
	input wire [63:0] id,
	input wire [63:0] key,
	input wire invalid,
	output reg [63:0] od,
	output reg outvalid
);

	reg [31:0] l[0:16];
	reg [31:0] r[0:16];
	reg [31:0] x[0:16];
	reg [27:0] kl[0:16], kr[0:16];
	reg [16:0] valid;
	reg [3:0] sbox0[0:63], sbox1[0:63], sbox2[0:63], sbox3[0:63], sbox4[0:63], sbox5[0:63], sbox6[0:63], sbox7[0:63];
	
	always @(posedge clk) begin
		l[0] <= {
			id[ 6], id[14], id[22], id[30], id[38], id[46], id[54], id[62],
			id[ 4], id[12], id[20], id[28], id[36], id[44], id[52], id[60],
			id[ 2], id[10], id[18], id[26], id[34], id[42], id[50], id[58],
			id[ 0], id[ 8], id[16], id[24], id[32], id[40], id[48], id[56]
		};
		r[0] <= {
			id[ 7], id[15], id[23], id[31], id[39], id[47], id[55], id[63],
			id[ 5], id[13], id[21], id[29], id[37], id[45], id[53], id[61],
			id[ 3], id[11], id[19], id[27], id[35], id[43], id[51], id[59],
			id[ 1], id[ 9], id[17], id[25], id[33], id[41], id[49], id[57]
		};
		x[0] <= 32'h0;
		kl[0] <= {
			key[ 7], key[15], key[23], key[31], key[39], key[47], key[55],
			key[63], key[ 6], key[14], key[22], key[30], key[38], key[46],
			key[54], key[62], key[ 5], key[13], key[21], key[29], key[37],
			key[45], key[53], key[61], key[ 4], key[12], key[20], key[28]
		};
		kr[0] <= {
			key[ 1], key[ 9], key[17], key[25], key[33], key[41], key[49],
			key[57], key[ 2], key[10], key[18], key[26], key[34], key[42],
			key[50], key[58], key[ 3], key[11], key[19], key[27], key[35],
			key[43], key[51], key[59], key[36], key[44], key[52], key[60]
		};
		valid[0] <= invalid;
	end
	
	wire [63:0] rout = {r[16] ^ x[16], l[16]};
	always @(posedge clk) begin
		od <= {
			rout[24], rout[56], rout[16], rout[48], rout[ 8], rout[40], rout[ 0], rout[32],
			rout[25], rout[57], rout[17], rout[49], rout[ 9], rout[41], rout[ 1], rout[33],
			rout[26], rout[58], rout[18], rout[50], rout[10], rout[42], rout[ 2], rout[34],
			rout[27], rout[59], rout[19], rout[51], rout[11], rout[43], rout[ 3], rout[35],
			rout[28], rout[60], rout[20], rout[52], rout[12], rout[44], rout[ 4], rout[36],
			rout[29], rout[61], rout[21], rout[53], rout[13], rout[45], rout[ 5], rout[37],
			rout[30], rout[62], rout[22], rout[54], rout[14], rout[46], rout[ 6], rout[38],
			rout[31], rout[63], rout[23], rout[55], rout[15], rout[47], rout[ 7], rout[39]
		};
		outvalid <= valid[16];
	end
	
	genvar i;
	
	generate
		for(i = 0; i < 16; i = i + 1) begin
			wire [55:0] rkey = {kl[i+1], kr[i+1]};
			wire [47:0] subkey = {
				rkey[42], rkey[39], rkey[45], rkey[32], rkey[55], rkey[51], rkey[53],
				rkey[28], rkey[41], rkey[50], rkey[35], rkey[46], rkey[33], rkey[37],
				rkey[44], rkey[52], rkey[30], rkey[48], rkey[40], rkey[49], rkey[29],
				rkey[36], rkey[43], rkey[54], rkey[15], rkey[ 4], rkey[25], rkey[19],
				rkey[ 9], rkey[ 1], rkey[26], rkey[16], rkey[ 5], rkey[11], rkey[23],
				rkey[ 8], rkey[12], rkey[ 7], rkey[17], rkey[ 0], rkey[22], rkey[ 3],
				rkey[10], rkey[14], rkey[ 6], rkey[20], rkey[27], rkey[24]
			};
			wire [31:0] rid = r[i] ^ x[i];
			wire [47:0] idex = {rid[0], rid[31:27], rid[28:23], rid[24:19], rid[20:15], rid[16:11], rid[12:7], rid[8:3], rid[4:0], rid[31]};
			wire [47:0] idxor = idex ^ subkey;
			wire [31:0] foutp = {sbox0[idxor[47:42]], sbox1[idxor[41:36]], sbox2[idxor[35:30]], sbox3[idxor[29:24]], sbox4[idxor[23:18]], sbox5[idxor[17:12]], sbox6[idxor[11:6]], sbox7[idxor[5:0]]};
			wire [31:0] fout = {
				foutp[16], foutp[25], foutp[12], foutp[11], foutp[3], foutp[20], foutp[4], foutp[15],
		 		foutp[31], foutp[17], foutp[9], foutp[6], foutp[27], foutp[14], foutp[1], foutp[22],
				foutp[30], foutp[24], foutp[8], foutp[18], foutp[0], foutp[5], foutp[29], foutp[23],
				foutp[13], foutp[19], foutp[2], foutp[26], foutp[10], foutp[21], foutp[28], foutp[7]
			};
	
			always @(posedge clk) begin
				if(i == 0 || i == 1 || i == 8 || i == 15) begin
					kl[i+1] <= {kl[i][26:0], kl[i][27]};
					kr[i+1] <= {kr[i][26:0], kr[i][27]};
				end else begin
					kl[i+1] <= {kl[i][25:0], kl[i][27:26]};
					kr[i+1] <= {kr[i][25:0], kr[i][27:26]};
				end
				l[i+1] <= r[i] ^ x[i];
				r[i+1] <= l[i];
				x[i+1] <= fout;
				valid[i+1] <= valid[i];
			end
		end
	endgenerate

	initial begin
	`include "sbox.vh"
	end
	
endmodule
