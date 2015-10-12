wget http://uttergrottan.localghost.net/ezquake/dev/libs/mingw32-libs.tar.gz
docker build -t localghost/ezquake_nightly .
rm mingw32-libs.tar.gz
