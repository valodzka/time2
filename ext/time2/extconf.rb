require 'mkmf'

# allow use old ruby time in parallel with new
# adds aditional checks and convertation
$defs << "-DOLD_TIME_COMPAT" 

create_makefile("time2")
