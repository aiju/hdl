source ../../xlnx/fns.tcl

new
read_verilog [notb [here]/*.v $hdl/lib/*.v]
read_xdc [here]/top.xdc
synth top none
implement
bitstream
