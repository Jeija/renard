# `renard` - Sigfox Protocol CLI
<img src="logo.svg" align="right" width="20%"/>

`renard` is the command-line interface for [`librenard`](https://github.com/jeija/librenard), an open source library that implements encoding and decoding of [Sigfox](https://www.sigfox.com) uplinks and downlinks. `renard` can be used to inspect the contents of Sigfox frames or forge Sigfox frames for transmission with an SDR.

`renard` does not implement physical layer (de)modulation, please refer to [`renard-phy`](https://github.com/Jeija/renard-phy) for that instead.

## Installation
Both `librenard` and `renard` are cross-platform software that doesn't require any dependencies. `librenard` is included as a git submodule in `renard`.

* Clone this repository recursively:
```
git clone https://github.com/Jeija/renard --recursive
```
* Make sure you have any C compiler installed (`gcc`, `clang`) and compile `renard`:
```
make
```
* Test `renard`:
```
make test
./build/renard help
```

## Modes of Operation: Samples
All examples are based on a Sigfox object with the device ID `004d33db` and a network authentication key (NAK, also called secret / private key) `479e4480fd7596d45b0122fd282db3cf`.

### Uplink Encoding
* Generate all three transmissions of an uplink frame containing the 4 bytes `0xab 0xad 0x1d 0xea` as payload with sequence number `0x1a0`
```
$ renard ulencode -p abad1dea -s 1a0 -i 004d33db -k 479e4480fd7596d45b0122fd282db3cf

aaaaa35f01a0db334d00abad1deadc32758a
aaaaa5980118806638c0d490d4650527d22d
aaaaa5a301c8edff9e4081465a906b3ee8e8
```

* Generate single-bit uplink frame with payload bit set to `1`. Sequence number (SN) is `0x1a1`, request downlink, do not generate replicas:
```
$ renard ulencode -e -p 1 -r 0 -d -s 1a1 -i 004d33db -k 479e4480fd7596d45b0122fd282db3cf

aaaaa06be1a1db334d0000b0c090
```

### Uplink Decoding
* Decode first replica (second transmission) of the 4-byte frame generated above and check message authentication code (MAC):
```
$ renard uldecode -f 5980118806638c0d490d4650527d22d -k 479e4480fd7596d45b0122fd282db3cf

Downlink request: no
Sequence Number : 1a0
Device ID       : 4d33db
Payload         : abad1dea
CRC             : OK
MAC             : OK
```

* Decode initial transmission of the single-bit frame generated above, do not check MAC:
```
$ renard uldecode -f 06be1a1db334d0000b0c090

Downlink request: yes
Sequence Number : 1a1
Device ID       : 4d33db
Payload         : 1 (single bit-message)
CRC             : OK
MAC             : didn't perform check, provide secret key to check MAC
```

### Downlink Encoding
* Generate downlink frame containing the 8 bytes `0xde 0xad 0xbe 0xef 0x02 0x46 0x8a 0xce` as a response to the uplink with SN `0x1a1`:
```
$ renard dlencode -p deadbeef02468ace -i 004d33db -k 479e4480fd7596d45b0122fd282db3cf -s 1a1

2aaaaaaaaaaaaaaaaaaaaab2273516816cc7ce125ef60d5b6d78c1c
```

### Downlink Decoding
The downlink's payload is scrambled.
Descrambling is only possible if the seed for the linear-feedback shift register (LFSR) that generates the whitening sequence is known.
This seed can be calculated from the Sigfox object's device ID and the SN of the uplink that requested the downlink.
It can, however, also be cracked by cycling through all possible SN values or LFSR seeds (brute-force modes).

* Decode the above downlink knowing device ID, SN and NAK:
```
$ renard dldecode -f 3516816cc7ce125ef60d5b6d78c1c1 -i 004d33db -k 479e4480fd7596d45b0122fd282db3cf -s 1a1

deadbeef02468ace
```

* Decode the above uplink knowing device ID and NAK, but not SN (first brute-force mode):
```
$ renard dldecode -f 3516816cc7ce125ef60d5b6d78c1c1 -i 004d33db -k 479e4480fd7596d45b0122fd282db3cf -b

Found possible uplink sequence number: 0x1a1
deadbeef02468ace
```

* Decode the above uplink knowing nothing. Only the LFSR seed that didn't cause any FEC to be applied produces the correct payload (second brute-force mode):
```
$ renard dldecode -f 3516816cc7ce125ef60d5b6d78c1c1 -c

Found LFSR seed with matching CRC: 0x12f, corresponding payload: 41690131d2f0843d - FEC was applied, probably incorrect
Found LFSR seed with matching CRC: 0x15f, corresponding payload: f8b99eec245fe148 - FEC was applied, probably incorrect
Found LFSR seed with matching CRC: 0x1bb, corresponding payload: deadbeef02468ace - no FEC was applied
Found LFSR seed with matching CRC: 0x1f1, corresponding payload: 314bf2036a9d8a17 - FEC was applied, probably incorrect
```
