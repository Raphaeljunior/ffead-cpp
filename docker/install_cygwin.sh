cd /tmp
wget -q https://github.com/efficient/libcuckoo/archive/master.zip
unzip -qq master.zip
rm -f master.zip
cd /tmp/libcuckoo-master
cmake -DCMAKE_INSTALL_PREFIX=/usr/local/ .
make install
cd /tmp
rm -rf libcuckoo-master

wget -q http://www.unixodbc.org/unixODBC-2.3.7.tar.gz
tar zxf unixODBC-2.3.7.tar.gz
cd unixODBC-2.3.7
./configure
make
make install
cd /tmp
rm -rf unixODBC-2.3.7

wget -q https://github.com/redis/hiredis/archive/v0.13.3.tar.gz
tar zxf v0.13.3.tar.gz
rm -f v0.13.3.tar.gz
cd hiredis-0.13.3/ && rm -f net.c && wget https://raw.githubusercontent.com/sumeetchhetri/ffead-cpp/master/docker/files/net.c && make && PREFIX=/usr make install
cd /tmp
rm -rf hiredis-0.13.3

wget -q https://github.com/sumeetchhetri/ffead-cpp/archive/master.zip
unzip -qq master.zip
mv ffead-cpp-master ffead-cpp-src
rm -f master.zip
cd /tmp/ffead-cpp-src
mkdir build
cd build
cmake -DSRV_EMB=on -DMOD_REDIS=on ..
make install -j4
mv /tmp/ffead-cpp-src/ffead-cpp-5.0-bin /tmp/
#cd /tmp/ffead-cpp-src
#chmod +x autogen.sh
#sed -i'' -e "s|m4_include|#m4_include|g" configure.ac
#sed -i'' -e "s|AX_CXX_COMPILE_STDCXX|#AX_CXX_COMPILE_STDCXX|g" configure.ac
#sed -i'' -e "s|AC_CHECK_LIB(regex|#AC_CHECK_LIB(regex|g" configure.ac
#./autogen.sh
#CXXFLAGS="-std=c++17" lt_cv_deplibs_check_method=pass_all ./configure --enable-srv_emb=yes --enable-mod_rediscache=yes
#make install -j4
#mv /tmp/ffead-cpp-src/ffead-cpp-5.0-bin /tmp/ffead-cpp-5.0-bin_ac
#cd /tmp
rm -rf /tmp/ffead-cpp-src
