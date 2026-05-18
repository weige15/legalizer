# set caseName "testcase/ispd19_sample"
# set db [ord::get_db]
# set chip [$db getChip]
# set block [$chip getBlock]
# set design_name [$block getName]

set file_name [file join $caseName "${design_name}_insts.gp"]

set fp [open $file_name "w"]
fconfigure $fp -buffering line

# 1. Extract DBU
set dbu_per_micron [$block getDbUnitsPerMicron]
puts $fp "DBU_Per_Micron $dbu_per_micron"

# 2. Die area & Site info
set die_area [$block getDieArea]
puts $fp "DieArea_LL [expr int([$die_area xMin])] [expr int([$die_area yMin])]"
puts $fp "DieArea_UR [expr int([$die_area xMax])] [expr int([$die_area yMax])]"

set rows [$block getRows]
if {[llength $rows] > 0} {
    set site [[lindex $rows 0] getSite]
    puts $fp "Site_Width [expr int([$site getWidth])]"
    puts $fp "Site_Height [expr int([$site getHeight])]"
}

puts $fp ""
puts $fp "Name LLX LLY Width Height Type"

# 3. Instances
set insts [$block getInsts]

foreach inst $insts {
    set name [$inst getName]
    set bbox [$inst getBBox]
    
    if {$bbox eq ""} { continue }

    # Get coord and size (DBU)
    set x1 [expr int([$bbox xMin])]
    set y1 [expr int([$bbox yMin])]
    set w  [expr int([$bbox xMax] - $x1)]
    set h  [expr int([$bbox yMax] - $y1)]

    # Cell or Macro
    set master [$inst getMaster]
    set m_type [$master getType]
    
    if {$m_type eq "BLOCK"} {
        set cat "MACRO"
    } else {
        set cat "CELL"
    }

    puts $fp "$name $x1 $y1 $w $h $cat"
}

# 4. Blockages
set blockages [$block getBlockages]
set b_count 0
foreach blockage $blockages {
    set bbox [$blockage getBBox]
    if {$bbox eq "NULL" || $bbox eq ""} { continue }

    set x1 [expr int([$bbox xMin])]
    set y1 [expr int([$bbox yMin])]
    set w  [expr int([$bbox xMax] - $x1)]
    set h  [expr int([$bbox yMax] - $y1)]
    
    set b_name "B$b_count"
    puts $fp "$b_name $x1 $y1 $w $h BLOCKAGE"
    incr b_count
}

close $fp
puts "DONE: $file_name generated"