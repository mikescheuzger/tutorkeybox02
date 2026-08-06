#include <juce_gui_extra/juce_gui_extra.h>
#include "../GUI/MainComponent.h"
#include <csignal>

// ==============================================================================
/**
 * Main desktop application wrapper managing the app lifecycle and window.
 */
class TutorKeyboxApplication : public juce::JUCEApplication {
public:
    TutorKeyboxApplication() {}

    const juce::String getApplicationName() override { return "TutorKeyBox 02"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {
        if (mainWindow != nullptr) {
            mainWindow->toFront(true);
        }
    }

    void initialise(const juce::String& /*commandLine*/) override {
#if JUCE_MAC || JUCE_LINUX
        signal(SIGPIPE, SIG_IGN);
#endif
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(
                  name,
                  juce::Colour(0xff09090b),
                  DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            setResizeLimits(1100, 750, 2560, 1440);
            setSize(1360, 880);
            centreWithSize(1360, 880);
#endif

            setVisible(true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(TutorKeyboxApplication)
