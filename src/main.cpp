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

// Project-Specific Headers
#include "adc/AD7124.h"
#include "interfaces/ReadingQueue.h"
#include "interfaces/SendingQueue.h"
#include "model_executor/ModelExecutor.h"
#include "serial_mail_sender/SerialMailSender.h"
#include "preprocessing/Normalization.h"
#include "utils/mbed_stats_wrapper.h"

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
#define MAX_VALUE 200.0
#define MIN_VALUE -200.0

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


int main()
{	
	// Start reading data from ADC Thread
	reading_data_thread.start(callback(get_input_model_values_from_adc));

	// Start sending Thread
	sending_data_thread.start(callback(send_output_to_data_sink));

    while (true) {

		// Instantiate and initialize the model executor
		ModelExecutor& executor = ModelExecutor::getInstance(16384); // Pass the desired pool size
		
		// Access the shared ReadingQueue instance
		ReadingQueue& reading_queue = ReadingQueue::getInstance();

		// Access the shared queue
		SendingQueue& sending_queue = SendingQueue::getInstance();

		auto mail = reading_queue.mail_box.try_get_for(rtos::Kernel::Clock::duration_u32::max());

		if (mail) {

			//mbed_lib::print_memory_info("1");

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


			// Min-Max Scaling
			std::vector<float> inputs_ch0_normalized = Preprocessing::minMaxNormalization(
				inputs_ch0_mv,
				-0.2,
				0.2,
				1.0);
			std::vector<float> inputs_ch1_normalized = Preprocessing::minMaxNormalization(
				inputs_ch1_mv,
				-0.2,
			    0.2,
				1000.0);
			
			// mbed_lib::print_memory_info("1");

			// Execute Model with received inputs
			std::vector<float> results_ch0 = executor.run_model(inputs_ch0_normalized);

			// mbed_lib::print_memory_info("2");
			std::vector<float> results_ch1 = executor.run_model(inputs_ch1_normalized);

			// mbed_lib::print_memory_info("3");

			while (!sending_queue.mail_box.empty()) {
                // Wait until sending queue is empty
                thread_sleep_for(1);
				//printf("Wait for the sending queue to become empty.\n");
            }
		    
			// mbed_lib::print_memory_info("4");

			if (sending_queue.mail_box.empty()) {
				SendingQueue::mail_t* sending_mail = sending_queue.mail_box.try_alloc();
				sending_mail->inputs_ch0 = inputs_as_bytes_ch0;
				sending_mail->inputs_ch1 = inputs_as_bytes_ch1;
				sending_mail->classification_ch0 = results_ch0;
				sending_mail->classification_ch1 = results_ch1;
				sending_queue.mail_box.put(sending_mail); 
			}

			// mbed_lib::print_memory_info("5");
		}
	}

	// main() is expected to loop forever.
	// If main() actually returns the processor will halt

	return 0;
}