/*
Change the values of the following variables in the file: mbed-os/connectivity/FEATUR_BLE/source/cordio/mbed_lib.json
- "desired-att-mtu": 250
- "rx-acl-buffer-size": 255
- PB_6 and PB_7 are reserevd for CONSOLE_TX and CNSOLE_RX. Therefore do not use PB_6 and PB_7.
*/

#include "mbed.h"

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
#include "utils/reading_mutex.h"
#include "utils/constants.h"

// Utility Headers
#include "utils/Conversion.h"
#include "utils/logger.h"

// *** DEFINE GLOBAL CONSTANTS ***
#define DOWNSAMPLING_RATE 60 // seconds 

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

    SerialMailSender& serial_mail_sender = SerialMailSender::getInstance();
	serial_mail_sender.sendMail();

}

void convertMailToVectors(const ReadingQueue::mail_t &reading_mail,
                          std::vector<std::array<uint8_t, 3>> &vec_ch0,
                          std::vector<std::array<uint8_t, 3>> &vec_ch1) {
    vec_ch0.assign(reading_mail.inputs_ch0.begin(), reading_mail.inputs_ch0.end());
    vec_ch1.assign(reading_mail.inputs_ch1.begin(), reading_mail.inputs_ch1.end());
}

int main()
{	
	// Start reading data from ADC Thread
	reading_data_thread.start(callback(get_input_model_values_from_adc));

	std::vector<std::array<uint8_t, 3>> inputs_ch1 = {
			{0x01, 0x02, 0x03},
			{0x04, 0x05, 0x06},
			{0x07, 0x08, 0x09}
		};

	// Start sending Thread
	sending_data_thread.start(callback(send_output_to_data_sink));

    while (true) {
		// Access the shared ReadingQueue instance
		reading_mutex.lock();
		ReadingQueue& reading_queue = ReadingQueue::getInstance();
		ReadingQueue::mail_t *mail = reading_queue.mail_box.try_get();
		std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch0; 
		std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch1;
		if (mail != nullptr) {
			convertMailToVectors(*mail, inputs_as_bytes_ch0, inputs_as_bytes_ch1);
			reading_queue.mail_box.free(mail);
		}
		else{
			reading_mutex.unlock();
			continue;
		}
		reading_mutex.unlock();

		// Instantiate and initialize the model executor
		ModelExecutor& executor = ModelExecutor::getInstance(16384); // Pass the desired pool size

		// Access the shared queue
		SendingQueue& sending_queue = SendingQueue::getInstance();
			
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
			1.0);
			

		// Execute Model with received inputs
		std::vector<float> results_ch0 = executor.run_model(inputs_ch0_normalized);
		std::vector<float> results_ch1 = executor.run_model(inputs_ch1_normalized);


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

	
	}

	// main() is expected to loop forever.
	// If main() actually returns the processor will halt

	return 0;
}