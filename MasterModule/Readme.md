# MasterModule

Мастер-модуль на STM32F103 для управления модулями насосов по CAN.

- [Code](Code/Readme.md) — прошивка, конфигурация платы, сборка и прошивка через ST-Link.
- `PCB/` — проект печатной платы.
- `3DModel/` — механические модели.

Сборка из каталога `MasterModule`:

```bash
cd Code
cmake --preset Debug
cmake --build --preset Debug
```