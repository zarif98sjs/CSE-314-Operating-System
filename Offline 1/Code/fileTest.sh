#!/bin/bash

FILES=$(find "working dir" -type f -name "*.pdf")

for f in $FILES
do
    echo $f
done

# FILES=`find working_dir -type f -name "*.pdf"`
# echo `expr length "$FILES"`

# for i in {0..5}; do printf '%s\n' {A..C} | paste -sd " " >> file.csv; don
# printf '%s\n' file_type no_of_files | paste -sd ',' > file.csv


# rm -rf dummy
# file=pdf
# mkdir -p dummy/$file
# FILES=`find working_dir -type f -name "*.$file"`
# for name in $FILES
# do
#     bn=`basename $name`
#     if ! test -f "dummy/$file/$bn"; then
#         echo "$bn copied"
#         cp "$name" dummy/"$file"
#         echo "$name" >> dummy/$file/"desc_$file.txt"
#     fi
# done


# find working_dir -type f -name "*.pdf" > desc_pdf.txt
# find working_dir -type f -name "*.pdf" 

# # while IFS= read -r line; do
# #     echo "Text read from file: $line"
# # done < input.txt

# n=1
# while IFS="" read -r p || [ -n "$p" ]
# do
#     # echo $n
#     ARRAY[$n]=$p
#     n=$((n+1))
# done < input.txt

# VAR=$(cut -d' ' -f1 input.txt)

# for x in $VAR
# do
#     x=$(echo $x|tr -d '\r')
#     echo $x
#     if ! [ "$x" = "mp3" ]; then
#         echo yes
#     fi
# done
