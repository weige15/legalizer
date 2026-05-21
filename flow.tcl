# OpenROAD integration flow for the public placement cases.
#
# Usage:
#   openroad -exit flow.tcl
#
# Optional environment variables:
#   CASE_NAME=ispd19_sample             ;# or a full/relative case path
#   CASES="{ispd19_sample ispd15_mgc_matrix_mult_a}"
#   ALPHA=0.7
#   THRESHOLD=45
#   RUN_GP=1                            ;# set 0 to skip global_placement

set script_path [file normalize [info script]]
set repo_root [file dirname $script_path]

proc flow_fail {msg} {
    error "FLOW ERROR: $msg"
}

proc normalize_case_path {repo_root case_name} {
    if {[file pathtype $case_name] eq "absolute"} {
        return [file normalize $case_name]
    }
    if {[file exists [file join $repo_root $case_name]]} {
        return [file normalize [file join $repo_root $case_name]]
    }
    return [file normalize [file join $repo_root public $case_name]]
}

proc flow_alpha {} {
    if {[info exists ::env(ALPHA)]} {
        return $::env(ALPHA)
    }
    return 0.7
}

proc flow_threshold {} {
    if {[info exists ::env(THRESHOLD)]} {
        return $::env(THRESHOLD)
    }
    return 45
}

proc flow_run_gp {} {
    if {[info exists ::env(RUN_GP)]} {
        return $::env(RUN_GP)
    }
    return 1
}

proc run_parent {repo_root script_path} {
    set single_case [expr {![info exists ::env(CASES)] && [info exists ::env(CASE_NAME)]}]
    if {[info exists ::env(CASES)]} {
        set cases $::env(CASES)
    } elseif {[info exists ::env(CASE_NAME)]} {
        set cases [list $::env(CASE_NAME)]
    } else {
        set cases [list ispd15_mgc_matrix_mult_a ispd19_sample]
    }

    puts "Building Legalizer"
    if {[catch {exec make -C $repo_root >@ stdout 2>@ stderr} result]} {
        flow_fail "make failed: $result"
    }

    if {$single_case} {
        set ::env(CASE_NAME) [normalize_case_path $repo_root $::env(CASE_NAME)]
        puts ""
        puts "============================================================"
        puts "Running public case: $::env(CASE_NAME)"
        puts "============================================================"
        run_child $repo_root
        return
    }

    set openroad_bin [info nameofexecutable]
    if {$openroad_bin eq "" || ![string match -nocase "*openroad*" [file tail $openroad_bin]]} {
        set openroad_bin openroad
    }

    foreach case_name $cases {
        set case_path [normalize_case_path $repo_root $case_name]
        puts ""
        puts "============================================================"
        puts "Running public case: $case_path"
        puts "============================================================"

        set cmd [list env FLOW_CHILD=1 CASE_NAME=$case_path ALPHA=[flow_alpha] \
                      THRESHOLD=[flow_threshold] RUN_GP=[flow_run_gp] \
                      $openroad_bin -exit $script_path]
        if {[catch {exec {*}$cmd >@ stdout 2>@ stderr} result]} {
            flow_fail "case failed: $case_path\n$result"
        }
    }

    puts ""
    puts "All requested public cases passed the OpenROAD flow."
}

proc load_case {case_name} {
    set lef_files [lsort [glob -nocomplain -directory $case_name *.lef]]
    set def_files [lsort [glob -nocomplain -directory $case_name *.def]]
    if {[llength $lef_files] == 0 || [llength $def_files] == 0} {
        flow_fail "cannot find LEF/DEF files in $case_name"
    }

    foreach lef_file $lef_files {
        puts "read_lef $lef_file"
        read_lef $lef_file
    }

    set def_file [lindex $def_files 0]
    puts "read_def $def_file"
    read_def $def_file
}

proc movable_insts {block} {
    set result {}
    foreach inst [$block getInsts] {
        set master [$inst getMaster]
        if {$master eq "NULL" || $master eq ""} {
            continue
        }
        if {[$master getType] eq "BLOCK"} {
            continue
        }
        lappend result $inst
    }
    return $result
}

proc record_locations {block array_name} {
    upvar $array_name locs
    array unset locs
    foreach inst [movable_insts $block] {
        set locs([$inst getName]) [$inst getLocation]
    }
}

proc report_displacement {block array_name} {
    upvar $array_name locs
    set total_dist 0
    set max_dist 0
    set count 0
    set dbu_val [$block getDbUnitsPerMicron]

    foreach inst [movable_insts $block] {
        set name [$inst getName]
        if {![info exists locs($name)]} {
            continue
        }
        set loc_new [$inst getLocation]
        set loc_old $locs($name)
        set dx [expr {abs([lindex $loc_new 0] - [lindex $loc_old 0])}]
        set dy [expr {abs([lindex $loc_new 1] - [lindex $loc_old 1])}]
        set dist [expr {$dx + $dy}]
        set total_dist [expr {$total_dist + $dist}]
        if {$dist > $max_dist} {
            set max_dist $dist
        }
        incr count
    }

    if {$count == 0} {
        return [dict create total_u 0.0 avg_u 0.0 max_u 0.0 count 0]
    }

    set total_u [expr {$total_dist / double($dbu_val)}]
    set avg_u [expr {$total_u / double($count)}]
    set max_u [expr {$max_dist / double($dbu_val)}]
    return [dict create total_u $total_u avg_u $avg_u max_u $max_u count $count]
}

proc remove_macros_for_heatmap {block} {
    set removed 0
    foreach inst [$block getInsts] {
        set master [$inst getMaster]
        if {$master eq "NULL" || $master eq ""} {
            continue
        }
        if {[$master getType] eq "BLOCK"} {
            odb::dbInst_destroy $inst
            incr removed
        }
    }
    return $removed
}

proc report_dor {case_name design_name threshold} {
    set heat_name [file join $case_name "${design_name}_heat.csv"]
    if {[catch {gui::dump_heatmap "Placement" $heat_name} result]} {
        puts "WARNING: could not dump OpenROAD placement heatmap: $result"
        return [dict create total_grids 0 overflow_grids 0 dor 0.0 heat_name ""]
    }

    set total_grids 0
    set overflow_grids 0
    set fp [open $heat_name r]
    gets $fp line
    while {[gets $fp line] >= 0} {
        set data [split [string trim $line] ","]
        set density [lindex $data 4]
        if {$density ne ""} {
            incr total_grids
            if {$density > $threshold} {
                incr overflow_grids
            }
        }
    }
    close $fp

    if {$total_grids > 0} {
        set dor [expr {($overflow_grids / double($total_grids)) * 100.0}]
    } else {
        set dor 0.0
    }
    return [dict create total_grids $total_grids overflow_grids $overflow_grids \
                 dor $dor heat_name $heat_name]
}

proc run_child {repo_root} {
    if {![info exists ::env(CASE_NAME)]} {
        flow_fail "CASE_NAME is required in child mode"
    }

    set caseName [normalize_case_path $repo_root $::env(CASE_NAME)]
    set alpha [flow_alpha]
    set threshold [flow_threshold]
    puts "Case directory: $caseName"
    puts "alpha=$alpha threshold=$threshold"

    load_case $caseName

    set db [ord::get_db]
    set chip [$db getChip]
    set block [$chip getBlock]
    set design_name [$block getName]
    set ::caseName $caseName
    set ::db $db
    set ::chip $chip
    set ::block $block
    set ::design_name $design_name

    if {[flow_run_gp]} {
        puts "Running global_placement -density 0.95"
        global_placement -density 0.95
    } else {
        puts "Skipping global_placement because RUN_GP=0"
    }

    record_locations $block inst_locs

    set extractor [file join $repo_root extract_v2.tcl]
    if {![file exists $extractor]} {
        flow_fail "missing extractor: $extractor"
    }
    puts "Extracting GP file with $extractor"
    source $extractor

    set gp_file [file join $caseName "${design_name}_insts.gp"]
    set out_tcl [file join $caseName "${design_name}_legalized.tcl"]
    if {![file exists $gp_file] || [file size $gp_file] == 0} {
        flow_fail "extractor did not create a nonempty GP file: $gp_file"
    }

    set legalizer [file join $repo_root Legalizer]
    if {![file executable $legalizer]} {
        flow_fail "Legalizer is not built or not executable: $legalizer"
    }

    puts "Running Legalizer"
    set cmd [list timeout 30m $legalizer $alpha $threshold $gp_file $out_tcl]
    if {[catch {exec {*}$cmd >@ stdout 2>@ stderr} result]} {
        flow_fail "Legalizer failed: $result"
    }
    if {![file exists $out_tcl] || [file size $out_tcl] == 0} {
        flow_fail "Legalizer did not create a nonempty TCL file: $out_tcl"
    }

    set fp [open $out_tcl r]
    set out_text [read $fp]
    close $fp
    if {[string first "detailed_placement" $out_text] >= 0} {
        flow_fail "Legalizer output contains forbidden detailed_placement command"
    }

    puts "Sourcing legalized placement TCL"
    source $out_tcl

    puts "Checking placement legality"
    if {[catch {check_placement -verbose} result]} {
        flow_fail "check_placement failed: $result"
    }
    puts "Legality PASS"

    set disp [report_displacement $block inst_locs]
    set removed [remove_macros_for_heatmap $block]
    puts "Removed $removed macro instance(s) before heatmap DOR reporting"
    set dor_info [report_dor $caseName $design_name $threshold]

    set avg_u [dict get $disp avg_u]
    set dor [dict get $dor_info dor]
    set quality [expr {$alpha * $avg_u + (1.0 - $alpha) * $dor}]

    puts ""
    puts "Performance Metrics"
    puts "----------------------------------------"
    puts "Design                 : $design_name"
    puts "Movable cells          : [dict get $disp count]"
    puts "Total displacement     : [format "%.3f" [dict get $disp total_u]] u"
    puts "Average displacement   : [format "%.3f" $avg_u] u"
    puts "Max displacement       : [format "%.3f" [dict get $disp max_u]] u"
    puts "Threshold              : $threshold"
    puts "Total grids            : [dict get $dor_info total_grids]"
    puts "Overflow grids         : [dict get $dor_info overflow_grids]"
    puts "DOR                    : [format "%.2f" $dor] %"
    puts "Quality                : [format "%.4f" $quality]"
    puts "GP input               : $gp_file"
    puts "Legalizer TCL          : $out_tcl"
    if {[dict get $dor_info heat_name] ne ""} {
        puts "Heatmap CSV            : [dict get $dor_info heat_name]"
    }
    puts "----------------------------------------"
}

if {[info exists ::env(FLOW_CHILD)] && $::env(FLOW_CHILD) eq "1"} {
    run_child $repo_root
} else {
    run_parent $repo_root $script_path
}
