
## Outline

On Ubuntu, PotreeConverter requires CMake and compilation (as indicated in the ["getting started"](https://github.com/potree/PotreeConverter)) and setting it up properly requires a few tricks.

To simplify it all, the docker image in this folder handles PotreeConverter compilation & setup as described below and in [issue 473](https://github.com/potree/PotreeConverter/issues/473#issuecomment-732075183).

### Building the docker container

Build the docker container (may take some time):
```
$ docker build . -t potreeconverter --network host
```

### Automatically potreeConvert a folder of las files

Run the docker container with your las files' folder mounted inside (adapt 'path/to/lasfile/folder' to the path to your las files folder):
```
$ docker run -it --rm -v 'path/to/lasfile-folder':/lasfiles potreeconverter 
```
This will automatically convert all the las files in your `path/to/lasfile-folder/` in the folder `path/to/lasfile-folder/potree/` with one potree octree per las file.

Do not forget to chown the resulting files (see below).

### chown Potree output
Once the conversion is done, don't forget to `chown` the output files so you're the owner:
```
$ sudo chown -R dddpt lasfiles/potree/
```

### Merge and convert a folder of las files

If you want to merge multiple las files in a single potree octree, comment the following line in the `Dockerfile`:
```
CMD for f in lasfiles/*.las; do  echo "PotreeConverting $f"; PotreeConverter $f -o "lasfiles/potree/$(basename "$f" .las)"; done
```
...and uncomment this one:
```
CMD PotreeConverter lasfiles/*.las -o lasfiles/potree/merged
```
re-build the docker image and run it as before and all the las files will be merged in a single potree octree in `path/to/lasfile-folder/potree/merged`.

Do not forget to chown the resulting files (see above).

### PotreeConvert las files by hand inside the docker

If you want to handpick the las files you want to convert, do:
```
$ docker run -it --rm -v 'path/to/lasfile-folder':/lasfiles potreeconverter bash
```

From inside the container, merge & potree-convert (adapt filenames), and then exit the container:
```
# PotreeConverter lasfiles/loop-X.las -o lasfiles/potree-loop-X
# exit
```
To potreeConvert multiple files from a file-list lasfiles_list.txt without merging them do:
```
# for f in $(cat lasfiles_list.txt); do  echo "PotreeConverting $f"; PotreeConverter $f -o "lasfiles/potree/$(basename "$f" .las)"; done
# exit
```
Do not forget to chown the resulting files (see above).


## Versioning notes

Since they work, versions of some of the utilities/binaries used in this docker have been freezed.
As of 23.11.2020, those are:
- cmake 3.19.0
- tbb git commit https://github.com/wjakob/tbb/commit/141b0e310e1fb552bdca887542c9c1a8544d6503
- PotreeConverter 2.0.2, git commit da93ec26411b82ffbf799daf5ac56608ef988bf8

Feel free to update them to more recent versions, but not guarantee it will still work.

## Merging las files without potree conversion

Use lastools: https://github.com/LAStools/LAStools
```
$ git clone https://github.com/LAStools/LAStools.git
$ cd LAStools
$ make
$ cd bin/
$ ./lasmerge -i input_las_files -o output_las_file
```