# 1. Load files
if {[info exists ::env(CASE_NAME)]} {
    set caseName $::env(CASE_NAME)
} else {
    set caseName "public/ispd15_mgc_matrix_mult_a"
}
if {[info exists ::env(THRESHOLD)]} {
    set threshold $::env(THRESHOLD)
} else {
    set threshold 45
}
if {[info exists ::env(ALPHA)]} {
    set alpha $::env(ALPHA)
} else {
    set alpha 0.7
}
if {[info exists ::env(PLACER_MODE)]} {
    set placer_mode $::env(PLACER_MODE)
} else {
    set placer_mode "legalizer"
}
set norm_factor 18.2

set lef_file [lindex [glob -directory $caseName *.lef] 0]
set def_file [lindex [glob -directory $caseName *.def] 0]
if {$lef_file eq "" || $def_file eq ""} {
    error "Error: Cannot find LEF or DEF file in $caseName"
}
read_lef $lef_file
read_def $def_file

set db [ord::get_db]
set chip [$db getChip]
set block [$chip getBlock]
set design_name [$block getName]
set die_area [$block getDieArea]

# 2. Perform OpenROAD Global Placement to disturb
global_placement -density 0.95

# 3. Extract <input>.gp
source extract.tcl

# Record prev position
set insts [$block getInsts]
foreach inst [$block getInsts] {
    set inst_locs([$inst getName]) [$inst getLocation]
}

# 4. Run the selected local validation mode.
set gp_file [file join $caseName "${design_name}_insts.gp"]
set out_tcl [file join $caseName "${design_name}_insts.tcl"]
if {$placer_mode eq "legalizer"} {
    exec make
    exec timeout 30m ./Legalizer $alpha $threshold $gp_file $out_tcl
    source $out_tcl
} elseif {$placer_mode eq "detailed"} {
    detailed_placement
} else {
    error "Unknown PLACER_MODE '$placer_mode'. Use legalizer or detailed."
}

# 5. Check Legality
if {[catch { check_placement -verbose } result] == 0} {
    puts "Legality PASS\n"
} else {
    puts $result
    error "Legality FAIL"
}

# 6. Calculate displacement
set total_dist 0
set max_dist 0
set dbu_val [$block getDbUnitsPerMicron]

foreach inst [$block getInsts] {
    set name [$inst getName]
    set loc_new [$inst getLocation]
    set loc_old $inst_locs($name)
    
    set dx [expr {abs([lindex $loc_new 0] - [lindex $loc_old 0])}]
    set dy [expr {abs([lindex $loc_new 1] - [lindex $loc_old 1])}]
    set d [expr {$dx + $dy}]
    
    set total_dist [expr {$total_dist + $d}]
    if {$d > $max_dist} { set max_dist $d }
}

set count [llength [$block getInsts]]
set total_u [expr {$total_dist / double($dbu_val)}]
set avg_u   [expr {$total_u / $count}]
set max_u   [expr {$max_dist / double($dbu_val)}]

# Compute an assignment-style DOR fallback directly from OpenDB geometry.
# This is used when the GUI placement heatmap is unavailable in batch mode.
set grid_size [expr {10 * $dbu_val}]
set die_x_min [expr {int([$die_area xMin])}]
set die_y_min [expr {int([$die_area yMin])}]
set die_x_max [expr {int([$die_area xMax])}]
set die_y_max [expr {int([$die_area yMax])}]
set grid_cols [expr {int(ceil(($die_x_max - $die_x_min) / double($grid_size)))}]
set grid_rows [expr {int(ceil(($die_y_max - $die_y_min) / double($grid_size)))}]
array unset manual_occ
array unset manual_macro

foreach inst [$block getInsts] {
    set bbox [$inst getBBox]
    if {$bbox eq ""} { continue }

    set x0 [expr {int([$bbox xMin])}]
    set y0 [expr {int([$bbox yMin])}]
    set x1 [expr {int([$bbox xMax])}]
    set y1 [expr {int([$bbox yMax])}]
    if {$x1 <= $die_x_min || $x0 >= $die_x_max || $y1 <= $die_y_min || $y0 >= $die_y_max} {
        continue
    }

    set x0 [expr {max($x0, $die_x_min)}]
    set y0 [expr {max($y0, $die_y_min)}]
    set x1 [expr {min($x1, $die_x_max)}]
    set y1 [expr {min($y1, $die_y_max)}]
    set gx0 [expr {int(floor(($x0 - $die_x_min) / double($grid_size)))}]
    set gy0 [expr {int(floor(($y0 - $die_y_min) / double($grid_size)))}]
    set gx1 [expr {int(floor(($x1 - 1 - $die_x_min) / double($grid_size)))}]
    set gy1 [expr {int(floor(($y1 - 1 - $die_y_min) / double($grid_size)))}]

    set is_macro 0
    set master [$inst getMaster]
    if {$master ne "" && [$master getType] eq "BLOCK"} {
        set is_macro 1
    }

    for {set gy $gy0} {$gy <= $gy1} {incr gy} {
        set grid_y0 [expr {$die_y_min + $gy * $grid_size}]
        set grid_y1 [expr {min($grid_y0 + $grid_size, $die_y_max)}]
        for {set gx $gx0} {$gx <= $gx1} {incr gx} {
            set grid_x0 [expr {$die_x_min + $gx * $grid_size}]
            set grid_x1 [expr {min($grid_x0 + $grid_size, $die_x_max)}]
            set ox [expr {min($x1, $grid_x1) - max($x0, $grid_x0)}]
            set oy [expr {min($y1, $grid_y1) - max($y0, $grid_y0)}]
            if {$ox <= 0 || $oy <= 0} { continue }
            set area [expr {$ox * $oy}]
            set key "$gx,$gy"
            if {$is_macro} {
                if {![info exists manual_macro($key)]} { set manual_macro($key) 0 }
                set manual_macro($key) [expr {$manual_macro($key) + $area}]
            } else {
                if {![info exists manual_occ($key)]} { set manual_occ($key) 0 }
                set manual_occ($key) [expr {$manual_occ($key) + $area}]
            }
        }
    }
}

# Remove macro
set count 0
foreach inst $insts {
    set inst_name [$inst getName]
    
    if {[string match "h*" $inst_name]} {
        puts "Removing instance: $inst_name"
        odb::dbInst_destroy $inst
        incr count
    }
}
puts "Done. Removed $count macro."

# Dump density csv, 10u grid
# DO NOT change the name
set heat_name [file join $caseName "${design_name}_heat.csv"]
set heatmap_ok 0
catch { gui::set_heatmap "Placement" rebuild }
if {[catch { gui::dump_heatmap "Placement" "$heat_name" } heatmap_result] == 0} {
    set heatmap_ok 1
    puts "Done: $heat_name generated."
} else {
    puts "Warning: could not dump OpenROAD placement heatmap: $heatmap_result"
    puts "Using manual 10u grid DOR fallback."
}

# 7. Calculate DOR (0-100)
set total_grids 0
set overflow_grids 0

if {$heatmap_ok} {
    set fp [open "$heat_name" r]
    gets $fp line ;

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
} else {
    for {set gy 0} {$gy < $grid_rows} {incr gy} {
        set grid_y0 [expr {$die_y_min + $gy * $grid_size}]
        set grid_y1 [expr {min($grid_y0 + $grid_size, $die_y_max)}]
        for {set gx 0} {$gx < $grid_cols} {incr gx} {
            set grid_x0 [expr {$die_x_min + $gx * $grid_size}]
            set grid_x1 [expr {min($grid_x0 + $grid_size, $die_x_max)}]
            set grid_area [expr {($grid_x1 - $grid_x0) * ($grid_y1 - $grid_y0)}]
            set key "$gx,$gy"
            set macro_area 0
            if {[info exists manual_macro($key)]} {
                set macro_area $manual_macro($key)
            }
            if {$macro_area >= $grid_area} {
                continue
            }
            set occ_area 0
            if {[info exists manual_occ($key)]} {
                set occ_area $manual_occ($key)
            }
            set free_area [expr {$grid_area - $macro_area}]
            set density [expr {100.0 * $occ_area / double($free_area)}]
            incr total_grids
            if {$density > $threshold} {
                incr overflow_grids
            }
        }
    }
}

if {$total_grids > 0} {
    set dor [expr {($overflow_grids / double($total_grids)) * 100.0}]
} else {
    set dor 0
}

# Normalize Displacement
set norm_disp [expr {$avg_u * $norm_factor}]

# Calculate Quality Score, lower is better
set quality_score [expr {$alpha * $norm_disp + (1.0 - $alpha) * $dor}]

# Final Result
puts ""
puts "Performance Metrics"
puts "----------------------------------------"
puts "Total displacement     : [format "%.1f" $total_u] u"
puts "Average displacement   : [format "%.1f" $avg_u] u"
puts "Max displacement       : [format "%.1f" $max_u] u"
puts "Threshold              : $threshold"
puts "Alpha                  : $alpha"
puts "Placer mode            : $placer_mode"
puts "Total Grids            : $total_grids"
puts "Overflow Grids         : $overflow_grids"
puts "DOR (Density Overflow) : [format "%.2f" $dor] %"
puts "Norm. Displacement     : [format "%.2f" $norm_disp]"
puts "----------------------------------------"
puts "FINAL QUALITY SCORE    : [format "%.4f" $quality_score]"
puts "----------------------------------------"
