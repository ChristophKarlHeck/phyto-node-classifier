#ifndef SENDING_QUEUE_H
#define SENDING_QUEUE_H

#include "mbed.h"
#include <vector>
#include <array>

#include "utils/constants.h"

class SendingQueue {
public:

    // Static method to access the single instance
    static SendingQueue& getInstance();

    // The structure used for inter-thread communication
    typedef struct {
        std::array<std::array<uint8_t, 3>, VECTOR_SIZE> inputs_ch0;
        std::array<std::array<uint8_t, 3>, VECTOR_SIZE> inputs_ch1;
        std::array<float, CLASSES> classification_ch0;
        std::array<float, CLASSES> classification_ch1;                              
    } mail_t;

    // Mail object for inter-thread communication
    Mail<mail_t, 1> mail_box;  

private:
    // Private constructor to prevent direct instantiation
    SendingQueue(void);

    // Private destructor (optional)
    ~SendingQueue();

    // Deleted copy constructor and assignment operator to prevent copies
    SendingQueue(const SendingQueue&) = delete;
    SendingQueue& operator=(const SendingQueue&) = delete;
};

#endif // SENDING_QUEUE_H
