# Tesseract: Sauerbraten

[!NOTE]
This project is more or less dead (and very bug-ridden), but someone else is continuing the effort to port Sauerbraten to Tesseract engine: https://github.com/Big-Onche/Tesseract-Sauerbraten

Most of all this is NOT my work; I made it from a couple of forks:
    The original tesseract: https://github.com/lsalzman/tesseract
    The fork of a fork I made this off of: https://github.com/2-bithacker/tesseract

The only difference between this one and the original fork is that this one is updated to accomodate the latest packages.

To download, run `git clone https://github.com/2-bithacker/Tesseract-Sauerbraten.git`

That will download the files.

I have made this for linux, paricularly debian-based linux distros, but If you have windows or mac, you will have to build yourself some binaries. I do not know how to do it; If I did, I would include binaries for them too. You can probably look it up and find out how, somewehere.

You might not have to build binaries, as it comes with some for linux pre-compiled. 

This includes a bash installer script for debian-baased linux distros that can build your code and add a dektop launcher.  If you do not have a debian-based linux distro, you will have to do it manually, but if you have a debian-based linux distro, you can run `cd ~/Tesseract-Sauerbraten && chmod +x install && ./install` to install it.

The script also makes a desktop launcher for easy launching.

You can undo the installation if it has failed.  To do that, run `cd ~/Tesseract-Sauerbraten && ./uninstall` That will uninstall it.

To install on non-debian based linux distros, or if the script didn't work, you can install manually.

First, you have to make sure the opengl and sdl2 libraries are installed.  The names of the debian packages are `zlib1g-dev`, `libgeoip-dev`, `build-essential`, `libsdl2-dev`, `libsdl2-mixer-dev`, and `libsdl2-image-dev`  (The installer script handles this if you can use it.)  Make sure to clone this repository to your home folder, or you will have to modify the launch script to get it to work.  In all this I am assuming you are working in your home folder.

The next thing you have to do, is to cd to the `/src` folder.  Once you are in there, you can run `make install` and that should build the necessary binaries.

How to launch without install script:  Navigate into the main directory, `cd Tesseract-Sauerbraten`, and run `./tesseract_unix`

That should get you going!  If you have any problems, feel free to start an issue, and I will do my best to set you up.

This version of tesseract sauerbraten (the more user-freindly version, not the source code updating (Credits fo to Calinou on that one)) was brought to you by the RD sauerclan.  Check us out!  https://freddaws.wixsite.com/realdeal :) This clan mad dead btw lol
