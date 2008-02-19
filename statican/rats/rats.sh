echo - Generating HTML list of all rats
./rats -x --database lists/rats-c.xml --warning 2 --language c --html ../../*.c > lists/rats_all.html
echo done
echo
echo - Generating TXT list of all rats
./rats -x --database lists/rats-c.xml --warning 2 --language c ../../*.c > lists/rats_all.txt
echo done
echo
echo - Generating difference file 
diff lists/rats_base.txt lists/rats_all.txt > lists/rats_new.txt
echo done
echo lists/rats_new.txt contains the difference between the base and the new list of rats
