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
#include "preprocessing/Normalization.h"
#include "preprocessing/OnlineMinMax.h"

// Utility Headers
#include "utils/Conversion.h"
#include "utils/logger.h"

// *** DEFINE GLOBAL CONSTANTS ***
#define DOWNSAMPLING_RATE 600 // 10 min in seconds
#define VECTOR_SIZE 100 // So, we get 100 values from adc each 10 min

// CONVERSION
#define DATABITS 8388608
#define VREF 2.5
#define GAIN 4.0

// MIN-MAX-SCALING
#define MAX_VALUE 0.2
#define MIN_VALUE -0.2

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
	std::vector<float> test_input_vector = {
		0.3148, -0.2392,  1.9567,  0.9869,  0.0050,  1.2305,  1.1584,
		-0.3791, -0.4159,  0.6352,  0.5685,  1.0393,  1.5020, -0.1033,
		0.8617, -1.5182, -1.1849, -1.7219,  1.7003, -0.0118,  0.0657,
		-0.5634, -0.0981, -0.4158,  0.3380,  1.0453,  0.9805, -1.2600,
		-1.3031, -1.1824,  0.3772,  0.1872,  0.3362, -0.0430,  0.1310,
		0.4116, -0.5709,  0.2433,  1.6742,  0.5843, -0.9308, -0.5684,
		-0.5613, -1.6491,  0.4206, -0.9054,  0.7312, -1.5096, -0.3996,
		-0.8861, -0.9108, -0.3805, -0.6605, -0.5171, -1.0995, -0.3569,
		1.5997, -2.5390, -2.4954, -0.7654,  1.4457,  1.2585,  0.1363,
		-0.1437, -0.0370,  0.6101, -1.1453,  1.4333, -1.2200,  0.9154,
		0.6001,  0.4928,  1.5454,  0.0624, -0.7268,  0.6277,  0.8344,
		-0.1834, -0.2857, -0.9900, -0.8002,  0.2790,  1.1976, -0.6656,
		-1.5403, -0.5115,  0.0698, -1.4269,  0.2222,  0.3240,  0.6256,
		1.6387, -1.6100,  0.1028,  0.1044, -0.4829, -0.2006, -2.0888,
		-0.4376,  1.2862
	};

	// Start reading data from ADC Thread
	reading_data_thread.start(callback(get_input_model_values_from_adc));

	// Start sending Thread
	sending_data_thread.start(callback(send_output_to_data_sink));

	// Online Min Max
	OnlineMinMax online_min_max_ch0(600); // 600 = 1h since 100 values each 10 min
	OnlineMinMax online_min_max_ch1(600); // 600 = 1h since 100 values each 10 min

    while (true) {

		// Instantiate and initialize the model executor
		ModelExecutor& executor = ModelExecutor::getInstance(16384); // Pass the desired pool size
		
		// Access the shared ReadingQueue instance
		ReadingQueue& reading_queue = ReadingQueue::getInstance();

		// Access the shared queue
		SendingQueue& sending_queue = SendingQueue::getInstance();

		auto mail = reading_queue.mail_box.try_get_for(rtos::Kernel::Clock::duration_u32::max());

		if (mail) {
			//print_heap_stats();

		    // Retrieve the message from the mail box
		    ReadingQueue::mail_t* reading_mail = mail;

			// Store reading data temporary
			std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch0 = reading_mail->inputs_ch0;
			std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch1 = reading_mail->inputs_ch1;

			// Free the allocated mail to avoid memory leaks
			// make mail box empty
			reading_queue.mail_box.free(reading_mail); 
				
			// Convert received bytes to floats
			std::vector<float> inputs_ch0_mv = get_analog_inputs(inputs_as_bytes_ch0, DATABITS, VREF, GAIN);
			std::vector<float> inputs_ch1_mv = get_analog_inputs(inputs_as_bytes_ch1, DATABITS, VREF, GAIN);

			// Online Min Max Sliding Window
			online_min_max_ch0.update(inputs_ch0_mv);
			online_min_max_ch1.update(inputs_ch1_mv);

			// Min-Max Scaling
			std::vector<float> inputs_ch0_normalized = Preprocessing::minMaxNormalization(
				inputs_ch0_mv,
				online_min_max_ch0.getMinValue(),
				online_min_max_ch0.getMaxValue(),
				1000.0);
			std::vector<float> inputs_ch1_normalized = Preprocessing::minMaxNormalization(
				inputs_ch1_mv,
				online_min_max_ch1.getMinValue(),
				online_min_max_ch1.getMaxValue(),
				1000.0);

			// Execute Model with received inputs
			std::vector<float> results_ch0 = executor.run_model(inputs_ch0_normalized);
			std::vector<float> results_ch1 = executor.run_model(inputs_ch1_normalized);

			while (!sending_queue.mail_box.empty()) {
                // Wait until sending queue is empty
                thread_sleep_for(1);
				//printf("Wait for the sending queue to become empty.\n");
            }
		    
			if (sending_queue.mail_box.empty()) {
				SendingQueue::mail_t* sending_mail = sending_queue.mail_box.try_alloc();
				sending_mail->inputs_ch0 = inputs_as_bytes_ch0;
				sending_mail->inputs_ch1 = inputs_as_bytes_ch1;
				sending_mail->classification_ch0 = results_ch0;
				sending_mail->classification_ch1 = results_ch1;
				sending_queue.mail_box.put(sending_mail); 
			}
			//print_heap_stats();
		}
	}

	// main() is expected to loop forever.
	// If main() actually returns the processor will halt

	return 0;
}