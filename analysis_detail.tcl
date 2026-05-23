# Analysis-only OpenROAD baseline flow using detailed_placement.
#
# Usage:
#   CASE_NAME=ispd19_sample openroad -exit analysis_detail.tcl
#   RUN_GP=0 CASE_NAME=generated/ispd15_mgc_matrix_mult_a_cluster openroad -exit analysis_detail.tcl
#
# This script is for comparison only. Do not use it as Legalizer output.

set script_path [file normalize [info script]]
set repo_root [file dirname $script_path]

proc analysis_fail {msg} {
    error "ANALYSIS ERROR: $msg"
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

proc selected_case {repo_root} {
    if {[info exists ::env(CASE_NAME)]} {
        return [normalize_case_path $repo_root $::env(CASE_NAME)]
    }
    return [normalize_case_path $repo_root ispd19_sample]
}

proc analysis_threshold {} {
    if {[info exists ::env(THRESHOLD)]} {
        return $::env(THRESHOLD)
    }
    return 45
}

proc analysis_run_gp {} {
    if {[info exists ::env(RUN_GP)]} {
        return $::env(RUN_GP)
    }
    return 1
}

proc load_case {case_name} {
    set lef_files [lsort [glob -nocomplain -directory $case_name *.lef]]
    set def_files [lsort [glob -nocomplain -directory $case_name *.def]]
    if {[llength $lef_files] == 0 || [llength $def_files] == 0} {
        analysis_fail "cannot find LEF/DEF files in $case_name"
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
    set heat_name [file join $case_name "${design_name}_after_detail_heat.csv"]
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

proc extract_gp {repo_root case_name design_name suffix} {
    set extractor [file join $repo_root extract_v2.tcl]
    if {![file exists $extractor]} {
        analysis_fail "missing extractor: $extractor"
    }
    source $extractor

    set default_gp [file join $case_name "${design_name}_insts.gp"]
    set named_gp [file join $case_name "${design_name}_${suffix}.gp"]
    file rename -force $default_gp $named_gp
    puts "Saved $suffix GP: $named_gp"
    return $named_gp
}

set caseName [selected_case $repo_root]
set threshold [analysis_threshold]
puts "Running detailed placement analysis for $caseName"
puts "threshold=$threshold"
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

if {[analysis_run_gp]} {
    puts "Running global_placement -density 0.95"
    global_placement -density 0.95
    record_locations $block gp_locs
    extract_gp $repo_root $caseName $design_name "after_global"
} else {
    puts "Skipping global_placement because RUN_GP=0"
    record_locations $block gp_locs
    extract_gp $repo_root $caseName $design_name "after_input"
}

puts "Running detailed_placement for analysis baseline"
detailed_placement
puts "Checking detailed placement legality"
check_placement -verbose
extract_gp $repo_root $caseName $design_name "after_detail"

set disp [report_displacement $block gp_locs]
set removed [remove_macros_for_heatmap $block]
puts "Removed $removed macro instance(s) before DOR reporting"
set dor_info [report_dor $block $caseName $design_name $threshold]

puts ""
puts "Detailed Placement Analysis Metrics"
puts "----------------------------------------"
puts "Design                 : $design_name"
puts "Movable cells          : [dict get $disp count]"
puts "Total displacement     : [format "%.3f" [dict get $disp total_u]] u"
puts "Average displacement   : [format "%.3f" [dict get $disp avg_u]] u"
puts "Max displacement       : [format "%.3f" [dict get $disp max_u]] u"
puts "Threshold              : $threshold"
puts "Total grids            : [dict get $dor_info total_grids]"
puts "Overflow grids         : [dict get $dor_info overflow_grids]"
puts "DOR                    : [format "%.2f" [dict get $dor_info dor]] %"
if {[dict get $dor_info heat_name] ne ""} {
    puts "Heatmap CSV            : [dict get $dor_info heat_name]"
}
puts "----------------------------------------"
