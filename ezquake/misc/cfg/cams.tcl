#
# Camera positions bookmarks 1.1
#
# Use [Ctrl]+[0], [1], ..., [9] to store cam positions
# Use [0], [1], ..., [9] to load stored camera positions
# Use [Shift]+[0], [1], ..., [9] to move smoothly to cam position
#   (will happen only during capturing)
#

# -- settings --
set cams_moveframes 50; # duration - how many frames will the movement take 

cmd bind ctrl +cams_save_mode
cmd bind shift +cams_move_mode


# -- code -- (don't touch this)

set cams_bindn 0

for {set i 0} {$i < 10} {incr i} {
    cmd bind $i cams_go $i
}

set cams_bindmode 0
set cams_totalmoves 0

alias +cams_save_mode {} {set ::cams_bindmode 1}
alias -cams_save_mode {} {set ::cams_bindmode 0}
alias +cams_move_mode {} {set ::cams_bindmode 2}
alias -cams_move_mode {} {set ::cams_bindmode 0}

#using globals sux in TCL (as everywhere), i'm new to TCL and perhaps i could use some namespaces here and there

# this one is bound to 0,1,2,...9 keys
alias cams_go {slot} {
    global cams_bindmode
    if {$cams_bindmode == 0} {
        cams_load $slot
    } elseif { $cams_bindmode == 1 } {
        cams_save $slot
    } else {
        cams_move $slot
    }
}

proc cams_load {slot} {
    global cam_angles_saved
    global cam_pos_saved
    cmd cam_angles $cam_angles_saved($slot)
    cmd cam_pos $cam_pos_saved($slot)
}

proc cams_save {slot} {
    global cam_angles_saved
    global cam_pos_saved
    global cam_angles
    set cam_angles_saved($slot) $::cam_angles
    set cam_pos_saved($slot) $::cam_pos
    cams_save_detailed $slot
}

# this saves each value to separated field. it's being used in the move
proc cams_save_detailed {slot} {
    global cams_saved_det

    set cams_saved_det($slot,pitch) $::cam_angles_pitch
    set cams_saved_det($slot,yaw) $::cam_angles_yaw
    set cams_saved_det($slot,roll) $::cam_angles_roll

    set cams_saved_det($slot,x) $::cam_pos_x
    set cams_saved_det($slot,y) $::cam_pos_y
    set cams_saved_det($slot,z) $::cam_pos_z
}

proc cams_move {part} {
    global cams_totalmoves
    set cams_totalmoves $part
    cams_move_partial 0
}

# calculate the differences between current and future position
proc cams_calc_diffs {slot} {
    global cams_step
    global cams_saved_det
    set cams_step(x) [expr {$cams_saved_det($slot,x) - $::cam_pos_x}]
    set cams_step(y) [expr {$cams_saved_det($slot,y) - $::cam_pos_y}]
    set cams_step(z) [expr {$cams_saved_det($slot,z) - $::cam_pos_z}]

    # get the shortest of the two possible ways: CW/CCW
    set cams_step(yaw) [expr {$cams_saved_det($slot,yaw) - $::cam_angles_yaw}]
    if {$cams_step(yaw) > 180} {
        set cams_step(yaw) [expr { $cams_step(yaw) - 360}]
    } elseif {$cams_step(yaw) < -180} {
        set cams_step(yaw) [expr { $cams_step(yaw) + 360}]
    }
    set cams_step(pitch) [expr {$cams_saved_det($slot,pitch) - $::cam_angles_pitch}]
    set cams_step(roll) [expr {$cams_saved_det($slot,roll) - $::cam_angles_roll}]
}

# cams_setp
proc cams_calc_divs {moveframes} {
    global cams_step
    set cams_step(x) [expr {$cams_step(x) / $moveframes}]
    set cams_step(y) [expr {$cams_step(y) / $moveframes}]
    set cams_step(z) [expr {$cams_step(z) / $moveframes}]
    set cams_step(yaw) [expr {$cams_step(yaw) / $moveframes}]
    set cams_step(pitch) [expr {$cams_step(pitch) / $moveframes}]
}

proc cams_move_partial {part} { ;# input: part = previous move number
    global cams_totalmoves   ;# how many moves we will take
    global cams_moveframes   ;# how many frames will the move take (todo)
    global cams_cappedframes ;# how many frames we have already passed
    incr part                ;# go to next move (this proc is first called with part = 0)
    
    if {$part > $cams_totalmoves} {
        cmd alias f_captureframe ""
    } else {
        cams_calc_diffs $part
        cams_calc_divs $cams_moveframes
        set cams_cappedframes 0
        cmd alias f_captureframe cams_moveframe $part
    }
}

# happens in each frame
alias cams_moveframe {part} {
    global cams_cappedframes ;# how many frames we have already captured on this move
    global cams_moveframes   ;# how many frames we should capture on this move
    global cams_step         ;# contains step info (x,y,z,pitch,yaw,roll)
    
    set cams_tx [expr {$::cam_pos_x + $cams_step(x)}]
    set cams_ty [expr {$::cam_pos_y + $cams_step(y)}]
    set cams_tz [expr {$::cam_pos_z + $cams_step(z)}]
    cmd cam_pos $cams_tx $cams_ty $cams_tz
    set cams_tw [expr {$::cam_angles_yaw + $cams_step(yaw)}]
    set cams_tp [expr {$::cam_angles_pitch + $cams_step(pitch)}]
    cmd cam_angles $cams_tp $cams_tw 0
    incr cams_cappedframes
    if {$cams_cappedframes > $cams_moveframes} {
        cams_move_partial $part
    }
}


proc cams_clear {} {
    cmd unset_re cam_angles.
}
