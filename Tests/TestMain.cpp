// CureumTests: unit tests and developer tools.
//
//   CureumTests --unit                         run all unit tests
//   CureumTests --import <file>                import a Serum/CUREUM preset and print a summary
//   CureumTests --render <preset|factory:N> <out.wav>   render a short phrase to a WAV file
//   CureumTests --screenshot <out.png> [tab]   render the editor to a PNG (needs a display or Xvfb)
//
// Set CUREUM_FIXTURES to a folder with real Serum presets to enable the fixture tests.

#include "TestHelpers.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);

    if (args.isEmpty() || args[0] == "--unit")
    {
        juce::UnitTestRunner runner;
        runner.setAssertOnFailure (false);
        runner.runTestsInCategory ("CUREUM");
        int failures = 0, passes = 0;
        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            failures += runner.getResult (i)->failures;
            passes += runner.getResult (i)->passes;
        }
        std::cout << "\n==== " << passes << " checks passed, " << failures << " failed ====" << std::endl;
        return failures == 0 ? 0 : 1;
    }
    if (args[0] == "--import" && args.size() >= 2)
        return cureumtest::runImport (juce::File (args[1]));
    if (args[0] == "--render" && args.size() >= 3)
        return cureumtest::runRender (args[1], juce::File (args[2]));
    if (args[0] == "--screenshot" && args.size() >= 2)
        return cureumtest::runScreenshot (juce::File (args[1]), args.size() >= 3 ? args[2] : juce::String());

    std::cout << "Unknown command" << std::endl;
    return 2;
}
