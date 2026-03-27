WSJT-X-Plus-Plus is a (hopefully temporary) fork of WSJT-X_IMPROVED (https://sourceforge.net/projects/wsjt-x-improved/).

This fork adds support for next-generation of digital radio transceivers which are NOT audio based in some ways.

Build steps (for Linux users):

```
sudo apt-get build-dep wsjtx

cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -S . -B build

cd build

make -j8

sudo make super-install
```

Windows users: [Grab the latest WSJT-X-Plus-Plus (plain) installer file here.](https://github.com/kholia/WSJT-X-Plus-Plus/raw/refs/heads/clean/wsjtx-windows-installer-x64-plain.zip)
