#pragma once

#include "BusConnectionConfig.h"

struct OperationBuffer {
  RotationOperation operations[16] = {};
  unsigned OperationCapacity = sizeof(operations)/sizeof(RotationOperation);
  unsigned OperationCount = 0;
  unsigned current_operation_num = 0;
};

