apt update && apt -yq upgrade
apt -yq install git u-boot-tools build-essential libncurses5-dev bison flex bc libboost1.67-all-dev libssl-dev libprotobuf-dev protobuf-compiler libgstreamer1.0-dev libconfig-dev libusb-1.0-0-dev libegl-dev libgles2-mesa-dev libglm-dev
apt -yq remove libsdl2-2.0-0 python3-distupgrade && rm -f /etc/pulse/default.pa && DEBIAN_FRONTEND=noninteractive apt -yq install libsdl2-dev libsdl2-image-dev liblxc-dev libproperties-cpp-dev libsystemd-dev libcap-dev libgmock-dev python3-distupgrade ubuntu-release-upgrader-core ubuntu-desktop-minimal libxtst-dev adb gpsd gpsd-clients lightdm gstreamer1.0-plugins-ugly gstreamer1.0-plugins-bad gstreamer1.0-libav tigervnc-scraping-server lxc-utils xfce4 libdw-dev mc clangd clang gpiod libfmt-dev
apt -yq install glxgears

git clone https://github.com/libusbgx/libusbgx.git && cd libusbgx && autoreconf -i && ./configure --prefix=/usr && make && make install && cd ..
apt install -yq libsdl1.2-dev libcairo2-dev libpango1.0-dev tcl8.6-dev

mkdir snowmix && cd snowmix
wget https://deac-riga.dl.sourceforge.net/project/snowmix/Snowmix-0.5.1.1.tar.gz && tar -zxvf Snowmix-0.5.1.1.tar.gz && cd Snowmix-0.5.1.1
aclocal && autoconf && libtoolize --force && automake --add-missing && ./configure --prefix=/usr --libdir=/usr/lib
make -j 4 && make install
cd ..
mkdir AA && cd AA
git clone --recurse-submodules https://github.com/bert1337/AACS.git && cd AACS && git checkout video-debug && mkdir build && cd build && cmake .. && make -j 4 && cd ../..
cd
apt -yq remove gdm3 && dpkg-reconfigure lightdm