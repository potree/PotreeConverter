#
# lasvalidate.py
#
# (c) 2013, martin isenburg - http://rapidlasso.com
#     rapidlasso GmbH - fast tools to catch reality
#
# uses lasvalidate.exe to output info or repair the bounding box
#
# LiDAR input:   LAS or LAZ
# report output: XML
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
gp.AddMessage("Starting lasvalidate ...")

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

### create the full path to the lasvalidate executable
lasvalidate_path = lastools_path+"\\lasvalidate.exe"

### check if executable exists
if os.path.exists(lastools_path) == False:
    gp.AddMessage("Cannot find lasvalidate.exe at " + lasvalidate_path)
    sys.exit(1)
else:
    gp.AddMessage("Found " + lasvalidate_path + " ...")

### create the command string for lasvalidate.exe
command = ['"'+lasvalidate_path+'"']

### maybe use '-verbose' option
if sys.argv[argc-1] == "true":
    command.append("-v")

### counting up the arguments
c = 1

### add input LiDAR
command.append("-i")
command.append('"' + sys.argv[c] + '"')
c = c + 1

### maybe output the default name
if sys.argv[c] == "true":
    command.append("-oxml")

### maybe an output file name was selected
elif sys.argv[c+1] != "#":
    command.append("-o")
    command.append('"'+sys.argv[c+1]+'"')
c = c + 2

### maybe there are additional input options
if sys.argv[c] != "#":
    additional_options = sys.argv[c].split()
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

### report output of lasvalidate
gp.AddMessage(str(output))

### check return code
if returncode != 0:
    gp.AddMessage("Error. lasvalidate failed.")
    sys.exit(1)

### report happy end
gp.AddMessage("Success. lasvalidate done.")
