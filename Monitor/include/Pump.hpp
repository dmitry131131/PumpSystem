#pragma once
#include <optional>
#include <variant>

enum class PumpStatus {
    PUMP_ONLINE,
    PUMP_OFFLINE,
    PUMP_FORWARD_LOCKED,
    PUMP_REVERSE_LOCKED
};

struct RotationInstruction {
    enum class Direction {
        REVERSE,
        FORWARD
    } direction_;    // Rotation direction

    float degree_;   // Rotation degree
    unsigned rpm_;   // RPM 

    RotationInstruction(Direction direction, float degree, unsigned rpm) : direction_(direction),
                                                                           degree_(degree),
                                                                           rpm_(rpm) {}
};

struct WaitingInstruction {
    unsigned time_; // Waiting time in ms

    WaitingInstruction(unsigned time) : time_(time) {}
};

using Instruction = std::variant<RotationInstruction, WaitingInstruction>;

class Pump {
    unsigned id_ = 0;
    PumpStatus status_ = PumpStatus::PUMP_OFFLINE;
    double springeArea_; // Springe area in cm^2

public:
    Pump(unsigned id, PumpStatus status = PumpStatus::PUMP_OFFLINE) : id_(id), status_(status), springeArea_(1) {}

    unsigned getId() const {return id_;}
    PumpStatus getStatus() const {return status_;}

    void setStatus(PumpStatus status) {status_ = status;}

    void setSpringeArea(double springeArea) {springeArea_ = springeArea;};
    double getSpringeArea() const { return springeArea_; }

    bool operator== (const Pump& rhs) {
        return id_ == rhs.id_;
    }
};
