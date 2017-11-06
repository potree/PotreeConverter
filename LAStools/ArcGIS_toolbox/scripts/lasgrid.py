#
# lasgrid.py
#
# (c) 2013, martin isenburg - http://rapidlasso.com
#     rapidlasso GmbH - fast tools to catch reality
#
# uses lasgrid.exe to grid a LiDAR file
#
# LiDAR input:   LAS/LAZ/BIN/TXT/SHP/BIL/ASC/DTM
# raster output: BIL/ASC/IMG/TIF/DTM/PNG/JPG
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
gp.AddMessage("Starting lasgrid ...")

### get number of arguments
argc = len(sys.argv)

### report arguments (for debug)
#gp.AddMessage("Arguments:")
#for i in range(0, argc):
#    gp.AddMessage("[" + str(i) + "]" + sys.argv[i])

### get the path to LAStools
lastools_path = os.path.dirname(os.path.dirname(os.path.dirname(sys.argv[0])))

### make sure the path does not contain spaces
if lastools_path.count(" ") > 0:
    gp.AddMessage("Error. Path to .\\lastools installation contains spaces.")
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

### create the full path to the lasgrid executable
lasgrid_path = lastools_path+"\\lasgrid.exe"

### check if executable exists
if os.path.exists(lastools_path) == False:
    gp.AddMessage("Cannot find lasgrid.exe at " + lasgrid_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lasgrid_path + " ...")

### create the command string for lasgrid.exe
command = ['"'+lasgrid_path+'"']

### maybe use '-verbose' option
if sys.argv[argc-1] == "true":
    command.append("-v")

### add input LiDAR
command.append("-i")
command.append('"'+sys.argv[1]+'"')

### maybe use a user-defined step size
if sys.argv[2] != "1":
    command.append("-step")
    command.append(sys.argv[2].replace(",","."))

### what should we raster
if sys.argv[3] != "elevation":
    command.append("-" + sys.argv[3])
        
### what operation should we use
if sys.argv[4] != "lowest":
    command.append("-" + sys.argv[4])

### should we fill a few pixels
if sys.argv[5] != "0":
    command.append("-fill")
    command.append(sys.argv[5])

### what should we output
if sys.argv[6] == "gray ramp":
    command.append("-gray")
elif sys.argv[6] == "false colors":
    command.append("-false")

### do we have a min max value for colors
if (sys.argv[6] == "gray ramp") or (sys.argv[6] == "false colors"):
    if (sys.argv[7] != "#") and (sys.argv[8] != "#"):
        command.append("-set_min_max")
        command.append(sys.argv[7].replace(",","."))
        command.append(sys.argv[8].replace(",","."))

### what should we triangulate
if sys.argv[9] == "ground points only":
    command.append("-keep_class")
    command.append("2")
elif sys.argv[9] == "ground and keypoints":
    command.append("-keep_class")
    command.append("2")
    command.append("8")
elif sys.argv[9] == "ground and buildings":
    command.append("-keep_class")
    command.append("2")
    command.append("6")
elif sys.argv[9] == "last return only":
    command.append("-last_only")
elif sys.argv[9] == "first return only":
    command.append("-first_only")

### should we use the bounding box
if sys.argv[10] == "true":
    command.append("-use_bb")

### should we use the tile bounding box
if sys.argv[11] == "true":
    command.append("-use_tile_bb")

### this is where the output arguments start
out = 12

### maybe an output format was selected
if sys.argv[out] != "#":
    command.append("-o" + sys.argv[out])

### maybe an output file name was selected
if sys.argv[out+1] != "#":
    command.append("-o")
    command.append('"'+sys.argv[out+1]+'"')
    
### maybe an output directory was selected
if sys.argv[out+2] != "#":
    command.append("-odir")
    command.append('"'+sys.argv[out+2]+'"')

### maybe an output appendix was selected
if sys.argv[out+3] != "#":
    command.append("-odix")
    command.append('"'+sys.argv[out+3]+'"')

### maybe there are additional command-line options
if sys.argv[out+4] != "#":
    additional_options = sys.argv[out+4].split()
    for option in additional_options:
        command.append(option)

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

### report output of lasgrid
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. lasgrid failed.")
    sys.exit(1)

### report happy end
gp.AddMessage("Success. lasgrid done.")
