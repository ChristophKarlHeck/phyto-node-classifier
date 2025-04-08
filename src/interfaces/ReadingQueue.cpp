#include "interfaces/ReadingQueue.h"

// Static method to access the single instance
// In C++11 and later, the common “Meyers Singleton” implementation
ReadingQueue& ReadingQueue::getInstance() {
    static ReadingQueue instance;  // Guaranteed to be destroyed, initialized on first use
    return instance;
}

// Private constructor to prevent direct instantiation
ReadingQueue::ReadingQueue(void) {
    // Initialization code, if necessary
}

// Private destructor (optional)
ReadingQueue::~ReadingQueue() {
    // Cleanup code, if necessary
}
