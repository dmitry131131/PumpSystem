#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "BusConnectionConfig.h"

enum DeviceType {
  PUMP,
  TEMPERATURE_CONTROLLER
};

struct Pump {
  unsigned Id;
  enum PumpState State;
  int MonitorNotified;  // TODO bool
  uint32_t LastAttendanceRequest;
  bool AttendanceQueued;
  bool AwaitingAttendance;
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
