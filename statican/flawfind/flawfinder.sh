echo -
echo - Generating list of new flaws
echo -

./flawfinder --html --omittime --quiet --diffhitlist=lists/flaws_base.list ../../*.c > lists/flaws_new.html

echo done!
echo lists/flaws_new.html contains list of newly found flaws
