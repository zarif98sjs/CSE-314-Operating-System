find . -depth -name '* *' \
| while IFS= read -r f ; do 
	chmod uao-wx "$f"
	chmod u+w "$f"
 done
