#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"
#include <Accelerate/Accelerate.h>

//----------------------------------------------------------------------------//

/* Ring Buffer to store audio data for STFT processing. */

class RingFifo
{
public:
    RingFifo() : abstractFifo (2048) // --> make at least 2x fft
    {
    }
    
    void addToFifo (const float* someData, int numItems)
    {
        int start1, size1, start2, size2;
        abstractFifo.prepareToWrite (numItems, start1, size1, start2, size2);
        if (size1 > 0)
            memcpy (myBuffer + start1, someData, size1 * sizeof(float));
        if (size2 > 0)
            memcpy (myBuffer + start2, someData + size1, size2 * sizeof(float));
        abstractFifo.finishedWrite (size1 + size2);
    }
    
    void readFromFifo (float* someData, int numItems)
    {
        int start1, size1, start2, size2;
        abstractFifo.prepareToRead (numItems, start1, size1, start2, size2);
        if (size1 > 0)
            memcpy (someData, myBuffer + start1, size1 * sizeof(float));
        if (size2 > 0)
            memcpy (someData + size1, myBuffer + start2, size2 * sizeof(float));
        abstractFifo.finishedRead (size1 + size2);
    }
    
    AbstractFifo abstractFifo;
    
private:
    float myBuffer[2048];
};

//----------------------------------------------------------------------------//

 /*
 
  4-channel localization using UDLR or CORNERS mic configuration.
  
  note:
  - CORNERS mode is TBD
  - size of image determines downsampling resolution
  
  features to make variable:
  - db reference & colormap scaling
  - decay beta factor
 
 */

class PanogramComp : public Component,
                     private Timer
{
    
public:
    PanogramComp (float* fftDataToUse1, float* fftDataToUse2,
                  float* fftDataToUse3, float* fftDataToUse4,
                  float fftSizeToUse)
    :
    fftData1 (fftDataToUse1),  //  0 = Up    , 1 = UpperLeft
    fftData2 (fftDataToUse2),  //  0 = Down  , 1 = UpperRight
    fftData3 (fftDataToUse3),  //  0 = Left  , 1 = LowerLeft
    fftData4 (fftDataToUse4),  //  0 = Right , 1 = LowerRight
    fftSize (fftSizeToUse),
    panogramImage (Image::RGB, 80, 80, true)
    {
        startTimerHz (30);
    }
    
    void paint (Graphics& g) override
    {
        // mode 0 UDLR
        if (mode == 0) {
            memcpy(fftDataU, fftData1, fftSize/2 * sizeof(float));
            memcpy(fftDataD, fftData2, fftSize/2 * sizeof(float));
            memcpy(fftDataL, fftData3, fftSize/2 * sizeof(float));
            memcpy(fftDataR, fftData4, fftSize/2 * sizeof(float));
        }
        
        // mode 1 CORNERS
        if (mode == 1) {
            vDSP_vadd(fftData1, 1, fftData2, 1, fftDataU, 1, fftSize/2);
            vDSP_vadd(fftData3, 1, fftData4, 1, fftDataD, 1, fftSize/2);
            vDSP_vadd(fftData1, 1, fftData3, 1, fftDataL, 1, fftSize/2);
            vDSP_vadd(fftData2, 1, fftData4, 1, fftDataR, 1, fftSize/2);
        }
        
        // compute crossover, gains
        computeCrossoverGains(fftDataL, fftDataR, gainDataX1, xoverDataX1);
        computeCrossoverGains(fftDataU, fftDataD, gainDataY1, xoverDataY1);
        
        // dbscale gains
        Float32 kZeroReference = 1.1; // <- make param
        Float32 offset = 1.0000000001;
        vDSP_vsadd(gainDataX1, 1, &offset, gainDataX1, 1, fftSize/2);
        vDSP_vdbcon(gainDataX1, 1, &kZeroReference, gainDataX1, 1, fftSize/2, 1);
        vDSP_vsadd(gainDataY1, 1, &offset, gainDataY1, 1, fftSize/2);
        vDSP_vdbcon(gainDataY1, 1, &kZeroReference, gainDataY1, 1, fftSize/2, 1);
        
        // fade image
        panogramImage.multiplyAllAlphas(0.9); // <- make param
        
        // fill image
        for (int i = 0; i < 512; i++)
        {
            int xPix = xoverDataX1[i] * panogramImage.getWidth();
            int yPix = xoverDataY1[i] * panogramImage.getHeight();
            Colour color = Colour::fromHSV ((float)i/(fftSize/2.0f), 1.0f, 1.0f, gainDataX1[i]/0.5); // <- make param
            
            panogramImage.setPixelAt (xPix, yPix, color);
        }
        
        // blackout
        g.fillAll(Colours::transparentBlack);
        
        // draw image direct to bounding rectangle UDLR mode
        if (mode == 0) {
            g.drawImageWithin (panogramImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit);
        }
        if (mode == 1) {
            g.drawImageWithin (panogramImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit);
        }
    }
    
    void computeCrossoverGains(float* fftDataL, float* fftDataR,
                               float* gainData, float* xoverData)
    {
        // gain = l + r
        vDSP_vadd(fftDataL, 1, fftDataR, 1, gainData, 1, fftSize/2);
        
        // xover = r / gain
        vDSP_vdiv(gainData, 1, fftDataR, 1, xoverData, 1, fftSize/2);
    }
    
    
    int mode = 0; // microphone spatial configuration: 0 is Up/Down/Left/Right, 1 is corners
    
private:
    
    void timerCallback() override
    {
        repaint();
    }
    
    float* fftData1;
    float* fftData2;
    float* fftData3;
    float* fftData4;
    
    float gainDataX1[512];
    float xoverDataX1[512];
    float gainDataY1[512];
    float xoverDataY1[512];
    
    float fftDataU [512];
    float fftDataD [512];
    float fftDataL [512];
    float fftDataR [512];
    
    int fftSize;
    
    Image panogramImage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanogramComp)
};


//----------------------------------------------------------------------------//

 /*

 Divides all 9-channels into overlapping panograms.
 
 note:
 - currently only using ONE panogram: channels 6,4,2,8 in UPLR mode
 
 */

class DualPanogramComp : public Component,
                         private Timer
{
public:
    DualPanogramComp (float* fftDataToUse1, float* fftDataToUse2, float* fftDataToUse3,
                      float* fftDataToUse4, float* fftDataToUse5, float* fftDataToUse6,
                      float* fftDataToUse7, float* fftDataToUse8, float* fftDataToUse9,
                      float fftSizeToUse)
    :
    panogram1 (fftDataToUse6, fftDataToUse4, fftDataToUse2, fftDataToUse8, fftSizeToUse), // UDLR
    panogram2 (fftDataToUse3, fftDataToUse7, fftDataToUse1, fftDataToUse9, fftSizeToUse)  // CORNERS
    {
        addAndMakeVisible (&panogram1);
        // addAndMakeVisible (&panogram2);
        
        // set modes
        panogram1.mode = 0;  // UDLR
        panogram2.mode = 1;  // corners
        
        // FERP map image
        FERPMap = ImageCache::getFromFile(File("/Users/davidkant/Develop/JUCE Projects/MultiLocalizer-Release/img/FERP plot locations inverted.jpg"));
        FERPMap.multiplyAllAlphas(0.55);
    }
    
    void resized() override
    {
        // main display region
        const Rectangle<int> panogramBounds  (5, 5, getWidth()-10, getWidth()-10);

        // overlap two panograms in main region
        panogram1.setBounds (panogramBounds);
        panogram2.setBounds (panogramBounds);
    }
    
    void paint (Graphics& g) override
    {
        g.drawImageWithin (FERPMap, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit);
    }
    
private:
    
    void timerCallback() override
    {
        repaint();
    }
    
    PanogramComp panogram1;
    PanogramComp panogram2;
    
    Image FERPMap;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualPanogramComp)
};

//----------------------------------------------------------------------------//

/* Main window component */

class MainContentComponent : public AudioAppComponent,
                             public Timer
{
public:
    MainContentComponent()
    :
    dualPanogramComp(fftCookedData1, fftCookedData2, fftCookedData3,
                     fftCookedData4, fftCookedData5, fftCookedData6,
                     fftCookedData7, fftCookedData8, fftCookedData9,
                     fftSize),
    forwardFFT (fftOrder, false),
    nextFFTBlockReady (false)
    {
        setLookAndFeel (&lookAndFeel);
        setSize (400, 400);

        addAndMakeVisible (&dualPanogramComp);
        addAndMakeVisible (audioSetupComp = new AudioDeviceSelectorComponent (deviceManager, 0, 256, 0, 256, true, true, true, false));
        
        // init fftWindow
        fftWindowScale = 0.5; // changes for hop != fft
        vDSP_hann_window(fftWindow, fftSize, vDSP_HANN_DENORM);
        vDSP_vsmul(fftWindow, 1, &fftWindowScale, fftWindow, 1, fftSize);
        
        // audio setup
        setAudioChannels (2, 2); // (numIn, numOut)

        // initialize to soundflower 64
        deviceManager.initialise (9, 0, nullptr, true, "Soundflower (64ch)");
    }
    
    ~MainContentComponent()
    {
        shutdownAudio();
    }
    
    //=======================================================================
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        
    }

    void releaseResources() override
    {
    
    }
    
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        // copy audio from input stream
        ringBuffer1.addToFifo (bufferToFill.buffer->getReadPointer(0), bufferToFill.numSamples);
        ringBuffer2.addToFifo (bufferToFill.buffer->getReadPointer(1), bufferToFill.numSamples);
        ringBuffer3.addToFifo (bufferToFill.buffer->getReadPointer(2), bufferToFill.numSamples);
        ringBuffer4.addToFifo (bufferToFill.buffer->getReadPointer(3), bufferToFill.numSamples);
        ringBuffer5.addToFifo (bufferToFill.buffer->getReadPointer(4), bufferToFill.numSamples);
        ringBuffer6.addToFifo (bufferToFill.buffer->getReadPointer(5), bufferToFill.numSamples);
        ringBuffer7.addToFifo (bufferToFill.buffer->getReadPointer(6), bufferToFill.numSamples);
        ringBuffer8.addToFifo (bufferToFill.buffer->getReadPointer(7), bufferToFill.numSamples);
        ringBuffer9.addToFifo (bufferToFill.buffer->getReadPointer(8), bufferToFill.numSamples);
        
        // if enough data in ring buffer to perform FFT
        if (ringBuffer1.abstractFifo.getNumReady () >= fftSize)
        {
            // copy from ring buffer to FFT buffer
            ringBuffer1.readFromFifo (fftCalcBuffer1, fftSize);
            ringBuffer2.readFromFifo (fftCalcBuffer2, fftSize);
            ringBuffer3.readFromFifo (fftCalcBuffer3, fftSize);
            ringBuffer4.readFromFifo (fftCalcBuffer4, fftSize);
            ringBuffer5.readFromFifo (fftCalcBuffer5, fftSize);
            ringBuffer6.readFromFifo (fftCalcBuffer6, fftSize);
            ringBuffer7.readFromFifo (fftCalcBuffer7, fftSize);
            ringBuffer8.readFromFifo (fftCalcBuffer8, fftSize);
            ringBuffer9.readFromFifo (fftCalcBuffer9, fftSize);
            
            // window
            vDSP_vmul(fftCalcBuffer1, 1, fftWindow, 1, fftCalcBuffer1, 1, fftSize);
            vDSP_vmul(fftCalcBuffer2, 1, fftWindow, 1, fftCalcBuffer2, 1, fftSize);
            vDSP_vmul(fftCalcBuffer3, 1, fftWindow, 1, fftCalcBuffer3, 1, fftSize);
            vDSP_vmul(fftCalcBuffer4, 1, fftWindow, 1, fftCalcBuffer4, 1, fftSize);
            vDSP_vmul(fftCalcBuffer5, 1, fftWindow, 1, fftCalcBuffer5, 1, fftSize);
            vDSP_vmul(fftCalcBuffer6, 1, fftWindow, 1, fftCalcBuffer6, 1, fftSize);
            vDSP_vmul(fftCalcBuffer7, 1, fftWindow, 1, fftCalcBuffer7, 1, fftSize);
            vDSP_vmul(fftCalcBuffer8, 1, fftWindow, 1, fftCalcBuffer8, 1, fftSize);
            vDSP_vmul(fftCalcBuffer9, 1, fftWindow, 1, fftCalcBuffer9, 1, fftSize);
            
            // perform forward FFT
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer1);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer2);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer3);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer4);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer5);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer6);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer7);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer8);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer9);
            
            // copy computed data to shared FFT buffer
            memcpy(fftCookedData1, fftCalcBuffer1, fftSize * sizeof(float));
            memcpy(fftCookedData2, fftCalcBuffer2, fftSize * sizeof(float));
            memcpy(fftCookedData3, fftCalcBuffer3, fftSize * sizeof(float));
            memcpy(fftCookedData4, fftCalcBuffer4, fftSize * sizeof(float));
            memcpy(fftCookedData5, fftCalcBuffer5, fftSize * sizeof(float));
            memcpy(fftCookedData6, fftCalcBuffer6, fftSize * sizeof(float));
            memcpy(fftCookedData7, fftCalcBuffer7, fftSize * sizeof(float));
            memcpy(fftCookedData8, fftCalcBuffer8, fftSize * sizeof(float));
            memcpy(fftCookedData9, fftCalcBuffer9, fftSize * sizeof(float));
        }
    }
    
    //=======================================================================
    
    void resized() override
    {
        // get dimensions from width and center with 10 pix padding to edge
        const Rectangle<int> multiPanogramCompBounds ((getWidth()-getHeight())/2+10, 10, getHeight()-20, getHeight()-20);
        dualPanogramComp.setBounds (multiPanogramCompBounds);
        
        //Rectangle<int> r (getLocalBounds().reduced (4));
        //audioSetupComp->setBounds (r.removeFromTop (proportionOfHeight (0.65f)));
    }

    //=======================================================================
    
    void timerCallback() override
    {
    }
    
    //=======================================================================

    void paint (Graphics& g) override
    {
        // white border rectangle
        // g.setColour(Colours::white);
        // g.drawRect(1, 1, getWidth()-2, getHeight()-2);
    }
    
private:

    enum
    {
        fftOrder = 10,  // 10 = 1024, 11 = 2048
        fftSize = 1 << fftOrder
    };
    
    //==========================================================================

    DualPanogramComp dualPanogramComp;
    
    AudioSampleBuffer tempBuffer;
    
    // FFT stuff
    float fftWindow [fftSize];
    float fftWindowScale;

    RingFifo ringBuffer1;
    float fftCalcBuffer1 [2 * fftSize];
    float fftCookedData1 [fftSize];

    RingFifo ringBuffer2;
    float fftCalcBuffer2 [2 * fftSize];
    float fftCookedData2 [fftSize];

    RingFifo ringBuffer3;
    float fftCalcBuffer3 [2 * fftSize];
    float fftCookedData3 [fftSize];
    
    RingFifo ringBuffer4;
    float fftCalcBuffer4 [2 * fftSize];
    float fftCookedData4 [fftSize];
    
    RingFifo ringBuffer5;
    float fftCalcBuffer5 [2 * fftSize];
    float fftCookedData5 [fftSize];
    
    RingFifo ringBuffer6;
    float fftCalcBuffer6 [2 * fftSize];
    float fftCookedData6 [fftSize];
    
    RingFifo ringBuffer7;
    float fftCalcBuffer7 [2 * fftSize];
    float fftCookedData7 [fftSize];
    
    RingFifo ringBuffer8;
    float fftCalcBuffer8 [2 * fftSize];
    float fftCookedData8 [fftSize];

    RingFifo ringBuffer9;
    float fftCalcBuffer9 [2 * fftSize];
    float fftCookedData9 [fftSize];

    FFT forwardFFT;
    bool nextFFTBlockReady;

    ScopedPointer<AudioDeviceSelectorComponent> audioSetupComp;
    
    LookAndFeel_V3 lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }

#endif  // MAINCOMPONENT_H_INCLUDED