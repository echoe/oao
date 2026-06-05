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

    // Settings page
    settingsPage.onColorsChanged = [this]
    {
        oaoLookAndFeel.applyColors();
	opsPage.lookAndFeelChanged();
	matrixPage.lookAndFeelChanged();
	audioMatrixPage.lookAndFeelChanged();
	effectsPage.lookAndFeelChanged();
	this->lookAndFeelChanged();
        repaint();
    };

    addAndMakeVisible (presetBar);
    addAndMakeVisible (opsPage);
    addAndMakeVisible (matrixPage);
    addAndMakeVisible (audioMatrixPage);
    addAndMakeVisible (effectsPage);
    addAndMakeVisible (settingsPage);

    settingsPage.onScaleChanged = [this] (float scale) //ensures settings page can change scale
    {
        int baseWidth  = ProjectConfig::pluginSizeX;
        int baseHeight = ProjectConfig::pluginSizeY;
        setSize (static_cast<int> (baseWidth * scale), static_cast<int> (baseHeight * scale));
        repaint();
    };

    // Navigation Buttons Configuration
    addAndMakeVisible (opsPageButton);
    opsPageButton.setButtonText ("Operators");
    opsPageButton.onClick = [this] { setPage (PageView::Operators); };

    addAndMakeVisible (matrixPageButton);
    matrixPageButton.setButtonText ("Mod Matrix");
    matrixPageButton.onClick = [this] { setPage (PageView::Matrix); };

    addAndMakeVisible (audioMatrixPageButton);
    audioMatrixPageButton.setButtonText ("Audio Matrix");
    audioMatrixPageButton.onClick = [this] { setPage (PageView::AudioMatrix); };

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

    // synth title :)
    titleLabel.setText ("OAO", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions().withHeight (18.0f).withStyle ("Bold")));
    addAndMakeVisible (titleLabel);
    
    //oscilloscope
    oscilloscope = std::make_unique<Oscilloscope> (audioProcessor);
    addAndMakeVisible (*oscilloscope);

    // set initial defaults
    setPage (PageView::Operators);
    setSize (ProjectConfig::pluginSizeX, ProjectConfig::pluginSizeY);
}

FMPluginAudioProcessorEditor::~FMPluginAudioProcessorEditor()
{
setLookAndFeel (nullptr); // To make sure plugin doesn't crash DAW when closed
}

void FMPluginAudioProcessorEditor::setPage (PageView pageToDisplay)
{
    currentPage = pageToDisplay;
    // Toggle visibility based on the active page enum state
    opsPage.setVisible (currentPage == PageView::Operators);
    matrixPage.setVisible (currentPage == PageView::Matrix);
    audioMatrixPage.setVisible (currentPage == PageView::AudioMatrix);
    effectsPage.setVisible (currentPage == PageView::Effects);
    settingsPage.setVisible (currentPage == PageView::Settings);
    // Ensure the top button highlight states visually match the current selection
    opsPageButton.setToggleState (currentPage == PageView::Operators, juce::dontSendNotification);
    matrixPageButton.setToggleState (currentPage == PageView::Matrix, juce::dontSendNotification);
    audioMatrixPageButton.setToggleState (currentPage == PageView::AudioMatrix, juce::dontSendNotification);
    effectsPageButton.setToggleState (currentPage == PageView::Effects, juce::dontSendNotification);
    settingsPageButton.setToggleState (currentPage == PageView::Settings, juce::dontSendNotification);
}

void FMPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
     g.fillAll (oaoColors.background);
}

void FMPluginAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    // Dedicate the top 40px to Preset Management controls
    auto topBarArea = area.removeFromTop (40);
    // Set preset and oversampling controls area to take up the left chunk
    auto presetArea = topBarArea.removeFromLeft(550);
    presetBar.setBounds (presetArea.reduced (2));
    // Head to the absolute right side of the bar for the gain slider
    auto gainArea = topBarArea.removeFromRight (220);
    gainLabel.setBounds (gainArea.removeFromLeft (45));
    gainSlider.setBounds (gainArea.reduced (2));

    // oscilloscope
    if (oscilloscope != nullptr)
        oscilloscope->setBounds (topBarArea.removeFromLeft(100));

    // Add title in remaining area in top 
    titleLabel.setBounds (topBarArea.reduced (2));

    // Dedicate the subsequent 40px block underneath the PresetBar to UI Navigation Page switching
    auto navArea = area.removeFromTop (40);
    int buttonWidth = getWidth() / 5;
    opsPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    matrixPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    audioMatrixPageButton.setBounds (navArea.removeFromLeft (buttonWidth).reduced (4));
    effectsPageButton.setBounds      (navArea.removeFromLeft (buttonWidth).reduced (4));
    settingsPageButton.setBounds     (navArea.reduced (4));
    // The active page occupies the remaining container bounds
    opsPage.setBounds (area);
    matrixPage.setBounds (area);
    audioMatrixPage.setBounds (area);
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
