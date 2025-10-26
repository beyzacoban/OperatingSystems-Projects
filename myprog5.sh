#!/bin/bash
dir=$1
prfx=$2
renamed_files=()

if [ $# != 2 ];then
   echo "Invalid number of arguments.Script should be like: ./myprog5.sh directory_name prefix"
   exit 1
fi   

if [ ! -d $dir ];then
   echo "There is no directory with this name."
   exit 1
fi

if find "$dir" -mindepth 1 -type d | grep -q .; then
    read -p "THere are subdirectories. Do you wanna rename files into subdirectories? (y/n): " choice
    if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
        recursive=true
    else
        recursive=false
    fi
else
    recursive=false
fi


if [ "$recursive" = true ]; then
    find_cmd=(find "$dir" -type f -name "*.txt")
else
    find_cmd=(find "$dir" -maxdepth 1 -type f -name "*.txt")
fi


count=0
mapfile -t files < <("${find_cmd[@]}")

for file in "${files[@]}"; do
    dirname=$(dirname "$file")
    basename=$(basename "$file")
    newname="${dirname}/${prfx}${basename}"
    mv "$file" "$newname"
    renamed_files+=("${prfx}${basename}")
    ((count++))
done

echo "$count file(s) renamed: ${renamed_files[*]}"


