/*
Change the values of the following variables in the file: mbed-os/connectivity/FEATUR_BLE/source/cordio/mbed_lib.json
- "desired-att-mtu": 250
- "rx-acl-buffer-size": 255
- PB_6 and PB_7 are reserevd for CONSOLE_TX and CNSOLE_RX. Therefore do not use PB_6 and PB_7.
*/

// Standard Library Headers
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Third-Party Library Headers
#include "mbed.h"

// Project-Specific Headers
#include "adc/AD7124.h"
#include "interfaces/ReadingQueue.h"
#include "interfaces/SendingQueue.h"
#include "model_executor/ModelExecutor.h"
#include "serial_mail_sender/SerialMailSender.h"

// Utility Headers
#include "utils/Conversion.h"
#include "utils/logger.h"

// *** DEFINE GLOBAL CONSTANTS ***
#define DOWNSAMPLING_RATE 1 // ms
#define VECTOR_SIZE 5

// CONVERSION
#define DATABITS 8388608
#define VREF 2.5
#define GAIN 4.0

// ADC
#define SPI_FREQUENCY 10000000 // 1MHz

// Thread for reading data from ADC
Thread reading_data_thread;

// Thread for sending data to data sink
Thread sending_data_thread;

// Function called in thread "reading_data_thread"
void get_input_model_values_from_adc(void){
	AD7124& adc = AD7124::getInstance(SPI_FREQUENCY);
    adc.read_voltage_from_both_channels(DOWNSAMPLING_RATE, VECTOR_SIZE);
}

void send_output_to_data_sink(void){

	// Access shared serial mail sender
    SerialMailSender& serial_mail_sender = SerialMailSender::getInstance();
	serial_mail_sender.sendMail();

}

void print_heap_stats() {
    mbed_stats_heap_t heap_stats;
    mbed_stats_heap_get(&heap_stats);
    printf("Current heap size: %lu / %lu bytes\n", heap_stats.current_size, heap_stats.reserved_size);
    printf("Max heap size: %lu bytes\n", heap_stats.max_size);
    printf("Allocations: %lu\n", heap_stats.alloc_cnt);
    printf("Failures: %lu\n", heap_stats.alloc_fail_cnt);
}

int main()
{	

	// Start reading data from ADC Thread
	reading_data_thread.start(callback(get_input_model_values_from_adc));

	// Start sending Thread
	sending_data_thread.start(callback(send_output_to_data_sink));

	int counter = 0;

    while (true) {

		// Instantiate and initialize the model executor
		ModelExecutor& executor = ModelExecutor::getInstance(1024); // Pass the desired pool size
		
		// Access the shared ReadingQueue instance
		ReadingQueue& reading_queue = ReadingQueue::getInstance();

		// Access the shared queue
		SendingQueue& sending_queue = SendingQueue::getInstance();

		auto mail = reading_queue.mail_box.try_get_for(rtos::Kernel::Clock::duration_u32::max());

		if (mail) {

		    // Retrieve the message from the mail box
		    ReadingQueue::mail_t* reading_mail = mail;

			// Store reading data temporary
			std::vector<std::array<uint8_t, 3>> inputs_as_bytes = reading_mail->inputs;
			bool channel = reading_mail->channel;

			// Free the allocated mail to avoid memory leaks
			// make mail box empty
			reading_queue.mail_box.free(reading_mail); 
				
			// Convert received bytes to floats
			std::vector<float> inputs = get_analog_inputs(inputs_as_bytes, DATABITS, VREF, GAIN);

			// Execute Model with received inputs
			std::vector<float> results = executor.run_model(inputs);

			while (!sending_queue.mail_box.empty()) {
                // Wait until sending queue is empty
                thread_sleep_for(1);
				printf("Wait for the sending queue to become empty.\n");
            }
		    
			if (sending_queue.mail_box.empty()) {
				SendingQueue::mail_t* sending_mail = sending_queue.mail_box.try_alloc();
				sending_mail->inputs = inputs_as_bytes;
				sending_mail->classification = results;
				sending_mail->classification_active = true;
				sending_mail->channel = channel;
				sending_queue.mail_box.put(sending_mail); 
			}
			counter = counter + 1;
			//printf("Counter: %d\n", counter);
			//print_heap_stats();
		}
	}

	// main() is expected to loop forever.
	// If main() actually returns the processor will halt

	return 0;
}