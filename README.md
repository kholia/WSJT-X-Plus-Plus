WSJT-X-Plus-Pls is a (hopefully temporary) fork of WSJT-X_IMPROVED (https://sourceforge.net/projects/wsjt-x-improved/).

This fork adds support for next-generation of digital radios transceivers which are NOT audio based in some ways.

Build steps:

```
sudo apt-get build-dep wsjtx

cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -S . -B build

cd build

make -j8

sudo make super-install
```

Note: My changes are in the `wsjtx.patch` file.
