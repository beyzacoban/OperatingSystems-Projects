#!/bin/bash
file=$1
old_word=$2
new_word=$3

if [ $# -ne 3 ];then
   echo "Invalid number of arguments.Script should be like <txt_file_name> <old_word> <new_word>"
   exit 1
fi   

if [ ! -f "$file" ]; then 
   echo "File could not find."
   exit 1
fi

ext="${file##*.}"
name="${file%.*}"
new_file="modified_$name.$ext"

#if file doesn't exits then create,else clear the file.
if [ ! -f "$new_file" ]; then
   touch "$new_file"
else
   > "$new_file"
fi

count=0

while read -r line; do
  for word in $line; do
    if [[ "$word" == *"$old_word"* ]]; then
       word=${word//$old_word/$new_word}
       count=$((count+1))
    fi
    echo -n "$word " >> "$new_file"
  done
  echo >> "$new_file"
done < "$file"

echo "All $count occurrences of '$old_word' replaced with '$new_word'" 
echo "Modified file: $new_file"
log="changes.log"
timestamp=$(date +"%Y-%m-%d %T")
echo "[$timestamp] All $count occurrences of '$old_word' replaced with '$new_word'" >> "$log"
echo "Logged to:$log"


