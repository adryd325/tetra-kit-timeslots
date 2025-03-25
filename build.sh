#!/bin/sh

printf "\x1b[1m; ====== Building Decoder ====== \x1b[0m;"
( 
    cd decoder || exit
    make clean
    make
)

printf "\x1b[1m; ====== Building Codec ====== \x1b[0m;"
(
    cd codec || exit
    make clean
    make
)

printf "\x1b[1m; ====== Building Recorder ====== \x1b[0m;"
(
    cd recorder-js || exit
    npm install
)