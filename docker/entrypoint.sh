#!/bin/sh

./PotreeConverter --help

FILES=$(find /data/input -regex '.*la[sz]$' -type f)

for FILE in $FILES; do
    echo "Processing file: $FILE"
    FILENAME=$(echo $FILE | sed 's|.*\/\(.*\)\.la[sz]$|\1|g')
    echo "Found filename: $FILENAME"
    echo "Overwrite: $OVERWRITE"
    if [[ -d /data/output/$FILENAME ]] &&
        [ "$OVERWRITE" != "TRUE" ]; then
        echo "Found existing output folder and overwrite disabled, skipping..."
        continue
    fi

    echo "Encoding: $POTREE_ENCODING"
    echo "Method: $POTREE_METHOD"
    echo "Extra options: $POTREE_EXTRA_OPTIONS"

    ./PotreeConverter \
        --source $FILE \
        --outdir /data/output/$FILENAME \
        --generate-page $FILENAME \
		--title $FILENAME \
        --encoding $POTREE_ENCODING \
        --method $POTREE_METHOD \
        $POTREE_EXTRA_OPTIONS
done

echo "Finished converting all files"

if [[ -f /data/output/index.html ]] &&
    [ "$REMOVE_INDEX" != "TRUE" ]; then
    echo "Removing index.html"
    rm -f /data/output/index.html
fi

echo "Done"

exec "$@"
