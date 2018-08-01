#!/bin/sh
libdir=libwebsockets
 
g++ -g -o ws_client minimal-ws-client-echo.c -I$libdir/lib -I$libdir/build -L$libdir/build/lib -lwebsockets -lpthread