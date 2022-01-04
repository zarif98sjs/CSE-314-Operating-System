#!/bin/bash

mapfile ARA < in.txt
echo "${ARA[@]}"

mapfile < in.txt
echo "${MAPFILE[@]}"

# mapfile < in.txt
# echo "${ARA[@]]}"
# declare -A ASOC_ARA
# ASOC_ARA[a]=1
# ASOC_ARA[b]=2
# ASOC_ARA+=([c]=3 [d]=4)

# echo ${#ASOC_ARA[@]} ## length

# for x in ${ASOC_ARA[@]}
# do
#     echo $x ## value
# done

# for x in ${!ASOC_ARA[@]}
# do
#     echo $x ## key
# done

# for i in $(ls)
# do
# echo $i
# done

# for x in "$@"
# do
#     echo "$x"
# done
# #!/bin/sh
# echo "Is it morning? Please answer yes or no"
# read timeofday
# case "$timeofday" in
#     y*) echo “Good Morning”;;
#     n* ) echo “Good Afternoon”;;
#     * ) echo “Sorry, answer not recognized”;;
# esac

# for (( i=0 ; i<5 ; i++ ))
# do
#   echo "$i"
# done

# n=0
# if (("$n" > 0))
# then
#     echo greather than 0
# elif (("$n" < 0))
#   then 
#     echo less than 0
# else
#   echo equals to 0
# fi

# while IFS="" read -r line || [ -n "$line" ]
# do
#     echo "$line"
# done < in.txt