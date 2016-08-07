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
    positionOverlay (transportSource)
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
        
        setSize (500, 230);
        
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

        // copy channels 3 and 4 to output buffer for now...
        bufferToFill.buffer->copyFrom (0, bufferToFill.startSample, tempBuffer, 2, 0, bufferToFill.numSamples);
        bufferToFill.buffer->copyFrom (1, bufferToFill.startSample, tempBuffer, 3, 0, bufferToFill.numSamples);
        
        // fill output buffer
        // transportSource.getNextAudioBlock (bufferToFill);
        
        // debug me
        // String message;
        // message << "slalalalalalal = " << bufferToFill.buffer->getNumChannels() << newLine;
        // Logger::getCurrentLogger()->writeToLog (message);
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
        
        const Rectangle<int> thumbnailBounds (10, 40, getWidth() - 20, getHeight() - 50);
        thumbnailComp.setBounds (thumbnailBounds);
        positionOverlay.setBounds (thumbnailBounds);
        
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
    AudioSampleBuffer tempBuffer;
    
    LookAndFeel_V3 lookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED
