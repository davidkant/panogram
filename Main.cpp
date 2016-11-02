/*
  ==============================================================================

 This is a very basic proof of concept test app for multichannel localization.
 
 Starting with 4-channel localization, done stupid simple. But we can easily 
 drop more sophisticated code into the visualization component now that we have
 the basics of audio and visualization up and running. 
 
 My recommendation if you want to try a new technique is make a new panogram
 component and drop it in in place of the current "QuadPanogramComp".
 
 Don't trust anything you see here; this is not the correct way to code anything!
 
 - dk, 2016
 
 todo:
 -> dynamic memory management
 -> variable fft size (will break lots of initializations)
 -> UI display paramaeters for gain scaling and display range
 -> UI select which audio channels output (currently output is 2-channel)
 -> newFFTData flag so don't redraw old data
 -> JUCE cross-platform equivalents for accelerate?
 -> numBins so not fft/2 all the time
 -> quad audio output
 
  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"

Component* createMainContentComponent();

//==============================================================================
class MultiLocalizerApplication  : public JUCEApplication
{
public:
    //==============================================================================
    MultiLocalizerApplication() {}

    const String getApplicationName()       { return ProjectInfo::projectName; }
    const String getApplicationVersion()    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed()       { return true; }

    //==============================================================================
    void initialise (const String& commandLine)
    {
        // initialisation code

        mainWindow = new MainWindow();
    }

    void shutdown()
    {
        // shutdown code

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit()
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const String& commandLine)
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainContentComponent class.
    */
    class MainWindow    : public DocumentWindow
    {
    public:
        MainWindow()  : DocumentWindow ("FERP Localizer",
                                        Colours::black,
                                        DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (createMainContentComponent(), true);
            setResizable (true, true);
            
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed()
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    ScopedPointer<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (MultiLocalizerApplication)
