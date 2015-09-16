close_project -quiet
create_project -part xc7z015clg485-1 -in_memory

read_verilog \
	top.v \
	regs.v \
	descrack.v \
	des.v \
	../lib/axi3.v \
	../lib/sync.v \
	
read_xdc constraints.xdc

synth_design -top top -flatten_hierarchy none
write_checkpoint -force build/post_synth.dcp

opt_design
place_design -directive Explore
write_checkpoint -force build/post_place.dcp

phys_opt_design -directive Explore
route_design -directive Explore
write_checkpoint -force build/post_route.dcp

write_bitstream -force -bin_file build/out.bit

report_timing_summary -file build/timing.rpt -name timing_1 -max_paths 100
report_utilization -file build/util.rpt

