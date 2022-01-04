#!/bin/bash

rm -rf dummy

IGNORE_VAR=$(cut -d' ' -f1 input.txt)

fileTypeArray=$(find working_dir -type f | grep -oE '\.(\w+)$' | sort -u| cut -c 2-)

containsElement(){
    for type in $2
    do
        type=$(echo $type|tr -d '\r')
        if [ "$type" = "$1" ]; then
            return 0 ## true
        fi
    done
    return 1 ## false
}

for file in $fileTypeArray
do
    if ! containsElement "$file" "$IGNORE_VAR"; then
        echo "$file"
        mkdir -p dummy/$file
        find working_dir -type f -name "*.$file" -exec cp '{}' dummy/$file \;
        find working_dir -type f -name "*.$file" > dummy/$file/"desc_$file.txt"
    fi
done

mkdir -p dummy/others
find working_dir -type f ! -name "*.*" -exec cp '{}' dummy/others \;
find working_dir -type f ! -name "*.*" > dummy/others/"desc_others.txt"