# Analysis-only OpenROAD baseline flow using detailed_placement.
#
# Usage:
#   CASE_NAME=ispd19_sample openroad -exit analysis_detail.tcl
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
puts "Running detailed placement analysis for $caseName"
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

puts "Running global_placement -density 0.95"
global_placement -density 0.95
record_locations $block gp_locs
extract_gp $repo_root $caseName $design_name "after_global"

puts "Running detailed_placement for analysis baseline"
detailed_placement
puts "Checking detailed placement legality"
check_placement -verbose
extract_gp $repo_root $caseName $design_name "after_detail"

set disp [report_displacement $block gp_locs]
puts ""
puts "Detailed Placement Analysis Metrics"
puts "----------------------------------------"
puts "Design                 : $design_name"
puts "Movable cells          : [dict get $disp count]"
puts "Total displacement     : [format "%.3f" [dict get $disp total_u]] u"
puts "Average displacement   : [format "%.3f" [dict get $disp avg_u]] u"
puts "Max displacement       : [format "%.3f" [dict get $disp max_u]] u"
puts "----------------------------------------"
