create_clock -name clk -period 10.000 [get_pins PS7/FCLKCLK[0]]
create_clock -name fastclk -period 3.000 [get_pins PS7/FCLKCLK[1]]

set_false_path -through [get_pins {regs/busy[*] regs/res[*] regs/goal[*] regs/run[*] regs/start[*]}]

set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLUP [current_design]

set pblock [create_pblock des0]
resize_pblock $pblock -add SLICE_X26Y112:SLICE_X39Y149
add_cells_to_pblock $pblock [get_cells {genblk1[0].descrack}]
set pblock [create_pblock des1]
resize_pblock $pblock -add SLICE_X52Y112:SLICE_X65Y149
add_cells_to_pblock $pblock [get_cells {genblk1[1].descrack}]
set pblock [create_pblock des2]
resize_pblock $pblock -add SLICE_X66Y112:SLICE_X79Y149
add_cells_to_pblock $pblock [get_cells {genblk1[2].descrack}]
set pblock [create_pblock des3]
resize_pblock $pblock -add SLICE_X80Y112:SLICE_X96Y149
add_cells_to_pblock $pblock [get_cells {genblk1[3].descrack}]
set pblock [create_pblock des4]
resize_pblock $pblock -add SLICE_X26Y74:SLICE_X39Y111
add_cells_to_pblock $pblock [get_cells {genblk1[4].descrack}]
set pblock [create_pblock des5]
resize_pblock $pblock -add SLICE_X52Y74:SLICE_X65Y111
add_cells_to_pblock $pblock [get_cells {genblk1[5].descrack}]
set pblock [create_pblock des6]
resize_pblock $pblock -add SLICE_X66Y74:SLICE_X79Y111
add_cells_to_pblock $pblock [get_cells {genblk1[6].descrack}]
set pblock [create_pblock des7]
resize_pblock $pblock -add SLICE_X80Y74:SLICE_X96Y111
add_cells_to_pblock $pblock [get_cells {genblk1[7].descrack}]
