#ifndef DEVICE_H
#define DEVICE_H

enum DeviceType {
  PUMP,
  TEMPERATURE_CONTROLLER
};

enum PumpState {
    PUMP_DISABLE,
    PUMP_ENABLE,
    PUMP_BLOCKED_FORWARD,
    PUMP_BLOCKED_REVERSE
};

struct Pump {
  unsigned Id;
  enum PumpState State;
};

struct PumpList {
    struct Pump *List;
    unsigned capacity;
    unsigned size;
};

int PumpListInit(struct PumpList *List, unsigned capacity);
int PumpListFree(struct PumpList *List);

struct Pump* FindPump(struct PumpList *List, unsigned ID);
int AddPump(struct PumpList *List, struct Pump);



#endif