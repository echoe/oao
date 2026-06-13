// PluginEditor.cpp
#include "Constants.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

FMPluginAudioProcessorEditor::FMPluginAudioProcessorEditor (FMPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), 
      audioProcessor (p),
      presetBar (p), //initialize preset bar	
      opsPage (p.apvts, oaoColors),     // Pass APVTS context straight down
      matrixPage (p.apvts, "MOD_", "Modulation Matrix", oaoColors),
      audioMatrixPage (p.apvts, "AUDIO_ROUTE_", "Audio Matrix", oaoColors),
      effectsPage (p.apvts, oaoColors),
      settingsPage (oaoColors, oaoLookAndFeel) //until here. 
{
    // Load saved colors, apply to look&feel, then set look&feel
    oaoColors.loadFromFile();
    oaoLookAndFeel.applyColors();
    setLookAndFeel (&oaoLookAndFeel);
    effectsPage.lookAndFeelChanged();
    settingsPage.refreshAll(); //update settingsPage boxes

// Settings page callbacks
    settingsPage.onColorsChanged = [this]
    {
        oaoLookAndFeel.applyColors();
#ifndef OAO_FX_ONLY
        opsPage.lookAndFeelChanged();
        matrixPage.lookAndFeelChanged();
        audioMatrixPage.lookAndFeelChanged();
#endif
        effectsPage.lookAndFeelChanged();
        this->lookAndFeelChanged();
        repaint();
    };

#ifndef OAO_FX_ONLY
    // Wire the sample-load callback from the operators page to the processor
    opsPage.onLoadSample = [&p] (int opIndex, juce::File file)
    {
        p.loadSampleForOperator (opIndex, file);
    };

    // After preset load, update the Load button text on each operator to the restored sample name
    p.onSamplesRestored = [this, &p]
    {
        for (int i = 0; i < ProjectConfig::numOperators; ++i)
            opsPage.setSampleButtonText (i, p.loadedSampleNames[i]);
    };

    addAndMakeVisible (opsPage);
    addAndMakeVisible (matrixPage);
    addAndMakeVisible (audioMatrixPage);
#endif
    
    addAndMakeVisible (presetBar);
    addAndMakeVisible (effectsPage);
    addAndMakeVisible (settingsPage);

    settingsPage.onScaleChanged = [this] (float scale)
    {
        // Update scale, then run resized.
        oaoLookAndFeel.currentScale = scale;
        oaoColors.scale = scale;
        int baseWidth  = ProjectConfig::pluginSizeX;
        int baseHeight = ProjectConfig::pluginSizeY;
        setSize (static_cast<int> (baseWidth * scale),
                 static_cast<int> (baseHeight * scale));
        // Then refresh everything else ...
        sendLookAndFeelChange();
        presetBar.refreshScale();
#ifndef OAO_FX_ONLY
        opsPage.repaintAll();
        matrixPage.refreshColors();
        audioMatrixPage.refreshColors();
#endif
        oaoColors.saveToFile();
        repaint();
    };

    // Navigation Buttons Configuration
#ifndef OAO_FX_ONLY
    addAndMakeVisible (opsPageButton);
    opsPageButton.setButtonText ("Operators");
    opsPageButton.onClick = [this] { setPage (PageView::Operators); };

    addAndMakeVisible (matrixPageButton);
    matrixPageButton.setButtonText ("Mod Matrix");
    matrixPageButton.onClick = [this] { setPage (PageView::Matrix); };

    addAndMakeVisible (audioMatrixPageButton);
    audioMatrixPageButton.setButtonText ("Audio Matrix");
    audioMatrixPageButton.onClick = [this] { setPage (PageView::AudioMatrix); };
#endif

    addAndMakeVisible (effectsPageButton);
    effectsPageButton.setButtonText ("Effects");
    effectsPageButton.onClick = [this] { setPage (PageView::Effects); };

    addAndMakeVisible (settingsPageButton);
    settingsPageButton.setButtonText ("Settings");
    settingsPageButton.onClick = [this] { setPage (PageView::Settings); };

    // Gain slider/label in case you worry about deafening yourself.
    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible (gainSlider);
    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (gainLabel);
    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "GAIN_CEIL", gainSlider);

    //oscilloscope
    oscilloscope = std::make_unique<Oscilloscope> (audioProcessor, oaoColors);
    addAndMakeVisible (*oscilloscope);

    // set initial defaults
#ifdef OAO_FX_ONLY
    setPage (PageView::Effects);
#else
    setPage (PageView::Operators);
#endif
    setSize (ProjectConfig::pluginSizeX, ProjectConfig::pluginSizeY);
}

FMPluginAudioProcessorEditor::~FMPluginAudioProcessorEditor()
{
setLookAndFeel (nullptr); // To make sure plugin doesn't crash DAW when closed
}

void FMPluginAudioProcessorEditor::setPage (PageView pageToDisplay)
{
    currentPage = pageToDisplay;
    
#ifndef OAO_FX_ONLY
    // Toggle visibility based on the active page enum state
    opsPage.setVisible (currentPage == PageView::Operators);
    matrixPage.setVisible (currentPage == PageView::Matrix);
    audioMatrixPage.setVisible (currentPage == PageView::AudioMatrix);
    
    // Ensure the top button highlight states visually match the current selection
    opsPageButton.setToggleState (currentPage == PageView::Operators, juce::dontSendNotification);
    matrixPageButton.setToggleState (currentPage == PageView::Matrix, juce::dontSendNotification);
    audioMatrixPageButton.setToggleState (currentPage == PageView::AudioMatrix, juce::dontSendNotification);
#endif

    effectsPage.setVisible (currentPage == PageView::Effects);
    settingsPage.setVisible (currentPage == PageView::Settings);
    
    effectsPageButton.setToggleState (currentPage == PageView::Effects, juce::dontSendNotification);
    settingsPageButton.setToggleState (currentPage == PageView::Settings, juce::dontSendNotification);
}

void FMPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
     g.fillAll (oaoColors.background);
}

void FMPluginAudioProcessorEditor::resized()
{
    auto area  = getLocalBounds();
    float scale = oaoLookAndFeel.currentScale;

    // Scale fixed heights with current scale
    int topBarHeight = juce::roundToInt(getHeight() * 0.05);
    int navBarHeight = juce::roundToInt(getHeight() * 0.05);

    // Top bar
    auto topBarArea = area.removeFromTop (topBarHeight);

    // Preset bar takes the left chunk — scale its width too
    auto presetArea = topBarArea.removeFromLeft (static_cast<int> (getWidth() * 0.70f));
    presetBar.setBounds (presetArea.reduced (2));

    // Gain slider on the right
    auto gainArea = topBarArea.removeFromRight (static_cast<int> (getWidth() * 0.22f));
    gainLabel.setBounds (gainArea.removeFromLeft (static_cast<int> (getWidth() * 0.04f))); //this is taken from the gainArea
    gainSlider.setBounds (gainArea.reduced (2));
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false,
        static_cast<int> (45 * scale), static_cast<int> (topBarHeight * 0.6f));

    // Oscilloscope, also on right
    if (oscilloscope != nullptr)
        oscilloscope->setBounds (topBarArea.removeFromRight (static_cast<int> (getWidth() * 0.08f)));

// Nav buttons
    auto navArea   = area.removeFromTop (navBarHeight);
    
#ifdef OAO_FX_ONLY
    int buttonWidth = getWidth() / 2;
#else
    int buttonWidth = getWidth() / 5;
    
    opsPageButton.setBounds          (navArea.removeFromLeft (buttonWidth).reduced (4));
    matrixPageButton.setBounds       (navArea.removeFromLeft (buttonWidth).reduced (4));
    audioMatrixPageButton.setBounds  (navArea.removeFromLeft (buttonWidth).reduced (4));
    
    opsPage.setBounds (area);
    matrixPage.setBounds (area);
    audioMatrixPage.setBounds (area);
#endif

    effectsPageButton.setBounds      (navArea.removeFromLeft (buttonWidth).reduced (4));
    settingsPageButton.setBounds     (navArea.reduced (4));
    
    effectsPage.setBounds (area);
    settingsPage.setBounds (area);
}

void FMPluginAudioProcessorEditor::lookAndFeelChanged()
{
    juce::AudioProcessorEditor::lookAndFeelChanged();

    gainSlider.setColour (juce::Slider::textBoxBackgroundColourId, oaoColors.surface);
    gainSlider.setColour (juce::Slider::textBoxTextColourId, oaoColors.text);
    gainSlider.sendLookAndFeelChange();

}
