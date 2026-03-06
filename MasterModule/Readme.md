# Master Module

Pipeline: 
CubeMX configuration -> Coding -> Arm-Gcc compilation -> openOCD upload

Разъем ST-link - Разъем SWD
3,3V ---> 1pin
GND ---> 10pin
SWDIO ---> 7pin
SWCLK ---> 9pin


# Build
```bash
    cd MasterModule/ 
    compiledb make
```

# Load
```bash
    st-info --probe
    st-flash write ./build/MasterModule.bin 0x08000000
    st-flash --connect-under-reset write ./build/MasterModule.bin 0x08000000
```

# Erase 
```bash
    st-flash --connect-under-reset erase
```