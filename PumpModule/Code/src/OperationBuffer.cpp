#include "OperationBuffer.hpp"

// Add operation to buffer
// Returns false if buffer is full
bool OperationBuffer::push(const Operation& op) {
    if (isFull()) {
        return false;
    }
    
    operations[tail] = op;
    tail = (tail + 1) % CAPACITY;
    count++;
    
    return true;
}

// Get pointer to next operation for execution
// Returns nullptr if buffer is empty
Operation* OperationBuffer::peek() {
    if (isEmpty()) {
        return nullptr;
    }
    
    return &operations[head];
}

// Remove completed operation from buffer
void OperationBuffer::pop() {
    if (isEmpty()) {
        return;
    }
    
    head = (head + 1) % CAPACITY;
    count--;
}

// Clear entire buffer
void OperationBuffer::clear() {
    head = 0;
    tail = 0;
    count = 0;
}
