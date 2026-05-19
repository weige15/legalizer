# 1. Load files
set caseName "public/ispd15_mgc_matrix_mult_a"
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

# 2. Perform OpenROAD Global Placement to disturb
global_placement -density 0.95

# 3. Extract <input>.gp
source extract.tcl

# Record prev position
foreach inst [$block getInsts] {
    set inst_locs([$inst getName]) [$inst getLocation]
}

# 4. Legalize with this assignment implementation
set threshold 45
set alpha 0.7
set legalizer_input [file join $caseName "${design_name}_insts.gp"]
set legalizer_output [file join $caseName "${design_name}_legalized.tcl"]
exec make
exec timeout 30m ./Legalizer $alpha $threshold $legalizer_input $legalizer_output
source $legalizer_output

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
set insts [$block getInsts]
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
gui::dump_heatmap "Placement" "$heat_name"
puts "Done: $heat_name generated."

# Parameter Settings
# You are highly encouraged to experiment with different combinations to optimize the performance.
set threshold $threshold;
set alpha $alpha;
set norm_factor 18.2;

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
