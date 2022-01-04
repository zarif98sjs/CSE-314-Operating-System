#!/bin/bash

if [ -z "$1" ]
    then
        echo "Please give an input file where you mention the type of files you want to ignore"
        echo "Usage :"
        echo "./1705010.sh INPUT_FILE_NAME YOUR_WORKING_DIRECTORY"
        exit
fi

if [ -z "$2" ] ; then
    echo "Working directory not given as argument. Making the current directory as working directory."
    DIR="."
else
    DIR=$2
fi

INPUT_FILE=$1

OUTPUT_DIR="output_dir"
rm -rf $OUTPUT_DIR
rm -rf output.csv

IGNORE_VAR=$(cut -d' ' -f1 "$INPUT_FILE")

fileTypeArray=$(find $DIR -type f | grep -oE '\.(\w+)$' | sort -u| cut -c 2-)

containsElement(){
    for type in $2
    do
        type=$(echo "$type"|tr -d '\r')
        if [ "$type" = "$1" ]; then
            return 0 ## true
        fi
    done
    return 1 ## false
}

mkdir -p $OUTPUT_DIR
printf '%s\n' "file_type" "no_of_files" | paste -sd ',' >> output.csv

ignore_count=0

for file in $fileTypeArray
do
    if ! containsElement "$file" "$IGNORE_VAR"; then
        # echo "$file"
        mkdir -p $OUTPUT_DIR/"$file"
        FILES=$(find "$DIR" -type f -name "*.$file")
        count=0
        for name in $FILES
        do
            bn=$(basename "$name")
            if ! test -f "$OUTPUT_DIR/$file/$bn"; then ## check if already exists
                count=$((count+1))
                # echo "$bn copied"
                cp "$name" $OUTPUT_DIR/"$file"
                echo "$name" >> $OUTPUT_DIR/"$file"/"desc_$file.txt"
            fi
        done
    printf '%s\n' "$file" "$count" | paste -sd ',' >> output.csv
    else 
        # echo "$file"
        FILES=$(find "$DIR" -type f -name "*.$file")
        for name in $FILES
        do
            ignore_count=$((ignore_count+1))
        done
    fi
done

mkdir -p $OUTPUT_DIR/others

FILES=$(find $DIR -type f ! -name "*.*")
count=0
for name in $FILES
do
    bn=$(basename "$name")
    if ! test -f "$OUTPUT_DIR/others/$bn"; then ## check if already exists
        count=$((count+1))
        # echo "$bn copied"
        cp "$name" $OUTPUT_DIR/others
        echo "$name" >> $OUTPUT_DIR/others/"desc_$file.txt"
    fi
done

printf '%s\n' "others" "$count" | paste -sd ',' >> output.csv
printf '%s\n' "ignored" "$ignore_count" | paste -sd ',' >> output.csv
