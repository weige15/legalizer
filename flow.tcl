# 1. Load files
set caseName "public/ispd19_sample"
set lef_files [glob -nocomplain -directory $caseName *.lef]
set def_files [glob -nocomplain -directory $caseName *.def]
if {[llength $lef_files] == 0 || [llength $def_files] == 0} {
    error "Cannot find LEF or DEF file in $caseName"
}
set lef_file [lindex $lef_files 0]
set def_file [lindex $def_files 0]
read_lef $lef_file
read_def $def_file

set db [ord::get_db]
set chip [$db getChip]
set block [$chip getBlock]
set design_name [$block getName]

# 2. Perform OpenROAD Global Placement to disturb
global_placement -density 0.95

# Parameter Settings
# You are highly encouraged to experiment with different combinations to optimize the performance.
set threshold 45
set alpha 0.7
set norm_factor 18.2

# 3. Extract <input>.gp
source extract.tcl
set gp_file [file join $caseName "${design_name}_insts.gp"]
set legalizer_tcl [file join $caseName "${design_name}_legalized.tcl"]

# Record prev position
foreach inst [$block getInsts] {
    set inst_locs([$inst getName]) [$inst getLocation]
}

# 4. Legalize with the external program and source only validated output.
if {[catch {exec make} build_result]} {
    error "Legalizer build failed:\n$build_result"
}
if {![file executable "./Legalizer"]} {
    error "Legalizer executable was not created by make"
}
if {![file exists $gp_file] || [file size $gp_file] == 0} {
    error "Expected extracted GP file is missing or empty: $gp_file"
}
if {[file exists $legalizer_tcl]} {
    file delete -force $legalizer_tcl
}
if {[catch {exec timeout 30m ./Legalizer $alpha $threshold $gp_file $legalizer_tcl} legalizer_result]} {
    error "Legalizer invocation failed:\n$legalizer_result"
}
if {![file exists $legalizer_tcl] || [file size $legalizer_tcl] == 0} {
    error "Legalizer did not produce a nonempty output TCL: $legalizer_tcl"
}
set fp_check [open $legalizer_tcl r]
set legalizer_text [read $fp_check]
close $fp_check
if {[string first "detailed_placement" $legalizer_text] >= 0} {
    error "Refusing to source output TCL because it contains detailed_placement"
}
if {[catch {source $legalizer_tcl} source_result]} {
    error "Failed to source Legalizer output $legalizer_tcl:\n$source_result"
}

proc rect_overlap_area {ax0 ay0 ax1 ay1 bx0 by0 bx1 by1} {
    set x0 [expr {max($ax0, $bx0)}]
    set y0 [expr {max($ay0, $by0)}]
    set x1 [expr {min($ax1, $bx1)}]
    set y1 [expr {min($ay1, $by1)}]
    if {$x0 >= $x1 || $y0 >= $y1} {
        return 0
    }
    return [expr {($x1 - $x0) * ($y1 - $y0)}]
}

proc write_fallback_density_csv {block heat_name} {
    set dbu_val [$block getDbUnitsPerMicron]
    set grid_size [expr {10 * $dbu_val}]
    set die_area [$block getDieArea]
    set die_x0 [expr {int([$die_area xMin])}]
    set die_y0 [expr {int([$die_area yMin])}]
    set die_x1 [expr {int([$die_area xMax])}]
    set die_y1 [expr {int([$die_area yMax])}]
    set cols [expr {int(ceil(($die_x1 - $die_x0) / double($grid_size)))}]
    set rows [expr {int(ceil(($die_y1 - $die_y0) / double($grid_size)))}]

    set fp [open $heat_name w]
    puts $fp "x,y,width,height,density"
    for {set gy 0} {$gy < $rows} {incr gy} {
        set gy0 [expr {$die_y0 + $gy * $grid_size}]
        set gy1 [expr {min($die_y1, $gy0 + $grid_size)}]
        for {set gx 0} {$gx < $cols} {incr gx} {
            set gx0 [expr {$die_x0 + $gx * $grid_size}]
            set gx1 [expr {min($die_x1, $gx0 + $grid_size)}]
            set grid_area [expr {double(($gx1 - $gx0) * ($gy1 - $gy0))}]
            if {$grid_area <= 0.0} {
                continue
            }

            set covered_by_macro 0
            set movable_area 0
            foreach inst [$block getInsts] {
                set bbox [$inst getBBox]
                if {$bbox eq "" || $bbox eq "NULL"} {
                    continue
                }
                set ix0 [expr {int([$bbox xMin])}]
                set iy0 [expr {int([$bbox yMin])}]
                set ix1 [expr {int([$bbox xMax])}]
                set iy1 [expr {int([$bbox yMax])}]
                set overlap [rect_overlap_area $gx0 $gy0 $gx1 $gy1 $ix0 $iy0 $ix1 $iy1]
                if {$overlap == 0} {
                    continue
                }
                set master [$inst getMaster]
                set m_type [$master getType]
                if {$m_type eq "BLOCK"} {
                    set covered_by_macro 1
                    break
                }
                set movable_area [expr {$movable_area + $overlap}]
            }
            if {$covered_by_macro} {
                continue
            }
            set density [expr {($movable_area / $grid_area) * 100.0}]
            puts $fp "$gx0,$gy0,[expr {$gx1 - $gx0}],[expr {$gy1 - $gy0}],$density"
        }
    }
    close $fp
}

# 5. Check Legality
if {[catch { check_placement -verbose } result] == 0} {
    puts "Legality PASS\n"
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
if {[catch {gui::dump_heatmap "Placement" "$heat_name"} heat_result]} {
    puts "Warning: GUI heatmap dump failed, writing fallback density CSV: $heat_result"
    write_fallback_density_csv $block $heat_name
}
if {![file exists $heat_name] || [file size $heat_name] == 0} {
    error "Density CSV was not created: $heat_name"
}
puts "Done: $heat_name generated."

# 7. Calculate DOR (0-100)
set total_grids 0
set overflow_grids 0

set fp [open "$heat_name" r]
gets $fp line ;

set total_grids 0
set overflow_grids 0

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
puts "Total Grids            : $total_grids"
puts "Overflow Grids         : $overflow_grids"
puts "DOR (Density Overflow) : [format "%.2f" $dor] %"
puts "Norm. Displacement     : [format "%.2f" $norm_disp]"
puts "----------------------------------------"
puts "FINAL QUALITY SCORE    : [format "%.4f" $quality_score]"
puts "----------------------------------------"
