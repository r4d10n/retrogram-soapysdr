/*

          _                                      /\/|____                         _________________
         | |                                    |/\/  ___|                       /  ___|  _  \ ___ \
 _ __ ___| |_ _ __ ___   __ _ _ __ __ _ _ __ ___   \ `--.  ___   __ _ _ __  _   _\ `--.| | | | |_/ /
| '__/ _ \ __| '__/ _ \ / _` | '__/ _` | '_ ` _ \   `--. \/ _ \ / _` | '_ \| | | |`--. \ | | |    /
| | |  __/ |_| | | (_) | (_| | | | (_| | | | | | | /\__/ / (_) | (_| | |_) | |_| /\__/ / |/ /| |\ \
|_|  \___|\__|_|  \___/ \__, |_|  \__,_|_| |_| |_| \____/ \___/ \__,_| .__/ \__, \____/|___/ \_| \_|
                         __/ |                                       | |     __/ |
                        |___/                                        |_|    |___/

Wideband Spectrum analyzer on your terminal/ssh console with ASCII art.
Hacked from Ettus UHD RX ASCII Art DFT code - adapted for SoapySDR.
Peak Hold feature by henrikssn [https://github.com/henrikssn]

*/

//
// Copyright 2010-2011,2014 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//


#include "ascii_art_dft.hpp" //implementation
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <curses.h>
#include <iostream>
#include <complex>
#include <cstdlib>
#include <chrono>
#include <thread>

#include <SoapySDR/Version.h>
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>

#define EXIT_ON_ERR false
#define DISABLE_STDERR true

namespace po = boost::program_options;
using std::chrono::high_resolution_clock;


void exiterr(int retcode)
{
    if (EXIT_ON_ERR) exit(retcode);
}


int main(int argc, char *argv[]){

    //variables to be set by po
    int  dev_id;

    int num_bins = 512;
    double rate, freq, step, frame_rate;
    float ref_lvl, dyn_rng;
    bool show_controls;
    bool peak_hold;

    int ch;
    bool loop = true;

    //setup the program options
    po::options_description desc("\nAllowed options");
    desc.add_options()
        ("help", "help message")
        ("dev", po::value<int>(&dev_id)->default_value(0), "soapysdr device index")
        // hardware parameters
        ("rate", po::value<double>(&rate)->default_value(1e6), "rate of incoming samples (sps) [r-R]")
        ("freq", po::value<double>(&freq)->default_value(100e6), "RF center frequency in Hz [f-F]")
        //("gain", po::value<double>(&gain)->default_value(0), "gain for the RF chain [g-G]")        // TODO:Gain handling
        // display parameters
        ("frame-rate", po::value<double>(&frame_rate)->default_value(15), "frame rate of the display (fps) [s-S]")
        ("peak-hold", po::value<bool>(&peak_hold)->default_value(false), "enable peak hold [h-H]")
        ("ref-lvl", po::value<float>(&ref_lvl)->default_value(0), "reference level for the display (dB) [l-L]")
        ("dyn-rng", po::value<float>(&dyn_rng)->default_value(80), "dynamic range for the display (dB) [d-D]")
        ("step", po::value<double>(&step)->default_value(1e5), "tuning step for rate/bw/freq [t-T]")
        ("show-controls", po::value<bool>(&show_controls)->default_value(true), "show the keyboard controls")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    std::cout << boost::format("retrogram~soapysdr - ASCII Art Spectrum Analysis for SoapySDR") << std::endl;

    //print the help message
    if (vm.count("help") or vm.count("h")){
        std::cout << boost::format("%s") % desc << std::endl;
        return EXIT_FAILURE;
    }

    //enumerate devices
    size_t length;
    SoapySDRKwargs *results = SoapySDRDevice_enumerate(NULL, &length);

    for (size_t i = 0; i < length; i++)
    {
        std::cout << boost::format("Device found: [%d] ") % ((int)i) << std::endl;

        for (size_t j = 0; j < results[i].size; j++)
        {
            std::cout << boost::format("%s=%s, ") % (results[i].keys[j]) % (results[i].vals[j]);
        }
        std::cout << std::endl;
    }

    std::cout << std::endl << boost::format("Creating the Soapysdr instance from device:[%s]...") % dev_id << std::endl << std::endl;

    SoapySDRDevice *sdr = SoapySDRDevice_make((&results[dev_id]));
    SoapySDRKwargsList_clear(results, length);

    if (sdr == NULL) {
        std::cout << boost::format("SoapySDRDevice_make fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
        return EXIT_FAILURE;
    }

    //set the sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate/1e6) << std::endl;

    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, rate) != 0)
    {
        std::cout << boost::format("setSampleRate fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
        exiterr(0);
    }

    //set the center frequency
    std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq/1e6) << std::endl;

    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
    {
        std::cout << boost::format("setFrequency fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
        exiterr(0);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1)); //allow for some setup time


    //setup a stream (complex floats)
    SoapySDRStream *rxStream;
    int ret = 0;
	
    #if SOAPY_SDR_API_VERSION >= 0x00080000		// API version 0.8
	#undef SoapySDRDevice_setupStream
	rxStream = SoapySDRDevice_setupStream(sdr, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL);
	ret = (rxStream == NULL);
    #else // API version 0.7 
	ret = SoapySDRDevice_setupStream(sdr, &rxStream, SOAPY_SDR_RX, SOAPY_SDR_CS16, NULL, 0, NULL);
    #endif

    if (ret != 0)
    {
        std::cout << boost::format("setupStream fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
        exiterr(0);
    }

    if (SoapySDRDevice_activateStream(sdr, rxStream, 0, 0, 0) != 0) //start streaming
    {
        std::cout << boost::format("activateStream failed: %s") % (SoapySDRDevice_lastError()) << std::endl;
        exiterr(0);
    }

    std::vector<std::complex<float> > buff(num_bins);

    //_Complex float buffer[num_bins * 2];
    int samples_per_buffer = num_bins * 2; //fix for int16 storage
    size_t elemsize = SoapySDR_formatToSize(SOAPY_SDR_CS16);
    int16_t *buffer = (int16_t *)malloc(samples_per_buffer * elemsize);

    void *buffs[] = {buffer}; //array of buffers
    int flags; //flags set by receive operation
    long long timeNs; //timestamp for receive buffer


    //------------------------------------------------------------------
    //-- Initialize
    //------------------------------------------------------------------
    initscr();

    auto next_refresh = high_resolution_clock::now();

    //disable stderr on ncurses screen
    if (DISABLE_STDERR) freopen("/dev/null", "w", stderr);

    //------------------------------------------------------------------
    //-- Main loop
    //------------------------------------------------------------------

    ascii_art_dft::log_pwr_dft_type last_lpdft;

    while (loop)
    {

        buff.clear();

        SoapySDRDevice_readStream(sdr, rxStream, buffs, samples_per_buffer, &flags, &timeNs, 100000);

        // TODO: Check sample buffer
    	/*if (n_read < samples_per_buffer)
        {
                fprintf(stderr, "Short read, samples lost, exiting - %d:%d!\n", n_read, samples_per_buffer);
                break;
        }*/

        //TODO: Find accurate scaling factor for all devices (rtlsdr: 3000, pluto:2048)
        for(int j = 0; j < samples_per_buffer; j+=2)
        {
            buff.push_back(std::complex<float> ( (float)(buffer[j]/3000.0), (float)(buffer[j+1]/3000.0)));
        }

         // Return early to save CPU if peak hold is disabled and no refresh is required.
        if (!peak_hold && high_resolution_clock::now() < next_refresh) {
            continue;
        }

        //calculate the dft and create the ascii art frame
        ascii_art_dft::log_pwr_dft_type lpdft(
            ascii_art_dft::log_pwr_dft(&buff.front(), buff.size())
        );

        // For peak hold, compute the max of last DFT and current one
        if (peak_hold && last_lpdft.size() == lpdft.size()) {
            for (size_t i = 0; i < lpdft.size(); ++i) {
                lpdft[i] = std::max(lpdft[i], last_lpdft[i]);
            }
        }
        last_lpdft = lpdft;


        //check and update the display refresh condition
        if (high_resolution_clock::now() < next_refresh) {
            continue;
        }

        next_refresh =
            high_resolution_clock::now()
            + std::chrono::microseconds(int64_t(1e6/frame_rate));
        
        std::string frame = ascii_art_dft::dft_to_plot(
            lpdft, COLS, (show_controls ? LINES-5 : LINES),
            rate,
            freq,
            dyn_rng, ref_lvl
        );

        std::string header = std::string((COLS-26)/2, '-');
    	std::string border = std::string((COLS), '-');

        //curses screen handling: clear and print frame
        clear();
        if (show_controls)
        {
            printw("-%s-={ retrogram~soapysdr }=-%s",header.c_str(),header.c_str());
            printw("[f-F]req: %4.3f MHz   |   [r-R]ate: %2.2f Msps ", freq/1e6, rate/1e6);
            printw("   |    Peak [h-H]hold: %s\n\n", peak_hold ? "On" : "Off");            
            printw("[d-D]yn Range: %2.0f dB    |   Ref [l-L]evel: %2.0f dB   |   fp[s-S] : %2.0f   |   [t-T]uning step: %3.3f M\n", dyn_rng, ref_lvl, frame_rate, step/1e6);
            printw("%s", border.c_str());
        }
        printw("%s\n", frame.c_str());

        //curses key handling: no timeout, any key to exit. TODO: Proper bounds check and limit
        timeout(0);
        ch = getch();

        switch(ch)
        {
            case 'r':
            {
                if ((rate - step) > 0)
                {
                    rate -= step;
                    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, rate) != 0)
                    {
                        std::cout << boost::format("setSampleRate fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                        exiterr(0);
                    }

                }
                break;
            }

            case 'R':
            {
                    rate += step;
                    if (SoapySDRDevice_setSampleRate(sdr, SOAPY_SDR_RX, 0, rate) != 0)
                    {
                        std::cout << boost::format("setSampleRate fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                        exiterr(0);
                    }
                    break;
            }

            case 'f':
            {
                freq -= step;
                if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
                {
                    std::cout << boost::format("setFrequency fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                    exiterr(0);
                }
                break;
            }

            case 'F':
            {
                freq += step;
                if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
                {
                    std::cout << boost::format("setFrequency fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                    exiterr(0);
                }
                break;
            }
            
            case 'h': { peak_hold = false; break; }
            case 'H': { peak_hold = true; break; }
            case 'l': { ref_lvl -= 10; break; }
            case 'L': { ref_lvl += 10; break; }
            case 'd': { dyn_rng -= 10; break; }
            case 'D': { dyn_rng += 10; break; }
            case 's': { if (frame_rate > 1) frame_rate -= 1; break;}
            case 'S': { frame_rate += 1; break; }
            case 't': { if (step > 1) step /= 2; break; }
            case 'T': { step *= 2; break; }
            case 'c': { show_controls = false; break; }
            case 'C': { show_controls = true; break; }

            case 'q':
            case 'Q': { loop = false; break; }

        }

        if (ch == '\033')    // '\033' '[' 'A'/'B'/'C'/'D' -- Up / Down / Right / Left Press
        {
            getch();
            switch(getch())
            {
		        case 'A':
                case 'C':
                    freq += step;

                    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
                    {
                        std::cout << boost::format("setFrequency fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                        exiterr(0);
                    }

                    break;

		        case 'B':
                case 'D':
                    freq -= step;

                    if (SoapySDRDevice_setFrequency(sdr, SOAPY_SDR_RX, 0, freq, NULL) != 0)
                    {
                        std::cout << boost::format("setFrequency fail: %s") % (SoapySDRDevice_lastError()) << std::endl;
                        exiterr(0);
                    }

                    break;
            }
        }
    }

    //------------------------------------------------------------------
    //-- Cleanup
    //------------------------------------------------------------------

    //shutdown the stream
    SoapySDRDevice_deactivateStream(sdr, rxStream, 0, 0); //stop streaming
    SoapySDRDevice_closeStream(sdr, rxStream);

    //cleanup device handle
    SoapySDRDevice_unmake(sdr);

    curs_set(true);
    endwin(); //curses done

    //finished
    std::cout << std::endl << (char)(ch) << std::endl << "Done!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}
