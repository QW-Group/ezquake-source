echo -
echo - Generating new base list of flaws
echo -

./flawfinder --html --omittime --quiet --dataonly --savehitlist=lists/flaws_base.list ../../*.c > lists/flaws_base.html
 
echo done!
echo lists/flaws_base.html contains list of all flaws
