set thepart xc7z015clg485-1
close_project -quiet
create_project -in_memory -part $thepart

set top top
read_verilog \
	top.v \
	../../lib/uart.v \
	../../lib/sync.v \
	../../lib/axi3.v

read_xdc ablcd.xdc

set outputDir ./build
file mkdir $outputDir

synth_design -top $top -part $thepart -flatten_hierarchy none
write_checkpoint -force $outputDir/post_synth
opt_design
place_design
write_checkpoint -force $outputDir/post_place
phys_opt_design
route_design
write_checkpoint -force $outputDir/post_route

report_timing_summary -file $outputDir/post_route_timing.rpt
report_utilization -file $outputDir/post_route_util.rpt

write_bitstream -force -bin_file -file $outputDir/out.bit

if {! [string match -nocase {*timing constraints are met*} \
[report_timing_summary -no_header -no_detailed_paths -return_string]]} \
{puts "timing constraints not met"}
