./rats -x --database rats-c.xml --warning 2 --language c --html ../*.c > rats_all.html
./rats -x --database rats-c.xml --warning 2 --language c ../*.c > rats_all.txt
diff rats_base.txt rats_all.txt > rats_new.txt
