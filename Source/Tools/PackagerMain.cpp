#include "../Synth/SamplePackager.h"
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include <iostream>

// =============================================================================
// TKBPackager — Standalone CLI Sample Container Packaging Utility
//
// Usage:
//   TKBPackager <input_wav_directory> <output_bin_file>
// =============================================================================
int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "=====================================================" << std::endl;
    std::cout << "  TKBPackager — TutorKeyBox02 Sample Container Tool  " << std::endl;
    std::cout << "=====================================================" << std::endl;

    if (argc < 3) {
        std::cout << "Usage: TKBPackager <input_wav_directory> <output_bin_file>" << std::endl;
        std::cout << "Example: TKBPackager \"./Samples/Neumann M49\" \"./NeumannM49.bin\"" << std::endl;
        return 1;
    }

    juce::File inputDir(argv[1]);
    juce::File outputFile(argv[2]);

    if (!inputDir.exists()) {
        std::cout << "Error: Input sample directory does not exist: " << inputDir.getFullPathName().toStdString() << std::endl;
        return 1;
    }

    std::cout << "Input Directory: " << inputDir.getFullPathName().toStdString() << std::endl;
    std::cout << "Output Container: " << outputFile.getFullPathName().toStdString() << std::endl;

    bool success = SamplePackager::createPackage(inputDir, outputFile);

    if (success) {
        std::cout << "SUCCESS: Binary sample container created successfully -> "
                  << outputFile.getFileName().toStdString() << std::endl;
        return 0;
    } else {
        std::cout << "ERROR: Failed to package sample container!" << std::endl;
        return 1;
    }
}
