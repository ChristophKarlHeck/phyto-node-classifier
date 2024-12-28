
#include "adc/AD7124.h"
#include "adc/AD7124-defs.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include "interfaces/ReadingQueue.h"


void AD7124::ctrl_reg(char RW){
    /* read/write the control register */

    if(RW == m_read){
        m_spi.write(AD7124_R | AD7124_ADC_CTRL_REG);
        TRACE("ADC contr_reg =");
        for (int i = 0; i<=1; i++){
            int byte = m_spi.write(0x00);
            TRACE("%s", byte_to_binary(byte).c_str());
        }
        TRACE("\n");
    } else {
        m_spi.write(AD7124_ADC_CTRL_REG);
        const uint16_t contr_reg_settings = AD7124_ADC_CTRL_REG_DATA_STATUS | AD7124_ADC_CTRL_REG_REF_EN | AD7124_ADC_CTRL_REG_CONT_READ | 
                                        AD7124_ADC_CTRL_REG_POWER_MODE(0) | AD7124_ADC_CTRL_REG_MODE(0)|AD7124_ADC_CTRL_REG_CLK_SEL(0);
        char contr_reg_set[]={contr_reg_settings>>8 & 0xFF, contr_reg_settings & 0xFF};

        for (int i = 0; i<=1; i++){
            m_spi.write(contr_reg_set[i]);
        }   
    }
}


/* channel_reg
 * Sets up the channel registers.
 * Can setup channel 0, or 1, or both depending on flags
 */
void AD7124::channel_reg(char RW){
    //RW=1 -> read else write
    if(RW == m_read){
        m_spi.write(AD7124_R | AD7124_CH1_MAP_REG);
        TRACE("Channel register =");
        for (int i = 0; i<=1; i++){
            int byte = m_spi.write(0x00);
            TRACE("%s", byte_to_binary(byte).c_str());
        }
        TRACE("\n");

    } else {
        // SET CHANNEL 0
        if(m_flag_0 == true){
            m_spi.write(AD7124_CH0_MAP_REG);
            const uint16_t channel_settings = AD7124_CH_MAP_REG_CH_ENABLE | AD7124_CH_MAP_REG_SETUP(0) |
                                                 AD7124_CH_MAP_REG_AINP(0) |AD7124_CH_MAP_REG_AINM(1);
            char channel_reg_set_ch0[]={channel_settings>>8 & 0xFF, channel_settings & 0xFF}; //channel 0 and 1 (0x80, 0x01)
                //0x80 for setup 0
            for (int i = 0; i<=1; i++){
                m_spi.write(channel_reg_set_ch0[i]);
            }
        }
        // SET CHANNEL 1
        if(m_flag_1 == true){
            m_spi.write(AD7124_CH1_MAP_REG);
            //register bytes set like this -> 10 00 00 (00 - 01 0)(0 00 11)
            //channel 1 - pins (2) and (3)
            //x80 is 1st byte, x43 is 2nd byte (for setting the AIN)
            const uint16_t channel_settings1 = AD7124_CH_MAP_REG_CH_ENABLE | AD7124_CH_MAP_REG_SETUP(1) |
                                        AD7124_CH_MAP_REG_AINP(2) |AD7124_CH_MAP_REG_AINM(3);

            char channel_reg_set_ch1[]={channel_settings1>>8 & 0xFF, channel_settings1 & 0xFF}; //channel 2 and 3 (0x90, 0x43)
                //0x90 for setup 1
            for (int i = 0; i<=1; i++){
                m_spi.write(channel_reg_set_ch1[i]);
            }
        }
    }
}

void AD7124::filter_reg(uint8_t filt, char RW){
    //default FS: 384, Post filter = 011
    if(RW == m_read){
        m_spi.write(AD7124_R | filt);
        TRACE("Filter register =");
        for (int i = 0; i<=2; i++){
            int byte = m_spi.write(0x00);
            TRACE("%s", byte_to_binary(byte).c_str());
        }
        TRACE("\n");
    }
    else{
        m_spi.write(filt);
        //char contr_reg_set[]={0x00,0x08};
        //char filter_reg_set[]={AD7124_FILT_REG_FILTER(4)>>16,0x00,0x40};
        char filter_reg_set[]={0x00, 0x00, 0x00}; //0x00,0x12,0xC0 for testing
        for (int i = 0; i<=2; i++){
            m_spi.write(filter_reg_set[i]);
        }
    }
}
void AD7124::config_reg(uint8_t address ,char RW){
    /* read/ write the configuration register */
    if(RW == m_read){
        m_spi.write(AD7124_R | address);
        TRACE("ADC conf = ");
        for (int i = 0; i<=1; i++){
            int byte = m_spi.write(0x00);
            TRACE("%s", byte_to_binary(byte).c_str());
        }
        TRACE("\n");
    }
    else{
        m_spi.write(address);
        char my_config[]={(AD7124_CFG_REG_BIPOLAR ) >>8 ,
                          AD7124_CFG_REG_AIN_BUFP | AD7124_CFG_REG_AINN_BUFM | AD7124_CFG_REG_REF_SEL(2) |AD7124_CFG_REG_PGA(2)};
        //original 0x08, 0x71
        for (int i = 0; i<=1; i++){
            m_spi.write(my_config[i]);
        } 
    }
}

void AD7124::reset(){
    /* reset the ADC */
    //INFO("Reset ADC\n");
    for (int i = 0; i< 8; i++){
        m_spi.write(0xFF);
    }
}

char AD7124::status(){
    /* read the status register */
    m_spi.write(AD7124_R | AD7124_STATUS_REG);
    char status = m_spi.write(0x00); 
    TRACE("ADC status = 0x%X, %s\n", status, byte_to_binary(status).c_str());
    return status;
}

void AD7124::init(bool f0, bool f1){
    m_flag_0 = f0;
    m_flag_1 = f1;
    m_sync = 1;
    m_cs=0;

    reset();
    status();

    channel_reg(m_read); //activate 2 channels
    channel_reg(m_write); //activate 2 channels
    channel_reg(m_read); //activate 2 channels

//flags for if you want to have channel 0, or 1, or both active
    if(m_flag_0 == true){
        //config reg 0
        config_reg(AD7124_CFG0_REG, m_read);   // read  configuration register
        config_reg(AD7124_CFG0_REG, m_write);  // write configuration register
        config_reg(AD7124_CFG0_REG, m_read);   // proof writing by reading again
        //filter reg 0
        filter_reg(AD7124_FILT0_REG, m_read);  // same with filter register
        filter_reg(AD7124_FILT0_REG, m_write);
        filter_reg(AD7124_FILT0_REG, m_read);
    }

    if(m_flag_1 == true){
        //config reg 1
        //AD7124::config_reg(AD7124_CFG1_REG, read);
        config_reg(AD7124_CFG1_REG, m_write);
        config_reg(AD7124_CFG1_REG, m_read);
        //filter reg 1
        //AD7124::filter_reg(AD7124_FILT1_REG, read);
        filter_reg(AD7124_FILT1_REG, m_write);
        filter_reg(AD7124_FILT1_REG, m_read);
    }
    //xAD7124::calibrate(1,1,0,0);

    ctrl_reg(m_read);     // same with control register
    ctrl_reg(m_write);
    ctrl_reg(m_read);
    //AD7124::calibrate(1,0,0,0);
}

/**
 * @brief Constructs an AD7124 object and initializes SPI communication.
 * @param spi_frequency The SPI clock frequency in Hz.
 */
AD7124::AD7124(int spi_frequency):
    m_spi(PA_7, PA_6, PA_5), m_drdy(PA_6), m_cs(PA_4), m_sync(PA_1),
    m_spi_frequency(spi_frequency), m_flag_0(false), m_flag_1(false),
    m_read(1), m_write(0){

    m_spi.format(8, 3);           
    m_spi.frequency(m_spi_frequency);

    init(true,true);
}

/**
 * @brief Gets the singleton instance of the AD7124 class.
 * @param spi_frequency The SPI clock frequency in Hz.
 * @return Reference to the singleton instance of the AD7124 class.
 */
AD7124& AD7124::getInstance(int spi_frequency) {
    static AD7124 instance(spi_frequency);
    return instance;
}


/**
 * @brief Sends ADC data to the main thread for further processing.
 * @param byte_inputs_channel_0 Data from channel 0.
 * @param byte_inputs_channel_1 Data from channel 1.
 */
void AD7124::send_data_to_main_thread(
    std::vector<std::array<uint8_t,3>> byte_inputs_channel_0,
    std::vector<std::array<uint8_t,3>> byte_inputs_channel_1,
    unsigned int model_input_size)
{
    // Access the shared queue
    ReadingQueue& reading_queue = ReadingQueue::getInstance();

    while (!reading_queue.mail_box.empty()) {
        // Wait until mail box is empty
        thread_sleep_for(1);
        //INFO("Wait for the reading queue to become empty.\n");
    }

    // Now that we have data in the channels, let's handle mailing
    if (reading_queue.mail_box.empty()) {
        ReadingQueue::mail_t* mail = reading_queue.mail_box.try_alloc();
        
        // If both channels have reached the target size
        if ((byte_inputs_channel_0.size() == model_input_size) && (byte_inputs_channel_1.size() == model_input_size)) {
            // Prefer channel_0 first
            mail->inputs = byte_inputs_channel_0;
            byte_inputs_channel_0.clear();
            mail->channel = 0;  // Indicate the channel
            reading_queue.mail_box.put(mail); 
            
            while (!reading_queue.mail_box.empty()) {
                // Wait until first mail processed
                thread_sleep_for(1);  
            }

            // Then assign channel_1 for the next mail
            ReadingQueue::mail_t* next_mail = reading_queue.mail_box.try_alloc();
            next_mail->inputs = byte_inputs_channel_1;
                byte_inputs_channel_1.clear();
            next_mail->channel = 1;  // Indicate the channel
            
            reading_queue.mail_box.put(next_mail);  // Put the second mail (for channel_1)
        }
        // If only channel_0 has the required size, send it
        else if (byte_inputs_channel_0.size() == model_input_size) {
            mail->inputs = byte_inputs_channel_0;
            byte_inputs_channel_0.clear();
            mail->channel = 0;
            reading_queue.mail_box.put(mail);
        }
        // If only channel_1 has the required size, send it
        else if (byte_inputs_channel_1.size() == model_input_size) {
            mail->inputs = byte_inputs_channel_1;
            byte_inputs_channel_1.clear();
            mail->channel = 1;
            reading_queue.mail_box.put(mail);
        }
    }

}

/**
 * @brief Reads voltage data from both ADC channels with downsampling.
 * @param downsampling_rate The rate to downsample ADC readings (in ms).
 * @param vector_size The size of the resulting data vectors.
 */
void AD7124::read_voltage_from_both_channels(unsigned int downsampling_rate, unsigned int vector_size){

    while (true){
        std::vector<std::array<uint8_t,3>> byte_inputs_channel_0;
        std::vector<std::array<uint8_t,3>> byte_inputs_channel_1;
        
        // While the vector's size is less than 4, append 3-byte arrays
        while ((byte_inputs_channel_0.size() < vector_size) && (byte_inputs_channel_1.size() < vector_size)){
            
            while(m_drdy == 0){
                wait_us(1);
            }
            while(m_drdy == 1){
                wait_us(1);
            }

            uint8_t data[4] = {0, 0, 0, 255};
            for(int j = 0; j < 4; j++){
                // Sends 0x00 and simultaneously receives a byte from the SPI slave device.
                data[j] = m_spi.write(0x00);
            }
                        
            if(data[3] == 0){
                // data from channel 0
                if (byte_inputs_channel_0.size() < vector_size) {
                    // Add new value
                    std::array<uint8_t, 3> new_bytes = {data[0], data[1], data[2]};
                    byte_inputs_channel_0.push_back(new_bytes);
                } else {
                    // Replace the oldest value with the new value (circular buffer approach)
                    std::array<uint8_t, 3> new_bytes = {data[0], data[1], data[2]};
                    byte_inputs_channel_0.erase(byte_inputs_channel_0.begin()); // Remove the oldest element
                    byte_inputs_channel_0.push_back(new_bytes); // Add the new value
                }
            }

            if(data[3] == 1){
                // data from channel 1
                if(byte_inputs_channel_1.size() < vector_size){
                    // Add new value
                    std::array<uint8_t, 3> new_bytes = {data[0], data[1], data[2]};
                    byte_inputs_channel_1.push_back(new_bytes);
                } else{
                    // Replace the oldest value with the new value (circular buffer approach)
                    std::array<uint8_t, 3> new_bytes = {data[0], data[1], data[2]};
                    byte_inputs_channel_1.erase(byte_inputs_channel_1.begin()); // Remove the oldest element
                    byte_inputs_channel_1.push_back(new_bytes);
                }
            }
            
        }
        thread_sleep_for(1); // ms
        send_data_to_main_thread(byte_inputs_channel_0, byte_inputs_channel_1, vector_size);
    }
}