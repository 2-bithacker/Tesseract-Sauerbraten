# Tesseract: Sauerbraten

Most of all this is NOT my work.  I made it off of a couple of forks.

The original tesseract: https://github.com/lsalzman/tesseract

The fork I made this off of: https://github.com/2-bithacker/tesseract-1

The only difference between this one and that one is that this one is updated to accomodate the latest packages.

You might not have to build binaries at all as is comes with some pre-compiled, but if you do, this includes a bash installer script for debian-baased linux distros.  If you do not have a debian-based linux distro, you will have to do it manually, but if you have a debian-based linux distro, you can run `cd ~/Tesseract-Sauerbraten && chmod +x install && ./install` to install it.

The script also makes a desktop launcher for easy launching.

You can undo the installation if it has failed.  To do that, run `cd ~/Tesseract-Sauerbraten && ./uninstall` That will uninstall it.

To install on non-debian based linux distros, or if the script didn't work, you can install manually.  First, you have to make sure the opengl and sdl2 libraries are installed.  The names of the debian packages are `zlib1g-dev`, `libgeoip-dev`, `build-essential`, `libsdl2-dev`, `libsdl2-mixer-dev`, and `libsdl2-image-dev`  (The installer script handles this if you can use it.)  Make sure to clone this repository ro your home folder, or you will have to modify the lauch script and launcher to get it to work.  In all this I am assuming you are working in your home folder.

THe next thing you have to do, is to cd to the `/src` folder.  Once you are in there, you can run `make install` and that should build the necessary binaries.

How to launch without install script:  Navigate into the main directory, `cd Tesseract-Sauerbraten`, and run `./tesseract_unix`

That should get you going!  If you have any problems, feel free to start an issue, and I will do my best to set you up.

This version of tesseract sauerbraten (the more user-freindly version, not the source code updating) was brought to you by the RD sauerclan.  Check us out!  https://freddaws.wixsite.com/realdeal
