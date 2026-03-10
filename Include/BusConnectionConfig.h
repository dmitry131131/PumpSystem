#ifndef BUS_CONNECTION_H
#define BUS_CONNECTION_H

enum CommandsID {
    COMMAND_STOP = 0x1,
    COMMAND_START = 0x2
};

enum MessageType {
    REGISTRATION_REQUEST = 0x10,
    REGISTRATION_SUCCESS = 0x11,
    REGISTRATION_DECLINED = 0x12,

    FORWARD_LOCK_REACHED = 0x13,
    FORWARD_LOCK_RELEASED = 0x14,
    REVERSE_LOCK_REACHED = 0x15,
    REVERSE_LOCK_RELEASED = 0x16,

    DATA_PACKAGE = 0x20,
    CLEAR_DATA_BUFFER = 0x21,

    ATTENDANCE_REQUEST = 0x30,
    ATTENDANCE_RESPONSE = 0x31
};

enum RotationDirection {
  REVERSE = 0,
  FORWARD = 1
};

enum OperationCode {
    ROTATION = 0x01,
    WAITING = 0x02
};

struct RotationOperation {
    enum OperationCode opCode;
    enum RotationDirection direction;
    float degree;
    unsigned RPM;  // [2 - 16] RPM
};

struct WaitingOperation {
    enum OperationCode opCode;
    unsigned time;
};

#endif