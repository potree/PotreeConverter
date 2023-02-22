# PotreeConverter docker 

Minimal customise docker image for PotreeConverter

## How to build the image

Build the docker container (may take some time):
```
$ cd docker
$ docker build --pull --rm -f Dockerfile -t potree-converter:latest --build-arg VERSION=0.0.0 .
```

## Usage

1. Pull the image from local or from the registry below

```
$ docker pull ghcr.io/thomas-tran/potree-converter-docker:latest
```

2. Create input and out directories

```
$ mkdir input output
```

3. Mount the input directory containing .laz or .las file to /data/input. Mount the empty output directory to /data/output and run the command below



```
$ docker run -it --rm -u $UID -v $PWD/input:/data/input:ro -v $PWD/output:/data/output:rw potree-converter:latest
```

4. Alternatively with environment variables

```
$ docker run -it --rm \
    -u $UID \
    -v $PWD/input:/data/input:ro \
    -v $PWD/output:/data/output:rw \
    --env POTREE_ENCODING=UNCOMPRESSED \
    --env POTREE_METHOD=poisson \
    --env POTREE_EXTRA_OPTIONS="--projection" \
    --env OVERWRITE=FALSE \
    potree-converter:latest
```

| Variable name          | Default value  | More info                                                                  |
| ---------------------- | -------------- | -------------------------------------------------------------------------- |
| `POTREE_ENCODING`      | `UNCOMPRESSED` | Other options: `BROTLI`                                                    |
| `POTREE_METHOD`        | `poisson`      | Other options: `poisson_average`, `random`                                 |
| `POTREE_EXTRA_OPTIONS` | `""`           | Check potree documentation for other available command line options        |
| `OVERWRITE`            | `TRUE`         | Change to anything else to disable converting already converted clouds     |
| `REMOVE_INDEX`         | `FALSE`        | Remove index.html from output dir. Useful for removing default apache file |