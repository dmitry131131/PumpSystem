# Master Module

Pipeline: 
CubeMX configuration -> Coding -> Arm-Gcc compilation -> openOCD upload

# Build
```bash
    cd MasterModule/ 
    make
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