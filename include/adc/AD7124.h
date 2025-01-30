
#ifndef ADC_PROCESS_H_
#define ADC_PROCESS_H_

// Required for SPI, DigitalIn, DigitalOut, NonCopyable
// Handle Body idiom could be applied to remove mbed.h
// from header entirely
#include "mbed.h"   

/**
 * @class AD7124
 * @brief Singleton class for interfacing with the AD7124 using SPI.
 *
 * The AD7124 class provides methods for initializing and interacting with the AD7124
 * Analog-to-Digital Converter (ADC) via SPI. It supports configuration of ADC channels,
 * reading voltage data, and resetting or controlling the device.
 */
class AD7124: private mbed::NonCopyable<AD7124>{
    public:
        /**
         * @brief Gets the singleton instance of the AD7124 class.
         * @param spi_frequency The SPI clock frequency in Hz.
         * @return Reference to the singleton instance of the AD7124 class.
         */
        static AD7124& getInstance(int spi_frequency);

        // Deleted copy constructor to prevent copying of the singleton instance.
        AD7124(const AD7124&) = delete;

        /// Deleted copy assignment operator to prevent copying of the singleton instance.
        AD7124& operator=(const AD7124&) = delete;

        /**
         * @brief Reads voltage data from both ADC channels.
         */
        void read_voltage_from_both_channels(unsigned int downsampling_rate, unsigned int vector_size);

    private:

        SPI         m_spi;              ///< SPI object for communication with the AD7124.
        DigitalIn   m_drdy;
        DigitalOut  m_cs;
        DigitalOut  m_sync;             
        int         m_spi_frequency;   ///< SPI clock frequency in Hz. 
        int         m_flag_0;
        int         m_flag_1;
        char        m_read;
        char        m_write;

        /**
        * @brief Private constructor for the AD7124 class.
        * @param spi_frequency The SPI clock frequency in Hz.
        */
        AD7124(int spi_frequency);

        /**
         * @brief Initializes the AD7124 ADC with specific channel flags.
         */
        void init(bool f0, bool f1);
 
        /**
         * @brief Resets the AD7124 device.
         */
        void reset(void);

        /**
         * @brief Retrieves the status of the AD7124 device.
         * @return Status byte of the AD7124 device.
         */
        char status(void);

        /**
         * @brief Configures the channel register.
         * @param RW Read (R) or Write (W) operation indicator.
         */
        void channel_reg(char RW);

        /**
         * @brief Configures a register by address.
         * @param address Register address to configure.
         * @param RW Read (R) or Write (W) operation indicator.
         */
        void config_reg(uint8_t address, char RW);

        /**
         * @brief Configures the filter register.
         * @param filt Filter value to set.
         * @param RW Read (R) or Write (W) operation indicator.
         */
        void filter_reg(uint8_t filt, char RW);

        /**
         * @brief Configures the control register.
         * @param RW Read (R) or Write (W) operation indicator.
         */
        void ctrl_reg(char RW);

        /**
         * @brief Sends data to the main thread for processing.
         * @param byte_inputs_channel_0 Byte array inputs for channel 0.
         * @param byte_inputs_channel_1 Byte array inputs for channel 1.
         */
        void send_data_to_main_thread(
            std::vector<std::array<uint8_t, 3>> byte_inputs_channel_0,
            std::vector<std::array<uint8_t, 3>> byte_inputs_channel_1
        );
};
#endif
