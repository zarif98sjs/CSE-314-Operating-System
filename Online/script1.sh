#!/bin/bash

idx=0
while read line; do
    ARA[$idx]=$line
    idx=$((idx+1))
done < in.txt

for x in "${ARA[@]}"
do 
    echo $x
done

echo "${#ARA[@]}"