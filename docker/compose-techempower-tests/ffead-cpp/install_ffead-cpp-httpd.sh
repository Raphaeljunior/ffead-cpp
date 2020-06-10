#!/bin/bash

cd $IROOT

#chown -R www-data:www-data ffead-cpp-4.0

#wget -q https://archive.apache.org/dist/httpd/httpd-2.4.25.tar.gz
#wget -q https://archive.apache.org/dist/apr/apr-1.5.2.tar.gz
#wget -q https://archive.apache.org/dist/apr/apr-util-1.5.4.tar.gz
#rm -rf ${IROOT}/httpd-2.4.25
#rm -rf ${IROOT}/apr-1.5.2
#rm -rf ${IROOT}/apr-util-1.5.4
#tar xf httpd-2.4.25.tar.gz
#tar xf apr-1.5.2.tar.gz
#tar xf apr-util-1.5.4.tar.gz
#mv -f apr-1.5.2 httpd-2.4.25/srclib/apr
#mv -f apr-util-1.5.4 httpd-2.4.25/srclib/apr-util
#cd ${IROOT}/httpd-2.4.25
#rm -rf ${IROOT}/httpd
#mkdir ${IROOT}/httpd
#./configure --prefix=${IROOT}/httpd --enable-mods-shared=all --with-included-apr
#make
#make install
#cd ${IROOT}

sed -i 's|#define PACKAGE_BUGREPORT "sumeet.chhetri@gmail.com"| |g' ${IROOT}/ffead-cpp-4.0/include/AppDefines.h
sed -i 's|#define PACKAGE_NAME "ffead-cpp"| |g' ${IROOT}/ffead-cpp-4.0/include/AppDefines.h
sed -i 's|#define PACKAGE_STRING "ffead-cpp 2.0"| |g' ${IROOT}/ffead-cpp-4.0/include/AppDefines.h
sed -i 's|#define PACKAGE_TARNAME "ffead-cpp"| |g' ${IROOT}/ffead-cpp-4.0/include/AppDefines.h
sed -i 's|#define PACKAGE_VERSION "2.0"| |g' ${IROOT}/ffead-cpp-4.0/include/AppDefines.h

#cd ${IROOT}/ffead-cpp-src/modules/apache_mod_ffeadcpp/
#g++ -fpic -DSHARED_MODULE -fpermissive -std=gnu++11 -I"/usr/include/apache2" -I"/usr/include/apr-1.0" -I"${IROOT}/ffead-cpp-4.0/include/" -I"${IROOT}/include" -I"${IROOT}/include/libbson-1.0/" -I"${IROOT}/include/libmongoc-1.0" mod_ffeadcpp.cpp -L"${IROOT}/ffead-cpp-4.0/lib" -L"${IROOT}" -L"${IROOT}/lib" -lffead_common -lffead_framework -ldl -lcrypto -lssl -lhiredis -lmemcachedutil -c mod_ffeadcpp.cpp
#g++ -shared -o mod_ffeadcpplib.so mod_ffeadcpp.o -L"${IROOT}/ffead-cpp-4.0/lib" -L"${IROOT}" -L"${IROOT}/lib" -lffead_common -lffead_framework -ldl -lcrypto -lssl -lapr-1 -laprutil-1 -lstdc++ -lhiredis -lmemcachedutil
#apxs -i -n 'ffead_cpp_module' mod_ffeadcpplib.so
#cd -

FFEADROOT=${IROOT}/ffead-cpp-4.0
ETROOT=${FFEADROOT//\//\\/}
EIROOT=${IROOT//\//\\/}

sed -i 's|Include ports.conf|#Include ports.conf|g' /etc/apache2/apache2.conf
#echo 'Include /etc/apache2/sites-enabled/ffead-site.conf' >> /etc/apache2/apache2.conf

bash -c 'cat <<EOL >> /etc/apache2/envvars
export FFEAD_CPP_PATH='"${FFEADROOT}"'
export LD_LIBRARY_PATH='"${FFEADROOT}"'/lib:'"${IROOT}"':'"${IROOT}"'/lib:/usr/local/lib:$LD_LIBRARY_PATH/
export ODBCINI='"$IROOT"'/odbc.ini
export ODBCSYSINI='"$IROOT"' 
EOL'

bash -c 'cat > /etc/apache2/mods-enabled/mpm_event.conf <<EOL
<IfModule mpm_event_module>
    AsyncRequestWorkerFactor   2
    ThreadsPerChild           64
    ServerLimit              100
    StartServers              20
    MinSpareThreads          100
    MaxSpareThreads          200
    ListenBacklog 			4096
</IfModule>
EOL'

mv /etc/apache2/sites-enabled/000-default.conf /etc/apache2/sites-enabled/ffead-site.conf
bash -c 'cat > /etc/apache2/sites-enabled/ffead-site.conf <<EOL
LoadModule ffead_cpp_module /usr/lib/apache2/modules/mod_ffeadcpplib.so
Listen 8080
FFEAD_CPP_PATH '"${FFEADROOT}"'
<VirtualHost *:8080>
	DocumentRoot '"${FFEADROOT}"'/web
	SetHandler ffead_cpp_module
	<Directory '"${FFEADROOT}"'>
		Options FollowSymLinks
		AllowOverride None
		Require all denied
	</Directory>
	<Directory '"${FFEADROOT}"'/web/>
		Options -Indexes +FollowSymLinks +MultiViews
		AllowOverride All
		Require all granted
	</Directory>
</VirtualHost>
EOL'
