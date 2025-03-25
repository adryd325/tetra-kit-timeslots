
# tetra-kit-timeslots

> [!IMPORTANT]
> This is a fork of https://gitlab.com/larryth/tetra-kit. This fork moves from identifying calls based on usage identifiers to identifying calls both usage identifiers and channel allocations. The decoder has changes to estimate timeslots so that dropped frames dont mess up count, and sends timeslot and channel allocation in its output. The recorder has been replaced with one written in javascript. It uses usage identifiers and channel allocations to differenciate calls. It also supports live playback and will convert recordings to mp3

## TODO

 - [x] Add channel allocation information to D-SETUP
 - [x] Add own carrier metadata or receiver id to every JSON object sent over UDP
 - [x] Live playback 
 - [ ] Configure recorder-js with command line args (RX port, whether or not to record, transcode, or playback)
 
## Flow

### phys (SDR++ with sdrpp-tetra-demodulator)
In place of the gnuradio based phys layer, I've elected to use [SDR++](https://github.com/AlexandreRouma/SDRPlusPlus) with [sdrpp-tetra-demodulator](https://github.com/cropinghigh/sdrpp-tetra-demodulator)


### decoder
For each downlink, start a decoder

you can calculate carrier number with this formula
`frequency` is the frequency of the downlink
`band` is the hundreds place of the given frequency. For example 380.2625 is 300, 423.2875 is 400

the `- 0.01` is just to make sure 0.5 gets rounded down since possible offsets are `0, 0.00625, 0.0125, -0.00625`

round(((frequency - band) / 0.025) - .01)

```sh
./decoder -r <RX Port> -c <Carrier Num>
```

To decode an entire site, for example:
```sh
./decoder -r 8355 -c 518 & # 518 = 412.9500 + 0.0125
./decoder -r 8356 -c 944 & # 944 = 423.6000 + 0.0125
./decoder -r 8357 -c 948 & # 948 = 423.7000 + 0.0125
```

### recorder (js)

At the moment recorder has no arguments and just listens on port 42100 for udp messages from the decoder(s)

Because of the reliance on channel allocations to sort and decode calls, It's required that one of the decoders is that of the control channel
```
node --experimental-strip-types index.ts
```