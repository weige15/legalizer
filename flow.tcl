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
#   FINAL_TABLES_ONLY=1                 ;# suppress progress logs and print metrics only
#   FINAL_TABLE_LOG=flow_openroad.log   ;# stderr log path for FINAL_TABLES_ONLY=1

set script_path [file normalize [info script]]
set repo_root [file dirname $script_path]

proc flow_fail {msg} {
    if {[llength [info commands final_tables_only]] > 0 && [final_tables_only]} {
        if {[info exists ::final_table_log_path]} {
            puts "FLOW ERROR: $msg"
            puts "OpenROAD log: $::final_table_log_path"
        } else {
            puts "FLOW ERROR: $msg"
        }
    }
    error "FLOW ERROR: $msg"
}

proc final_tables_only {} {
    if {[info exists ::env(FINAL_TABLES_ONLY)]} {
        return [expr {$::env(FINAL_TABLES_ONLY) ne "" && $::env(FINAL_TABLES_ONLY) ne "0"}]
    }
    return 0
}

proc init_final_table_stderr {repo_root default_name} {
    if {![final_tables_only]} {
        return
    }

    if {[info exists ::env(FINAL_TABLE_LOG)] && $::env(FINAL_TABLE_LOG) ne ""} {
        set log_path $::env(FINAL_TABLE_LOG)
        if {[file pathtype $log_path] ne "absolute"} {
            set log_path [file normalize [file join $repo_root $log_path]]
        }
    } else {
        set log_path [file normalize [file join $repo_root $default_name]]
    }
    set ::final_table_log_path $log_path

    if {[catch {
        close stderr
        open $log_path w
    }]} {
        # If stderr redirection is unavailable, keep running; the final table still prints.
    }
}

proc log_puts {msg} {
    if {![final_tables_only]} {
        puts $msg
    }
}

proc run_quietly {script} {
    if {![final_tables_only]} {
        return [uplevel 1 $script]
    }
    if {[llength [info commands redirect]] > 0} {
        return [uplevel 1 [list redirect -variable ::quiet_output $script]]
    }
    return [uplevel 1 $script]
}

proc run_exec_streaming {cmd} {
    if {[final_tables_only]} {
        return [exec {*}$cmd 2>@1]
    }
    return [exec {*}$cmd >@ stdout 2>@ stderr]
}

proc print_flow_metrics {design_name alpha threshold disp dor_info avg_u dor quality gp_file out_tcl} {
    if {[final_tables_only]} {
        puts "Performance Metrics"
        puts "| Design | Alpha | Threshold | Movable cells | Total disp (u) | Avg disp (u) | Max disp (u) | Total grids | Overflow grids | DOR | Quality |"
        puts "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"
        puts "| $design_name | $alpha | $threshold | [dict get $disp count] | [format "%.3f" [dict get $disp total_u]] | [format "%.3f" $avg_u] | [format "%.3f" [dict get $disp max_u]] | [dict get $dor_info total_grids] | [dict get $dor_info overflow_grids] | [format "%.2f" $dor]% | [format "%.4f" $quality] |"
        puts "----------------------------------------"
        return
    }

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

init_final_table_stderr $repo_root "flow_openroad.log"

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

proc flow_metrics_csv {repo_root} {
    if {[info exists ::env(METRICS_CSV)]} {
        if {$::env(METRICS_CSV) eq ""} {
            return ""
        }
        if {[file pathtype $::env(METRICS_CSV)] eq "absolute"} {
            return [file normalize $::env(METRICS_CSV)]
        }
        return [file normalize [file join $repo_root $::env(METRICS_CSV)]]
    }
    return [file normalize [file join $repo_root flow_metrics.csv]]
}

proc csv_escape {value} {
    set text [format "%s" $value]
    if {[string first "\"" $text] >= 0} {
        regsub -all {"} $text {""} text
    }
    if {[regexp {[,\"\n\r]} $text]} {
        return "\"$text\""
    }
    return $text
}

proc append_metrics_csv {csv_path row_values} {
    if {$csv_path eq ""} {
        return
    }

    set need_header [expr {![file exists $csv_path] || [file size $csv_path] == 0}]
    set fp [open $csv_path a]
    if {$need_header} {
        puts $fp "case,design,alpha,threshold,movable_cells,average_displacement_u,total_displacement_u,max_displacement_u,total_grids,overflow_grids,dor_percent,quality"
    }

    set escaped {}
    foreach value $row_values {
        lappend escaped [csv_escape $value]
    }
    puts $fp [join $escaped ","]
    close $fp
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

    log_puts "Building Legalizer"
    if {[catch {run_exec_streaming [list make -C $repo_root]} result]} {
        flow_fail "make failed: $result"
    }

    set metrics_csv [flow_metrics_csv $repo_root]
    if {$metrics_csv ne ""} {
        set fp [open $metrics_csv w]
        puts $fp "case,design,alpha,threshold,movable_cells,average_displacement_u,total_displacement_u,max_displacement_u,total_grids,overflow_grids,dor_percent,quality"
        close $fp
        log_puts "Metrics CSV: $metrics_csv"
    }

    if {$single_case} {
        set ::env(CASE_NAME) [normalize_case_path $repo_root $::env(CASE_NAME)]
        if {$metrics_csv ne ""} {
            set ::env(METRICS_CSV) $metrics_csv
        }
        log_puts ""
        log_puts "============================================================"
        log_puts "Running public case: $::env(CASE_NAME)"
        log_puts "============================================================"
        run_child $repo_root
        return
    }

    set openroad_bin [info nameofexecutable]
    if {$openroad_bin eq "" || ![string match -nocase "*openroad*" [file tail $openroad_bin]]} {
        set openroad_bin openroad
    }

    foreach case_name $cases {
        set case_path [normalize_case_path $repo_root $case_name]
        log_puts ""
        log_puts "============================================================"
        log_puts "Running public case: $case_path"
        log_puts "============================================================"

        set cmd [list env FLOW_CHILD=1 CASE_NAME=$case_path ALPHA=[flow_alpha] \
                      THRESHOLD=[flow_threshold] RUN_GP=[flow_run_gp] \
                      METRICS_CSV=$metrics_csv \
                      $openroad_bin -exit $script_path]
        if {[catch {run_exec_streaming $cmd} result]} {
            flow_fail "case failed: $case_path\n$result"
        }
        if {[final_tables_only] && $result ne ""} {
            puts $result
        }
    }

    log_puts ""
    log_puts "All requested public cases passed the OpenROAD flow."
}

proc load_case {case_name} {
    set lef_files [lsort [glob -nocomplain -directory $case_name *.lef]]
    set def_files [lsort [glob -nocomplain -directory $case_name *.def]]
    if {[llength $lef_files] == 0 || [llength $def_files] == 0} {
        flow_fail "cannot find LEF/DEF files in $case_name"
    }

    foreach lef_file $lef_files {
        log_puts "read_lef $lef_file"
        run_quietly [list read_lef $lef_file]
    }

    set def_file [lindex $def_files 0]
    log_puts "read_def $def_file"
    run_quietly [list read_def $def_file]
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

proc intersection_area {a_lx a_ly a_ux a_uy b_lx b_ly b_ux b_uy} {
    set lx [expr {max($a_lx, $b_lx)}]
    set ly [expr {max($a_ly, $b_ly)}]
    set ux [expr {min($a_ux, $b_ux)}]
    set uy [expr {min($a_uy, $b_uy)}]
    if {$ux <= $lx || $uy <= $ly} {
        return 0
    }
    return [expr {($ux - $lx) * ($uy - $ly)}]
}

proc fallback_report_dor {block threshold} {
    set dbu_val [$block getDbUnitsPerMicron]
    set grid_size [expr {10 * $dbu_val}]
    set die [$block getDieArea]
    set die_lx [$die xMin]
    set die_ly [$die yMin]
    set die_ux [$die xMax]
    set die_uy [$die yMax]
    set total_grids 0
    set overflow_grids 0

    array unset occupied_by_grid
    foreach inst [movable_insts $block] {
        set bbox [$inst getBBox]
        set bx_min [$bbox xMin]
        set by_min [$bbox yMin]
        set bx_max [$bbox xMax]
        set by_max [$bbox yMax]
        set gx_min [expr {int(floor(($bx_min - $die_lx) / double($grid_size)))}]
        set gx_max [expr {int(floor((max($bx_max - 1, $die_lx) - $die_lx) / double($grid_size)))}]
        set gy_min [expr {int(floor(($by_min - $die_ly) / double($grid_size)))}]
        set gy_max [expr {int(floor((max($by_max - 1, $die_ly) - $die_ly) / double($grid_size)))}]
        if {$gx_min < 0} { set gx_min 0 }
        if {$gy_min < 0} { set gy_min 0 }
        for {set gy $gy_min} {$gy <= $gy_max} {incr gy} {
            set grid_y [expr {$die_ly + $gy * $grid_size}]
            if {$grid_y >= $die_uy} {
                continue
            }
            set grid_uy [expr {min($grid_y + $grid_size, $die_uy)}]
            for {set gx $gx_min} {$gx <= $gx_max} {incr gx} {
                set grid_x [expr {$die_lx + $gx * $grid_size}]
                if {$grid_x >= $die_ux} {
                    continue
                }
                set grid_ux [expr {min($grid_x + $grid_size, $die_ux)}]
                set overlap [intersection_area \
                    $grid_x $grid_y $grid_ux $grid_uy \
                    $bx_min $by_min $bx_max $by_max]
                if {$overlap <= 0} {
                    continue
                }
                set key "$gx,$gy"
                if {![info exists occupied_by_grid($key)]} {
                    set occupied_by_grid($key) 0
                }
                set occupied_by_grid($key) [expr {$occupied_by_grid($key) + $overlap}]
            }
        }
    }

    foreach key [array names occupied_by_grid] {
        scan $key "%d,%d" gx gy
        set grid_x [expr {$die_lx + $gx * $grid_size}]
        set grid_y [expr {$die_ly + $gy * $grid_size}]
        set grid_ux [expr {min($grid_x + $grid_size, $die_ux)}]
        set grid_uy [expr {min($grid_y + $grid_size, $die_uy)}]
        set area [expr {($grid_ux - $grid_x) * ($grid_uy - $grid_y)}]
        if {$area <= 0} {
            continue
        }
        set density [expr {100.0 * $occupied_by_grid($key) / double($area)}]
        incr total_grids
        if {$density > $threshold} {
            incr overflow_grids
        }
    }

    if {$total_grids > 0} {
        set dor [expr {($overflow_grids / double($total_grids)) * 100.0}]
    } else {
        set dor 0.0
    }
    return [dict create total_grids $total_grids overflow_grids $overflow_grids \
                 dor $dor heat_name ""]
}

proc report_dor {block case_name design_name threshold} {
    set heat_name [file join $case_name "${design_name}_heat.csv"]
    if {[catch {gui::set_heatmap "Placement" rebuild} result]} {
        puts "WARNING: could not rebuild OpenROAD placement heatmap: $result"
    }
    if {[catch {gui::dump_heatmap "Placement" $heat_name} result]} {
        puts "WARNING: could not dump OpenROAD placement heatmap: $result"
        puts "Using Tcl bbox-based DOR fallback"
        return [fallback_report_dor $block $threshold]
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

    if {$total_grids == 0} {
        puts "WARNING: OpenROAD heatmap CSV contained no grids"
        puts "Using Tcl bbox-based DOR fallback"
        return [fallback_report_dor $block $threshold]
    }
    set dor [expr {($overflow_grids / double($total_grids)) * 100.0}]
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
    log_puts "Case directory: $caseName"
    log_puts "alpha=$alpha threshold=$threshold"

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
        log_puts "Running global_placement -density 0.95"
        run_quietly {global_placement -density 0.95}
    } else {
        log_puts "Skipping global_placement because RUN_GP=0"
    }

    record_locations $block inst_locs

    set extractor [file join $repo_root extract_v2.tcl]
    if {![file exists $extractor]} {
        flow_fail "missing extractor: $extractor"
    }
    log_puts "Extracting GP file with $extractor"
    run_quietly [list source $extractor]

    set gp_file [file join $caseName "${design_name}_insts.gp"]
    set out_tcl [file join $caseName "${design_name}_insts.tcl"]
    if {![file exists $gp_file] || [file size $gp_file] == 0} {
        flow_fail "extractor did not create a nonempty GP file: $gp_file"
    }

    set legalizer [file join $repo_root Legalizer]
    if {![file executable $legalizer]} {
        flow_fail "Legalizer is not built or not executable: $legalizer"
    }

    log_puts "Running Legalizer"
    set cmd [list timeout 30m $legalizer $alpha $threshold $gp_file $out_tcl]
    if {[catch {run_exec_streaming $cmd} result]} {
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

    log_puts "Sourcing legalized placement TCL"
    run_quietly [list source $out_tcl]

    log_puts "Checking placement legality"
    if {[catch {run_quietly {check_placement -verbose}} result]} {
        flow_fail "check_placement failed: $result"
    }
    log_puts "Legality PASS"

    set disp [report_displacement $block inst_locs]
    set removed [remove_macros_for_heatmap $block]
    log_puts "Removed $removed macro instance(s) before heatmap DOR reporting"
    set dor_info [run_quietly [list report_dor $block $caseName $design_name $threshold]]

    set avg_u [dict get $disp avg_u]
    set dor [dict get $dor_info dor]
    set quality [expr {$alpha * $avg_u + (1.0 - $alpha) * $dor}]

    print_flow_metrics $design_name $alpha $threshold $disp $dor_info $avg_u $dor $quality $gp_file $out_tcl

    set metrics_csv [flow_metrics_csv $repo_root]
    append_metrics_csv $metrics_csv [list \
        $caseName \
        $design_name \
        $alpha \
        $threshold \
        [dict get $disp count] \
        [format "%.6f" $avg_u] \
        [format "%.6f" [dict get $disp total_u]] \
        [format "%.6f" [dict get $disp max_u]] \
        [dict get $dor_info total_grids] \
        [dict get $dor_info overflow_grids] \
        [format "%.6f" $dor] \
        [format "%.6f" $quality]]
    if {$metrics_csv ne "" && ![final_tables_only]} {
        puts "Metrics CSV            : $metrics_csv"
        puts "Markdown row           : | $design_name | $alpha | $threshold | [format "%.3f" $avg_u] | [format "%.2f" $dor]% | [format "%.4f" $quality] |"
    }
}

if {[info exists ::env(FLOW_CHILD)] && $::env(FLOW_CHILD) eq "1"} {
    run_child $repo_root
} else {
    run_parent $repo_root $script_path
}
