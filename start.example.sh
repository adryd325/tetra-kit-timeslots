#!/bin/bash

# Use sdr++ tetra demodulator netsyms to feed the decoders
( 
    cd decoder || exit
    ./decoder -r 8355 -c 518 & # 518 = 412.9500 + 0.0125
    ./decoder -r 8356 -c 944 & # 944 = 423.6000 + 0.0125
    ./decoder -r 8357 -c 948 & # 948 = 423.7000 + 0.0125
)

( 
    cd recorder-js || exit
    node --experimental-strip-types index.ts
)