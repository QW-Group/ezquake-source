for source in *.txt; do
  cat $source | grep -v -E -e "^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+:[0-9]{5}$" 
done

