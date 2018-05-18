#!/bin/bash

read -p 'Compile modules or not [y/n]: ' compileMods

echo "Making old config\n"
make ARCH=i386 oldconfig
export ARCH=i386
make oldconfig

echo "Compiling kernel\n"
make ARCH=i386 bzImage -j5

echo "Compiling modules"
make ARCH=i386 modules -j5

echo "Exporting install path\n"
mkdir /home/antoine/Documents/OS6/boot
export INSTALL_PATH=/home/antoine/Documents/OS6/boot

echo "Exporting mods install path\n"
mkdir /home/antoine/Documents/OS6/mods
export INSTALL_MOD_PATH=/home/antoine/Documents/OS6/mods

echo "Installing modules\n"
make ARCH=i386 modules_install

echo "Installing kernel\n"
make ARCH=i386 install
