#pragma once

#include <vector>
#include <variant>
#include <unordered_map>
#include <optional>

enum class PumpStatus {
    PUMP_ONLINE,
    PUMP_OFFLINE
};

struct RotationInstruction {
    enum class Direction {
        FORWARD,
        REVERSE
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
    std::vector<Instruction> instructions_;

public:
    Pump(unsigned id, PumpStatus status = PumpStatus::PUMP_OFFLINE) : id_(id), status_(status) {}

    bool operator== (const Pump& rhs) {
        return id_ == rhs.id_;
    }

    


};

class PumpMonitor {
    std::unordered_map<unsigned, Pump> Pumps;
    
public:
    PumpMonitor() = default;

    bool registerPump(const Pump& pump);
    std::optional<Pump> getPump(unsigned id);
};