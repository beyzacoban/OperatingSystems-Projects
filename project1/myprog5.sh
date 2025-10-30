#!/bin/bash
# This script renames all .txt files in a given directory by adding a prefix.
# It takes 2 arguments: <directory_name> <prefix>
# If subdirectories are found, it asks the user whether to rename
# files inside them (recursively) or not.
dir=$1
prfx=$2
renamed_files=()
# Check for 2 argument
if [ $# != 2 ];then
   echo "Invalid number of arguments.Script should be like: ./myprog5.sh directory_name prefix"
   exit 1
fi   
# Check if directory exists
if [ ! -d $dir ];then
   echo "There is no directory with this name."
   exit 1
fi
#If there is/are subdirector(y/ies) then ask the user whether to rename files recursively.
if find "$dir" -mindepth 1 -type d | grep -q .; then
    read -p "There are subdirectories. Do you wanna rename files into subdirectories? (y/n): " choice
    if [[ "$choice" == "y" || "$choice" == "Y" ]]; then
        recursive=true
    else
        recursive=false
    fi
else
    recursive=false
fi

#If the user accept to rename files recursively, then find all .txt files.Else don't include the files in subdirectories.
if [ "$recursive" = true ]; then
    find_cmd=(find "$dir" -type f -name "*.txt")
else
    find_cmd=(find "$dir" -maxdepth 1 -type f -name "*.txt")
fi


count=0 #to write total renamed files

#map the files to files array from find_cmd 
mapfile -t files < <("${find_cmd[@]}")

#rename files through mv command
for file in "${files[@]}"; do
    #directory name of file(main/subdirect/) ,to place the file correctly
    dirname=$(dirname "$file") 
    echo "$dirname"  
    basename=$(basename "$file")  
    newname="${dirname}/${prfx}${basename}"
    mv "$file" "$newname"
    renamed_files+=("${prfx}${basename}")
    ((count++))
done

echo "$count file(s) renamed: ${renamed_files[*]}"


