# retrogram~soapysdr 

Wideband Spectrum analyzer on your terminal/ssh console with ASCII art. 
Hacked from Ettus UHD RX ASCII Art DFT code for SoapySDR. Adapted from [retrogram~plutosdr](https://github.com/r4d10n/retrogram-plutosdr). 

![retrogram-rtlsdr-vhf](https://i.imgur.com/BGmYK5i.jpg)
[Tuning on VHF band while transmitting on 145.500 MHz with Handheld Radio]

![retrogram-rtlsdr-gsm](https://imgur.com/REhEnv2.jpg)
[Spotting FCCH (Freq. Correction) burst in the local GSM BCCH (Broadcast Control Channel) @ 938.2 MHz]

Pan & Zoom spectrum using keyboard controls [decrement-Increment]. 

* Center Frequency using keys [f-F] 
* Sampling rate    using keys [r-R]
* Reference level  using keys [l-L] 
* Dynamic Range    using keys [d-D]
* Frame rate       using keys [s-S]
* Tuning step	   using keys [t-T]

Tuning step applies for decrementing / Incrementing Center Frequency and Sampling Rate.

---

## Requires: libsoapysdr, libcurses, libboost-program-options
	
	sudo apt install libsoapysdr-dev libncurses5-dev libboost-program-options-dev

## Build:

For running on a Linux host with RTLSDR 

	make

## Run:

	./retrogram-soapysdr --dev 0 --rate 1.8e6 --freq 100e6 --step 1e5

## TODO:

* Direct Freq Entry / parameter change 
* Mouse tuning
* Modularize / Optimize with std::vector transform
* Waterfall (!) / Markers / Demodulators (!!) :)
* HTML output / Web(sockets)-server
