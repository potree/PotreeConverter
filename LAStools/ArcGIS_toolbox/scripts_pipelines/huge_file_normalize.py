#
# huge_file_normalize.py
#
# (c) 2014, martin isenburg - http://rapidlasso.com
#     rapidlasso GmbH - fast tools to catch reality
#
# This LAStools pipeline height-normalizes very large LAS
# or LAZ files by operating with a tile-based multi-core
# pipeline. The input file is first tiled using lastile
# with the specified tile size and the specified buffer
# around each tile to avoid edge artifacts. These tiles
# are then ground-classified using lasground on as many
# cores as specified. The tiles are then height-normalized
# using lasheight with optional removal of points that
# are too high above or too far below the ground points.
# Finally the height-normalized tiles are rejoined back
# into a single file with points in their original order
# and all temporary files are deleted.
#
# LiDAR input:   LAS/LAZ/BIN/TXT/SHP/BIL/ASC/DTM
# LiDAR output:  LAS/LAZ/BIN/TXT
#
# for licensing see http://lastools.org/LICENSE.txt
#

import sys, os, arcgisscripting, subprocess

def check_output(command,console):
    if console == True:
        process = subprocess.Popen(command)
    else:
        process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True)
    output,error = process.communicate()
    returncode = process.poll()
    return returncode,output 

### create the geoprocessor object
gp = arcgisscripting.create(9.3)

### report that something is happening
gp.AddMessage("Starting huge_file_normalize ...")

### define positions of arguments in argv array
arg_input_file     =  1
arg_tile_size      =  2
arg_buffer         =  3
arg_terrain_type   =  4
arg_granularity    =  5
arg_drop_above     =  6
arg_drop_below     =  7
arg_cores          =  8
arg_empty_temp_dir =  9
arg_output_file    = 10
arg_output_format  = 11
arg_verbose        = 12
arg_count_needed   = 13

### get number of arguments
argc = len(sys.argv)

### make sure we have right number of arguments
if argc != arg_count_needed:
    gp.AddMessage("Error. Wrong number of arguments. Got " + str(argc) + " expected " + str(arg_count_needed))
    sys.exit(1)    

### report arguments (for debug)
#gp.AddMessage("Arguments:")
#for i in range(0, argc):
#    gp.AddMessage("[" + str(i) + "]" + sys.argv[i])

### get selected arguments
empty_temp_dir = sys.argv[arg_empty_temp_dir]

### get the path to LAStools
lastools_path = os.path.dirname(os.path.dirname(os.path.dirname(sys.argv[0])))

### make sure the path does not contain spaces
if lastools_path.count(" ") > 0:
    gp.AddMessage("Error. Path to .\\lastools installation contains spaces.")
    gp.AddMessage("This does not work: " + lastools_path)
    gp.AddMessage("This would work:    C:\\software\\lastools")
    sys.exit(1)    

### make sure the path does not contain open or closing brackets
if (lastools_path.count("(") > 0) or (lastools_path.count(")") > 0):
    gp.AddMessage("Error. Path to .\\lastools installation contains brackets.")
    gp.AddMessage("This does not work: " + lastools_path)
    gp.AddMessage("This would work:    C:\\software\\lastools")
    sys.exit(1)   

### complete the path to where the LAStools executables are
lastools_path = lastools_path + "\\bin"

### check if path exists
if os.path.exists(lastools_path) == False:
    gp.AddMessage("Cannot find .\\lastools\\bin at " + lastools_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lastools_path + " ...")

### create the full path to the lastile executable
lastile_path = lastools_path+"\\lastile.exe"

### check if the lastile executable exists
if os.path.exists(lastile_path) == False:
    gp.AddMessage("Cannot find lastile.exe at " + lastile_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lastile_path + " ...")

### create the full path to the lasground executable
lasground_path = lastools_path+"\\lasground.exe"

### check if the lasground executable exists
if os.path.exists(lasground_path) == False:
    gp.AddMessage("Cannot find lasground.exe at " + lasground_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lasground_path + " ...")

### create the full path to the lasheight executable
lasheight_path = lastools_path+"\\lasheight.exe"

### check if the lasheight executable exists
if os.path.exists(lasheight_path) == False:
    gp.AddMessage("Cannot find lasheight.exe at " + lasheight_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lasheight_path + " ...")

### check if the empty temp directory exists
if os.path.exists(empty_temp_dir) == False:
    gp.AddMessage("Cannot find empty temp dir " + empty_temp_dir)
    sys.exit(1)
else:
    gp.AddMessage("Found " + empty_temp_dir + " ...")

### make sure the empty temp directory is emtpy
if os.listdir(empty_temp_dir) != []:
    gp.AddMessage("Empty temp directory '" + empty_temp_dir + "' is not empty")
    sys.exit(1)
else:
    gp.AddMessage("And it's empty ...")

###################################################
### first step: tile huge input file
###################################################

### create the command string for lastile.exe
command = ['"'+lastile_path+'"']

### maybe use '-verbose' option
if sys.argv[arg_verbose] == "true":
    command.append("-v")

### add input LiDAR
command.append("-i")
command.append('"'+sys.argv[arg_input_file]+'"')

### maybe use a user-defined tile size
if sys.argv[arg_tile_size] != "1000":
    command.append("-tile_size")
    command.append(sys.argv[arg_tile_size].replace(",","."))

### maybe create a buffer around the tiles
if sys.argv[arg_buffer] != "0":
    command.append("-buffer")
    command.append(sys.argv[arg_buffer].replace(",","."))

### make tiling reversible
command.append("-reversible")

### maybe an output directory was selected
if empty_temp_dir != "#":
    command.append("-odir")
    command.append('"'+empty_temp_dir+'"')

### give temporary tiles a meaningful name
command.append("-o")
command.append("temp_huge_normalize.laz")

### store temporary tiles in compressed format
command.append("-olaz")

### report command string
gp.AddMessage("LAStools command line:")
command_length = len(command)
command_string = str(command[0])
command[0] = command[0].strip('"')
for i in range(1, command_length):
    command_string = command_string + " " + str(command[i])
    command[i] = command[i].strip('"')
gp.AddMessage(command_string)

### run command
returncode,output = check_output(command, False)

### report output of lastile
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. huge_file_normalize failed in lastile step.")
    sys.exit(1)

### report success
gp.AddMessage("lastile step done.")

###################################################
### second step: ground classify each tile
###################################################

### create the command string for lasground.exe
command = ['"'+lasground_path+'"']

### maybe use '-verbose' option
if sys.argv[arg_verbose] == "true":
    command.append("-v")

### add input LiDAR
command.append("-i")
if empty_temp_dir != "#":
    command.append('"'+empty_temp_dir+"\\temp_huge_normalize*.laz"+'"')
else:
    command.append("temp_huge_normalize*.laz")

### what type of terrain do we have
if sys.argv[arg_terrain_type] == "wilderness":
    command.append("-wilderness")
elif sys.argv[arg_terrain_type] == "city or warehouses":
    command.append("-city")
elif sys.argv[arg_terrain_type] == "towns or flats":
    command.append("-town")
elif sys.argv[arg_terrain_type] == "metropolis":
    command.append("-metro")

### what granularity should we operate with
if sys.argv[arg_granularity] == "fine":
    command.append("-fine")
elif sys.argv[arg_granularity] == "extra fine":
    command.append("-extra_fine")
elif sys.argv[arg_granularity] == "ultra fine":
    command.append("-ultra_fine")

### give ground-classified tiles a meaningful appendix
command.append("-odix")
command.append("_g")

### store ground-classified tiles in compressed format
command.append("-olaz")

### maybe we should run on multiple cores
if sys.argv[arg_cores] != "1":
    command.append("-cores")
    command.append(sys.argv[arg_cores])

### report command string
gp.AddMessage("LAStools command line:")
command_length = len(command)
command_string = str(command[0])
command[0] = command[0].strip('"')
for i in range(1, command_length):
    command_string = command_string + " " + str(command[i])
    command[i] = command[i].strip('"')
gp.AddMessage(command_string)

### run command
returncode,output = check_output(command, False)

### report output of lasground
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. huge_file_normalize failed in lasground step.")
    sys.exit(1)

### report success
gp.AddMessage("lasground step done.")

###################################################
### third step: height-normalize (maybe drop points)
###################################################

### create the command string for lasheight.exe
command = ['"'+lasheight_path+'"']

### maybe use '-verbose' option
if sys.argv[arg_verbose] == "true":
    command.append("-v")

### add input LiDAR
command.append("-i")
if empty_temp_dir != "#":
    command.append('"'+empty_temp_dir+"\\temp_huge_normalize*_g.laz"+'"')
else:
    command.append("temp_huge_normalize*_g.laz")

### height normalize
command.append("-replace_z")

### maybe we should drop all points above
if sys.argv[arg_drop_above] != "#":
    command.append("-drop_above")
    command.append(sys.argv[arg_drop_above].replace(",","."))

### maybe we should drop all points below 
if sys.argv[arg_drop_below] != "#":
    command.append("-drop_below")
    command.append(sys.argv[arg_drop_below].replace(",","."))

### give height-normalized tiles a meaningful appendix
command.append("-odix")
command.append("h")

### store height-normalized tiles in compressed format
command.append("-olaz")

### maybe we should run on multiple cores
if sys.argv[arg_cores] != "1":
    command.append("-cores")
    command.append(sys.argv[arg_cores])

### report command string
gp.AddMessage("LAStools command line:")
command_length = len(command)
command_string = str(command[0])
command[0] = command[0].strip('"')
for i in range(1, command_length):
    command_string = command_string + " " + str(command[i])
    command[i] = command[i].strip('"')
gp.AddMessage(command_string)

### run command
returncode,output = check_output(command, False)

### report output of lasheight
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. huge_file_normalize failed in lasheight step.")
    sys.exit(1)

### report success
gp.AddMessage("lasheight step done.")

###################################################
### fourth step: reverse the tiling
###################################################

### create the command string for lastile.exe
command = ['"'+lastile_path+'"']

### maybe use '-verbose' option
if sys.argv[arg_verbose] == "true":
    command.append("-v")

### add input LiDAR
command.append("-i")
if empty_temp_dir != "#":
    command.append('"'+empty_temp_dir+"\\temp_huge_normalize*_gh.laz"+'"')
else:
    command.append("temp_huge_normalize*_gh.laz")

### use mode reverse tiling
command.append("-reverse_tiling")

### maybe an output file name was selected
if sys.argv[arg_output_file] != "#":
    command.append("-o")
    command.append('"'+sys.argv[arg_output_file]+'"')

### maybe an output format was selected
if sys.argv[arg_output_format] != "#":
    if sys.argv[arg_output_format] == "las":
        command.append("-olas")
    elif sys.argv[arg_output_format] == "laz":
        command.append("-olaz")
    elif sys.argv[arg_output_format] == "bin":
        command.append("-obin")
    elif sys.argv[arg_output_format] == "xyzc":
        command.append("-otxt")
        command.append("-oparse")
        command.append("xyzc")
    elif sys.argv[arg_output_format] == "xyzci":
        command.append("-otxt")
        command.append("-oparse")
        command.append("xyzci")
    elif sys.argv[arg_output_format] == "txyzc":
        command.append("-otxt")
        command.append("-oparse")
        command.append("txyzc")
    elif sys.argv[arg_output_format] == "txyzci":
        command.append("-otxt")
        command.append("-oparse")
        command.append("txyzci")

### report command string
gp.AddMessage("LAStools command line:")
command_length = len(command)
command_string = str(command[0])
command[0] = command[0].strip('"')
for i in range(1, command_length):
    command_string = command_string + " " + str(command[i])
    command[i] = command[i].strip('"')
gp.AddMessage(command_string)

### run command
returncode,output = check_output(command, False)

### report output of lastile (reverse)
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. huge_file_normalize failed in lastile (reverse) step.")
    sys.exit(1)

### report success
gp.AddMessage("lastile (reverse) step done.")

###################################################
### final step: clean-up all temporary files
###################################################

### create the command string for clean-up
command = ["del"]

### add temporary files wildcard
if empty_temp_dir != "#":
    command.append('"'+empty_temp_dir+"\\temp_huge_normalize*.laz"+'"')
else:
    command.append("temp_huge_normalize*.laz")

### report command string
gp.AddMessage("clean-up command line:")
command_length = len(command)
command_string = str(command[0])
command[0] = command[0].strip('"')
for i in range(1, command_length):
    command_string = command_string + " " + str(command[i])
    command[i] = command[i].strip('"')
gp.AddMessage(command_string)

### run command
returncode,output = check_output(command, False)

### report output of clean-up
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. huge_file_normalize failed in clean-up step.")
    sys.exit(1)

### report success
gp.AddMessage("clean-up step done.")

### report happy end
gp.AddMessage("Success. huge_file_normalize done.")
