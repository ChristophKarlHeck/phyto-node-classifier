#include "serial_mail_sender/SerialMailSender.h"
#include "utils/sending_mutex.h"

#define BAUDRATE 115200

// Initialize the static BufferedSerial instance (PC_1 = TX, PC_0 = RX)
// Raspberry Pi: (GPIO 14 = TX, GPIO 15 = RX)
// Connection TX-RX, RX-TX, GND-GND
BufferedSerial SerialMailSender::m_serial_port(/*PC_1*/USBTX,/*PC_0*/USBRX, BAUDRATE);

// Get the single instance of SerialMailSender
SerialMailSender& SerialMailSender::getInstance(void) {
    static SerialMailSender instance;
    return instance;
}

// Private constructor
SerialMailSender::SerialMailSender(void) {
    m_serial_port.set_format(8, BufferedSerial::None, 1);  // 8N1 format
}

// Function to convert inputs to SerialMail::Value array
std::vector<SerialMail::Value> SerialMailSender::convertToSerialMailValues(const std::vector<std::array<uint8_t, 3>>& inputs) {
    std::vector<SerialMail::Value> raw_input_bytes;
    raw_input_bytes.reserve(inputs.size()); // Reserve space for efficiency

    for (const auto& input : inputs) {
        // Create SerialMail::Value from each 3-byte array
        raw_input_bytes.emplace_back(SerialMail::Value(input[0], input[1], input[2]));
    }

    return raw_input_bytes;
}

void SerialMailSender::convertMailToVectors(const SendingQueue::mail_t &sending_mail,
                          std::vector<std::array<uint8_t, 3>> &vec_ch0,
                          std::vector<std::array<uint8_t, 3>> &vec_ch1) {
    vec_ch0.assign(sending_mail.inputs_ch0.begin(), sending_mail.inputs_ch0.end());
    vec_ch1.assign(sending_mail.inputs_ch1.begin(), sending_mail.inputs_ch1.end());
}

void SerialMailSender::convertMailToFloatVectors(const SendingQueue::mail_t &sending_mail,
                                                 std::vector<float> &vec_ch0,
                                                 std::vector<float> &vec_ch1) {
    vec_ch0.assign(sending_mail.classification_ch0.begin(), sending_mail.classification_ch0.end());
    vec_ch1.assign(sending_mail.classification_ch1.begin(), sending_mail.classification_ch1.end());
}

// Serialize and send the SerialMail data
void SerialMailSender::sendMail(void) {

    while(true){
        sending_mutex.lock();
        SendingQueue& sending_queue = SendingQueue::getInstance();
        auto mail = sending_queue.mail_box.try_get();
        std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch0; 
		std::vector<std::array<uint8_t, 3>> inputs_as_bytes_ch1;
        std::vector<float> classification_values_ch0;
        std::vector<float> classification_values_ch1;
		if (mail != nullptr) {
            convertMailToVectors(*mail, inputs_as_bytes_ch0, inputs_as_bytes_ch1);
            convertMailToFloatVectors(*mail, classification_values_ch0, classification_values_ch1);
            sending_queue.mail_box.free(mail);
        }
        else{
            sending_mutex.unlock();
            continue;
        }
        sending_mutex.unlock();

        // Prepare the FlatBufferBuilder
        // FlatBufferBuilder should ideally be re-initialized inside the while loop 
        // for each iteration, especially if you are processing multiple messages. 
        // This is because the FlatBufferBuilder does not automatically clear its
        // internal buffer, and reusing it without clearing can lead to undefined behavior or memory issues.
        flatbuffers::FlatBufferBuilder builder(1024);

        // Create Flatbuffers vector of bytes
        std::vector<SerialMail::Value> raw_input_bytes_ch0 = convertToSerialMailValues(inputs_as_bytes_ch0);
        auto inputs_ch0 = builder.CreateVectorOfStructs(raw_input_bytes_ch0.data(), raw_input_bytes_ch0.size());

        std::vector<SerialMail::Value> raw_input_bytes_ch1 = convertToSerialMailValues(inputs_as_bytes_ch1);
        auto inputs_ch1 = builder.CreateVectorOfStructs(raw_input_bytes_ch1.data(), raw_input_bytes_ch1.size());

        // Create Flatbuffers float array
        auto classification_ch0 = builder.CreateVector(classification_values_ch0.data(), classification_values_ch0.size());

        // Create Flatbuffers float array
        auto classification_ch1 = builder.CreateVector(classification_values_ch1.data(), classification_values_ch1.size());

        // Channel and Classification active
        // bool classification_active = sending_mail->classification_active;
        // bool channel = sending_mail->channel;

        // Create the SerialMail object
        auto orc = CreateSerialMail(builder, inputs_ch0, inputs_ch1, classification_ch0, classification_ch1);
        builder.Finish(orc);

        // Get the buffer pointer and size
        uint8_t* buf = builder.GetBufferPointer();
        uint32_t size = builder.GetSize();

        // Send a synchronization marker (e.g., 0xAAAA)
        uint16_t sync_marker = 0xAAAA;
        m_serial_port.write(reinterpret_cast<const char*>(&sync_marker), sizeof(sync_marker));


        // Send the size (4 bytes)
        m_serial_port.write(reinterpret_cast<const char*>(&size), sizeof(size));

        // Send the FlatBuffers buffer
        m_serial_port.write(reinterpret_cast<const char*>(buf), size);
        
        //printf("sent\n");
        // Free the allocated mail to avoid memory leaks
        // make mail box empty
			
    }
}
