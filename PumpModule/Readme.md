# PumpModule

## Release build

```bash
    cmake -G "Unix Makefiles" -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
```

```bash
    cmake --build build/Release/ -j20
```

## Build from scratch
```bash
    cmake --build build/Release/ -j20 --verbose --clean-first
```

## Uploading
```bash
    avrdude -v -patmega328p -c arduino -P/dev/ttyUSB0 -b57600 -D -Uflash:w:./build/Release/bin/PumpModule.hex:i

    avrdude -v -patmega328p -c arduino -P /dev/ttyUSB0 -b57600 -Uflash:w:./build/Release/bin/PumpModule.hex:i
```