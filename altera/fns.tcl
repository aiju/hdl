load_package flow

proc new {} {
	if [is_project_open] {project_close}
	project_new test -overwrite
}

proc here {} {
	file dirname [info script]
}

proc verilog_files {l} {
	foreach i $l {
		set_global_assignment -name VERILOG_FILE [file join [here] $i]
	}
}
