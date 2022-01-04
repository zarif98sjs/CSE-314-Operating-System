# find working_dir -type f -exec cp '{}' dummy \; # copies all files

# containsElement () {
#   local e match="$1"
#   shift
#   for e; do [[ "$e" == "$match" ]] && return 0; done
#   return 1
# }

# n=1
# while IFS="" read -r p || [ -n "$p" ]
# do
#     echo $n
#     p=$(echo $p|tr -d '\r')
#     ARRAY[$n]=$p
#     n=$((n+1))
# done < input.txt

IGNORE_VAR=$(cut -d' ' -f1 input.txt)

fileTypeArray=$(find working_dir -type f | grep -oE '\.(\w+)$' | sort -u| cut -c 2-)

containsElement(){
    for type in $2
    do
        type=$(echo $type|tr -d '\r')
        # echo $type
        if [ "$type" = "$1" ]; then
            echo Now $type
            return 0 ## true
            # mkdir -p dummy/$file
            # find working_dir -type f -name "*.$file" -exec cp '{}' dummy/$file \;
        fi
    done
    return 1 ## false
}
# echo `containsElement "mp3" "$IGNORE_VAR"`
for file in $fileTypeArray
do
    if ! containsElement "$file" "$IGNORE_VAR"; then
        echo "$file"
        mkdir -p dummy/$file
        find working_dir -type f -name "*.$file" -exec cp '{}' dummy/$file \;
    fi
done

#  find working_dir -type f -name "*.pdf" -exec cp '{}' dummy/pdf \;