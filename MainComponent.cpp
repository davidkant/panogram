#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class SimpleThumbnailComponent : public Component,
private ChangeListener
{
public:
    SimpleThumbnailComponent (int sourceSamplesPerThumbnailSample,
                              AudioFormatManager& formatManager,
                              AudioThumbnailCache& cache)
    : thumbnail (sourceSamplesPerThumbnailSample, formatManager, cache)
    {
        thumbnail.addChangeListener (this);
    }
    
    void setFile (const File& file)
    {
        thumbnail.setSource (new FileInputSource (file));
    }
    
    void paint (Graphics& g) override
    {
        if (thumbnail.getNumChannels() == 0)
            paintIfNoFileLoaded (g);
        else
            paintIfFileLoaded (g);
    }
    
    void paintIfNoFileLoaded (Graphics& g)
    {
        g.fillAll (Colours::white);
        g.setColour (Colours::darkgrey);
        g.drawFittedText ("No File Loaded", getLocalBounds(), Justification::centred, 1.0f);
    }
    
    void paintIfFileLoaded (Graphics& g)
    {
        g.fillAll(Colours::white);
        
        g.setColour (Colours::grey);
        thumbnail.drawChannels (g, getLocalBounds(), 0.0, thumbnail.getTotalLength(), 1.0f);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            thumbnailChanged();
    }
    
private:
    void thumbnailChanged()
    {
        repaint();
    }
    
    AudioThumbnail thumbnail;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleThumbnailComponent)
};

//------------------------------------------------------------------------------

class SimplePositionOverlay : public Component,
private Timer
{
public:
    SimplePositionOverlay (AudioTransportSource& transportSourceToUse)
    : transportSource (transportSourceToUse)
    {
        startTimer (40);
    }
    
    void paint (Graphics& g) override
    {
        const double duration = transportSource.getLengthInSeconds();
        
        if (duration > 0.0)
        {
            const double audioPosition = transportSource.getCurrentPosition();
            const float drawPosition = (audioPosition / duration) * getWidth();
            
            g.setColour (Colours::red);
            g.drawLine (drawPosition, 0.0f, drawPosition, (float) getHeight(), 2.0f);
        }
    }
    
    void mouseDown (const MouseEvent& event) override
    {
        const double duration = transportSource.getLengthInSeconds();
        
        if (duration > 0.0)
        {
            const double clickPosition = event.position.x;
            const double audioPosition = (clickPosition / getWidth()) * duration;
            
            transportSource.setPosition (audioPosition);
        }
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    AudioTransportSource& transportSource;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimplePositionOverlay)
};

//------------------------------------------------------------------------------

class RingFifo
{
public:
    RingFifo()  : abstractFifo (2048) // --> make at least 2x fft 1x didn't work...
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
//------------------------------------------------------------------------------

/* this displays tick marks and other freq spectrum plot stuff */

class SimpleFreqSpectrumBackground : public Component,
private Timer
{
public:
    SimpleFreqSpectrumBackground ()
    {
        startTimer (40);
    }
    
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black);
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleFreqSpectrumBackground)
};

//------------------------------------------------------------------------------

/* a very simple frequency spectrum plot */

class SimpleFreqSpectrum : public Component,
private Timer
{
public:
    SimpleFreqSpectrum (float* fftDataToUse, float fftSizeToUse, Colour spectrumColourToUse)
    : fftData (fftDataToUse),
    fftSize (fftSizeToUse),
    spectrumColour (spectrumColourToUse)
    {
        startTimerHz (60);
    }
    
    void paint (Graphics& g) override
    {
        // create graphics from image
        // Graphics g (spectrogramImage);
        
        // fade previous image
        // spectrogramImage.multiplyAllAlphas(0.5);
        
        // black background
        // g.fillAll (Colours::black);

        // create path
        Path spectrumPath;
        spectrumPath.clear();
        spectrumPath.startNewSubPath (0, getHeight());
     
        // fill path
        for (int x = 0; x < getWidth(); x++)
        {
            // log freq scale
            const float skewedProportionY = 1.0f - std::exp (std::log (x / (float) getWidth()) * 0.3f);
            const int fftDataIndex = jlimit (0, fftSize / 2, (int) (skewedProportionY * fftSize / 2));
            
            // lin freq scale
            // const int fftDataIndex = int((float(x) / getWidth()) * (fftSize/2));
            
            // level
            const float level = fftData[fftDataIndex] / 10.0;
            
            // add path
            spectrumPath.lineTo(getWidth() - x, (1 - level) * getHeight());
        }
     
        // close path
        spectrumPath.closeSubPath();
     
        // render path
        g.setColour (spectrumColour);
        g.setOpacity (0.666);
        g.strokePath (spectrumPath, PathStrokeType(1.5));
        g.setOpacity (0.333);
        g.fillPath (spectrumPath);
        
        // draw image
        // gi.drawImageWithin (spectrogramImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit);
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    float* fftData;
    int fftSize;
    Colour spectrumColour;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleFreqSpectrum)
};

//------------------------------------------------------------------------------

/* a very simple spectrogram plot */

class SimpleSpectrogram : public Component,
private Timer
{
public:
    SimpleSpectrogram (float* fftDataToUse, float fftSizeToUse)
    : fftData(fftDataToUse),
    fftSize (fftSizeToUse),
    spectrogramImage (Image::RGB, 512, 512, true)
    {
        startTimer (40);
    }
    
    void paint (Graphics& g) override
    {
        const int rightHandEdge = spectrogramImage.getWidth() - 1;
        const int imageHeight = spectrogramImage.getHeight();
        
        // first, shuffle our image leftwards by 1 pixel
        spectrogramImage.moveImageSection (0, 0, 1, 0, rightHandEdge, imageHeight);

        // find the range of values produced
        // Range<float> maxLevel = FloatVectorOperations::findMinAndMax (fftData, fftSize / 2);
        
        // fill image
        for (int y = 1; y < imageHeight; ++y)
        {
            const float skewedProportionY = 1.0f - std::exp (std::log (y / (float) imageHeight) * 0.2f);
            const int fftDataIndex = jlimit (0, fftSize / 2, (int) (skewedProportionY * fftSize / 2));
            // const float level = jmap (fftCalcBuffer[fftDataIndex], 0.0f, maxLevel.getEnd(), 0.0f, 1.0f);
            const float level = fftData[fftDataIndex] / 50.0;
            
            spectrogramImage.setPixelAt (rightHandEdge, y, Colour::fromHSV (level, 1.0f, level, 1.0f));
        }
        
        // draw image
        g.drawImageWithin (spectrogramImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit);
    }
    
private:
    void timerCallback() override
    {
        repaint();
    }
    
    float* fftData;
    int fftSize;
    Image spectrogramImage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleSpectrogram)
};

//------------------------------------------------------------------------------

class MainContentComponent   : public AudioAppComponent,
public ChangeListener,
public Button::Listener,
public Timer
{
public:
    
    MainContentComponent()
    :   state (Stopped),
    thumbnailCache (5),
    thumbnailComp (512, formatManager, thumbnailCache),
    positionOverlay (transportSource),
    freqSpectrumComp1(fftCookedData1, fftSize, Colours::magenta),
    freqSpectrumComp2(fftCookedData2, fftSize, Colours::cyan),
    freqSpectrumComp3(fftCookedData3, fftSize, Colours::yellow),
    freqSpectrumComp4(fftCookedData4, fftSize, Colours::grey),
    freqSpectrumBackground(),
    forwardFFT (fftOrder, false),
    nextFFTBlockReady (false)
    {
        setLookAndFeel (&lookAndFeel);
        
        addAndMakeVisible (&openButton);
        openButton.setButtonText ("Open");
        openButton.addListener (this);
        
        addAndMakeVisible (&playButton);
        playButton.setButtonText ("Play");
        playButton.addListener (this);
        playButton.setColour (TextButton::buttonColourId, Colours::green);
        playButton.setEnabled (false);
        
        addAndMakeVisible (&stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.addListener (this);
        stopButton.setColour (TextButton::buttonColourId, Colours::red);
        stopButton.setEnabled (false);
        
        addAndMakeVisible (&loopingToggle);
        loopingToggle.setButtonText ("Loop");
        loopingToggle.addListener (this);
        
        addAndMakeVisible (&currentPositionLabel);
        currentPositionLabel.setText ("Stopped", dontSendNotification);
        
        addAndMakeVisible (&thumbnailComp);
        addAndMakeVisible (&positionOverlay);
        addAndMakeVisible (&freqSpectrumBackground);
        addAndMakeVisible (&freqSpectrumComp1);
        addAndMakeVisible (&freqSpectrumComp2);
        addAndMakeVisible (&freqSpectrumComp3);
        addAndMakeVisible (&freqSpectrumComp4);
        
        setSize (500, 420);
        
        formatManager.registerBasicFormats();
        transportSource.addChangeListener (this);
        
        setAudioChannels (2, 2);
        startTimer (20);
    }
    
    ~MainContentComponent()
    {
        shutdownAudio();
    }
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
        String message;
        message << "Preparing to play audio..." << newLine;
        message << " samplesPerBlockExpected = " << samplesPerBlockExpected << newLine;
        message << " sampleRate = " << sampleRate;
        Logger::getCurrentLogger()->writeToLog (message);
    }
    
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        // if no audio source clear buffer and return
        if (readerSource == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }
        
        // create and fill temp buffer with 4-channel audio
        tempBuffer.setSize (4, bufferToFill.buffer->getNumSamples());
        AudioSourceChannelInfo tempBufferInfo (&tempBuffer, 0, bufferToFill.numSamples);
        transportSource.getNextAudioBlock (tempBufferInfo);
        
        // copy from temp ch 0 to ring buffer
        ringBuffer1.addToFifo (tempBufferInfo.buffer->getReadPointer(0), bufferToFill.numSamples);
        ringBuffer2.addToFifo (tempBufferInfo.buffer->getReadPointer(1), bufferToFill.numSamples);
        ringBuffer3.addToFifo (tempBufferInfo.buffer->getReadPointer(2), bufferToFill.numSamples);
        ringBuffer4.addToFifo (tempBufferInfo.buffer->getReadPointer(3), bufferToFill.numSamples);
        
        // copy audio from input stream
        // ringBuffer.addToFifo(bufferToFill.buffer->getReadPointer(0), bufferToFill.numSamples);
        
        // if enough data in ring buffer to perform FFT
        if (ringBuffer1.abstractFifo.getNumReady () >= fftSize)
        {
            // copy from ring buffer to FFT buffer
            ringBuffer1.readFromFifo (fftCalcBuffer1, fftSize);
            ringBuffer2.readFromFifo (fftCalcBuffer2, fftSize);
            ringBuffer3.readFromFifo (fftCalcBuffer3, fftSize);
            ringBuffer4.readFromFifo (fftCalcBuffer4, fftSize);

            // perform forward FFT
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer1);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer2);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer3);
            forwardFFT.performFrequencyOnlyForwardTransform (fftCalcBuffer4);
            
            // copy computed data to shared FFT buffer
            memcpy(fftCookedData1, fftCalcBuffer1, fftSize * sizeof(float));
            memcpy(fftCookedData2, fftCalcBuffer2, fftSize * sizeof(float));
            memcpy(fftCookedData3, fftCalcBuffer3, fftSize * sizeof(float));
            memcpy(fftCookedData4, fftCalcBuffer4, fftSize * sizeof(float));
        }
    
        // copy channels 3 and 4 to output buffer
        bufferToFill.buffer->copyFrom (0, bufferToFill.startSample, tempBuffer, 2, 0, bufferToFill.numSamples);
        bufferToFill.buffer->copyFrom (1, bufferToFill.startSample, tempBuffer, 3, 0, bufferToFill.numSamples);
    }
    
    void releaseResources() override
    {
        transportSource.releaseResources();
    }
    
    void resized() override
    {
        openButton.setBounds (10, 10, 50, 20);
        playButton.setBounds (65, 10, 50, 20);
        stopButton.setBounds (120, 10, 50, 20);
        loopingToggle.setBounds(175, 10, 50, 20);
        currentPositionLabel.setBounds (230, 10, 100, 20);
        
        const Rectangle<int> thumbnailBounds (10, 40, getWidth() - 20, 160);
        thumbnailComp.setBounds (thumbnailBounds);
        positionOverlay.setBounds (thumbnailBounds);
        
        const Rectangle<int> spectrogramBounds (10, 40+160+20, getWidth() - 20, getHeight() - 230);
        freqSpectrumBackground.setBounds (spectrogramBounds);
        freqSpectrumComp1.setBounds (spectrogramBounds);
        freqSpectrumComp2.setBounds (spectrogramBounds);
        freqSpectrumComp3.setBounds (spectrogramBounds);
        freqSpectrumComp4.setBounds (spectrogramBounds);
        
        // position multiple thumbnail views
        // const int thumbnailHeight = (getHeight() - 50)/4;
        // thumbnailComp.setBounds (10, 40, getWidth() - 20, thumbnailHeight);
        // thumbnailComp2.setBounds (10, 40 + thumbnailHeight * 1, getWidth() - 20, thumbnailHeight);
        // thumbnailComp3.setBounds (10, 40 + thumbnailHeight * 2, getWidth() - 20, thumbnailHeight);
        // thumbnailComp4.setBounds (10, 40 + thumbnailHeight * 3, getWidth() - 20, thumbnailHeight);
        // positionOverlay.setBounds (10, 40, getWidth() - 20, thumbnailHeight*4);
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &transportSource)
            transportSourceChanged();
    }
    
    void buttonClicked (Button* button) override
    {
        if (button == &openButton)      openButtonClicked();
        if (button == &playButton)      playButtonClicked();
        if (button == &stopButton)      stopButtonClicked();
        if (button == &loopingToggle)   loopButtonChanged();
    }
    
    void timerCallback() override
    {
        if (transportSource.isPlaying())
        {
            const RelativeTime position (transportSource.getCurrentPosition());
            
            const int minutes = ((int) position.inMinutes()) % 60;
            const int seconds = ((int) position.inSeconds()) % 60;
            const int millis  = ((int) position.inMilliseconds()) % 1000;
            
            const String positionString (String::formatted ("%02d:%02d:%03d", minutes, seconds, millis));
            
            currentPositionLabel.setText (positionString, dontSendNotification);
        }
        else
        {
            currentPositionLabel.setText ("--:--:---", dontSendNotification);
        }
    }
    
    void updateLoopState (bool shouldLoop)
    {
        if (readerSource != nullptr)
            readerSource->setLooping (shouldLoop);
    }
    
    
private:
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Stopping
    };
    
    void changeState (TransportState newState)
    {
        if (state != newState)
        {
            state = newState;
            
            switch (state)
            {
                case Stopped:
                    stopButton.setEnabled (false);
                    playButton.setEnabled (true);
                    transportSource.setPosition (0.0);
                    break;
                    
                case Starting:
                    playButton.setEnabled (false);
                    transportSource.start();
                    break;
                    
                case Playing:
                    stopButton.setEnabled (true);
                    break;
                    
                case Stopping:
                    transportSource.stop();
                    break;

                default:
                    jassertfalse;
                    break;
            }
        }
    }
    
    void transportSourceChanged()
    {
        if (transportSource.isPlaying())
            changeState (Playing);
        else
            changeState (Stopped);
    }
    
    void openButtonClicked()
    {
        
        for (int i=0; i<1; i++) {
        
            FileChooser chooser ("Select a Wave file to play...",
                                 File::nonexistent,
                                 "*.wav");
        
            if (chooser.browseForFileToOpen())
            {
                File file (chooser.getResult());
            
                if (AudioFormatReader* reader = formatManager.createReaderFor (file))
                {
                    ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
                    transportSource.setSource (newSource, 0, nullptr, reader->sampleRate, 4);
                    playButton.setEnabled (true);
                    thumbnailComp.setFile (file);
                    readerSource = newSource.release();
                }
            }
        }
    }
    
    void playButtonClicked()
    {
        updateLoopState (loopingToggle.getToggleState());
        changeState (Starting);
    }
    
    void stopButtonClicked()
    {
        changeState (Stopping);
    }
    
    void loopButtonChanged()
    {
        updateLoopState (loopingToggle.getToggleState());
    }
    
    enum
    {
        fftOrder = 10,
        fftSize = 1 << fftOrder
    };
    
    //==========================================================================
    TextButton openButton;
    TextButton playButton;
    TextButton stopButton;
    ToggleButton loopingToggle;
    Label currentPositionLabel;
    
    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    TransportState state;
    AudioThumbnailCache thumbnailCache;
    SimpleThumbnailComponent thumbnailComp;
    SimplePositionOverlay positionOverlay;

    SimpleFreqSpectrum freqSpectrumComp1;
    SimpleFreqSpectrum freqSpectrumComp2;
    SimpleFreqSpectrum freqSpectrumComp3;
    SimpleFreqSpectrum freqSpectrumComp4;
    SimpleFreqSpectrumBackground freqSpectrumBackground;
    
    AudioSampleBuffer tempBuffer;
    
    RingFifo ringBuffer1;
    RingFifo ringBuffer2;
    RingFifo ringBuffer3;
    RingFifo ringBuffer4;
    float fftCalcBuffer1 [2 * fftSize];
    float fftCalcBuffer2 [2 * fftSize];
    float fftCalcBuffer3 [2 * fftSize];
    float fftCalcBuffer4 [2 * fftSize];
    float fftCookedData1 [fftSize];
    float fftCookedData2 [fftSize];
    float fftCookedData3 [fftSize];
    float fftCookedData4 [fftSize];
    FFT forwardFFT;
    bool nextFFTBlockReady;

    LookAndFeel_V3 lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
