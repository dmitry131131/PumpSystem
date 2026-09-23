#pragma once

#include "BusConnectionConfig.h"

// Ring buffer for operations
// After execution, operation is removed from buffer (pop)
class OperationBuffer {
private:
    static constexpr unsigned CAPACITY = 32;  // Power of two for fast modulo
    
    Operation operations[CAPACITY];
    unsigned head = 0;      // Read index (for execution)
    unsigned tail = 0;      // Write index (for adding)
    unsigned count = 0;     // Current operation count

public:
    // Add operation. Returns false if buffer is full
    bool push(const Operation& op);
    
    // Get pointer to next operation (without removing)
    Operation* peek();
    
    // Remove completed operation (advance head)
    void pop();
    
    // State checks
    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count == CAPACITY; }
    unsigned size() const { return count; }
    
    // Clear entire buffer
    void clear();
};
