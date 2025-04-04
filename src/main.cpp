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
#define MAX_VALUE 200
#define MIN_VALUE -200

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
	printf("hi");
	std::vector<float> test_input_vector = {
		906.1954, 995.5962, 950.4269, 815.3856, 815.6190,
		905.4908, 725.5136, 770.2163, 681.5200, 860.7928,
		815.1478, 905.4908, 860.0837, 770.4496, 905.4908,
		861.4973, 771.1541, 770.9208, 681.9911, 681.0488,
		771.6253, 816.0901, 771.1541, 726.6893, 771.3920,
		771.3920, 637.0552, 53.5965, -440.6903, -845.1141,
		-799.7069, -934.5149, -755.2421, -844.8762, -844.8762,
		-1069.3182, -1068.8472, -1024.1489, -799.9403, -710.5395,
		-82.3826, 366.9727, 591.1769, 680.8155, 725.2803,
		725.9848, 725.7515, 636.1129, 681.7534, 770.9208,
		816.0901, 680.8155, 725.9848, 636.8174, 859.3792,
		816.0901, 816.0901, 726.6893, 770.9208, 726.9272,
		680.5777, 725.9848, 726.4560, 680.8155, 770.9208,
		726.6893, 680.5777, 725.7515, 726.6893, 366.7348,
		-81.9159, -350.3516, -260.7130, -440.6903, -619.4919,
		-665.3702, -620.2008, -665.3702, -440.6903, -126.3808,
		-260.9509, -126.6141, 6.7803, 277.8052, 97.5946,
		-36.5088, -709.3638, -1337.2874, -1966.3821, -2056.4873,
		-2101.1899, -2010.8468, -2190.3530, -2191.0620, -2145.8882,
		-2146.3591, -2191.0620, -2190.3530, -2190.3530, -2100.4854
	};

	// Start reading data from ADC Thread
	reading_data_thread.start(callback(get_input_model_values_from_adc));

	// Start sending Thread
	sending_data_thread.start(callback(send_output_to_data_sink));

	// Online Min Max
	// OnlineMinMax online_min_max_ch0(600); // 600 = 1h since 100 values each 10 min
	// OnlineMinMax online_min_max_ch1(600); // 600 = 1h since 100 values each 10 min

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
			// online_min_max_ch0.update(inputs_ch0_mv);
			// online_min_max_ch1.update(inputs_ch1_mv);

			// Min-Max Scaling
			std::vector<float> inputs_ch0_normalized = Preprocessing::minMaxNormalization(
				inputs_ch0_mv,
				MIN_VALUE,
				MAX_VALUE,
				1000.0);
			std::vector<float> inputs_ch1_normalized = Preprocessing::minMaxNormalization(
				inputs_ch1_mv,
				MIN_VALUE,
				MAX_VALUE,
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