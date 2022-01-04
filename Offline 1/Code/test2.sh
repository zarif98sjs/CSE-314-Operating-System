#!/bin/bash

# ls -R *.pdf > list.txt
# find working_dir -type f -print0 | xargs -0 basename -a | sort > all_files.txt

# find working_dir -type f -exec basename '{}' \; | sort > all_files.txt ## better

find working_dir -type f | grep -oE '\.(\w+)$' | sort -u

# find working_dir -type f -name '*.pdf' -exec basename '{}' \;

list_files_of_type () {
    find working_dir -type f -name "*$1" -exec basename '{}' \;
}

# list_files_of_type ".pdf"
# list_files_of_type ".mp3"