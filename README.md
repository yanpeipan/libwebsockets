# depend
## libwebsockets

```
git clone git@github.com:warmcat/libwebsockets.git
git checkout v3.0-stable
cd libwebsockets
mkdir build && cd build
cmake ..
make && sudo make install
```

## json-c

```
wget https://github.com/json-c/json-c/archive/json-c-0.13.1-20180305.tar.gz
tar zxvf json-c-0.13.1-20180305.tar.gz
cd json-c-json-c-0.13.1-20180305/
mkdir build
cd build
cmake ../
make
sudo make install
```

# install

```
mkdir build && cd build
cmake ..
make
```

# run

```
./client
```
