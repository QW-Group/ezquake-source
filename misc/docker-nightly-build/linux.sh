OUTPUTDIR=/where/you/want/the/linux/binary/to/go
docker run -v $OUTPUTDIR:/nightly localghost/ezquake_nightly sh -c "cd ezquake-source; git pull; make; cp ezquake-linux-x86_64 /nightly/; make clean"
