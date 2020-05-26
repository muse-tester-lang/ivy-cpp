sudo apt-get install libpcre3-dev tk-dev libglib2.0-dev libxt-dev 

V=3.15.1
TARBALL=ivy-c_${V}.tar.xz
curl -OL http://www.eei.cena.fr/products/ivy/download/packages/${TARBALL}
tar xvf ${TARBALL}
cd ivy-c_${V}/src
make
sudo make install
