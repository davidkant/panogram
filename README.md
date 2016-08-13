#Panogram
---

This is a very basic proof of concept test app for multichannel localization.
 
Starting with 4-channel localization, done stupid simple. But we can easily drop more sophisticated code into the visualization component now that we have the basics of audio and visualization up and running. 

My recommendation if you want to try a new technique is make a new panogram component and drop it in in place of the current "QuadPanogramComp".

Don't trust anything you see here; this is not the correct way to code anything!
 
__- dk, 2016__
 
###todo:
* dynamic memory management
* variable fft size (will break lots of initializations)
* UI display paramaeters for gain scaling and display range
* UI select which audio channels output (currently output is 2-channel)
* newFFTData flag so don't redraw old data
* JUCE cross-platform equivalents for accelerate?
* numBins so not fft/2 all the time
* quad audio output

#####__#madewithJUCE__
#####__#envirosoundlab__
