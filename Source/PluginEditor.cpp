#include "PluginEditor.h"
namespace { const juce::Colour bg {0xff101417}, panel {0xff1b2429}, teal {0xff42d6c5}; }

VoxRestoreAudioProcessorEditor::VoxRestoreAudioProcessorEditor(VoxRestoreAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(920, 610);
    presetBox.addItem("Gentle Repair", 1);
    presetBox.addItem("Metallic Clean-up", 2);
    presetBox.addItem("Smooth Sibilance", 3);
    presetBox.setSelectedId(1);
    presetBox.onChange = [this] { setPreset(presetBox.getSelectedId()); };
    addAndMakeVisible(presetBox);
    autoButton.onClick = [this] {
        processor.startAutoAnalysis();
        status.setText("Listening to input for 10 seconds…", juce::dontSendNotification);
        startTimerHz(20);
    };
    addAndMakeVisible(autoButton);
    powerButton.setClickingTogglesState(true);
    powerButton.setToggleState(true, juce::dontSendNotification);
    powerButton.onClick = [this] {
        if (auto* v = processor.parameters.getParameter("power"))
            v->setValueNotifyingHost(powerButton.getToggleState() ? 1.0f : 0.0f);
    };
    addAndMakeVisible(powerButton);
    status.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(status);

    for (int i = 0; i < 16; ++i)
    {
        auto* s = sliders.add(new juce::Slider());
        s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 20);
        s->setColour(juce::Slider::rotarySliderFillColourId, teal);
        s->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        s->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
        auto* l = labels.add(new juce::Label());
        l->setText(names[i], juce::dontSendNotification);
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, juce::Colour(0xffc4d0d4));
        l->setFont(juce::Font(10.5f, juce::Font::bold));
        addAndMakeVisible(l);
        attachments.push_back(std::make_unique<Attachment>(processor.parameters, ids[i], *s));
    }
}
void VoxRestoreAudioProcessorEditor::setPreset(int id)
{
    const float presets[3][16] = {
        {0,45,12,8,22,18,22,52,48,58,14,14,16,18,0,100},
        {0,55,16,10,48,34,22,72,58,55,10,12,34,20,0,100},
        {0,45,10,6,18,22,54,48,52,78,14,12,18,18,0,100}
    };
    int row = juce::jlimit(0, 2, id - 1);
    for (int i = 0; i < 16; ++i)
        if (auto* p = processor.parameters.getParameter(ids[i]))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(presets[row][i]));
    status.setText(presetBox.getText() + " loaded", juce::dontSendNotification);
}
void VoxRestoreAudioProcessorEditor::timerCallback()
{
    float progress = processor.getAutoProgress();
    status.setText("Analysing vocal… " + juce::String((int)(progress * 100.0f)) + "%",
                   juce::dontSendNotification);
    if (processor.isAutoAnalysisComplete()) {
        processor.applyAutoAnalysis();
        status.setText("Analysis complete • suggested settings applied", juce::dontSendNotification);
        stopTimer();
    }
}
void VoxRestoreAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(25.0f, juce::Font::bold));
    g.drawText("VOX RE:STORE", 24, 16, 360, 34, juce::Justification::centredLeft);
    g.setColour(teal);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("VOCAL RESTORATION  /  REPAIR  /  ENHANCEMENT", 26, 49, 400, 18, juce::Justification::centredLeft);
    const juce::Rectangle<int> a[] = {{20,100,215,470},{247,100,430,470},{685,100,215,225},{685,340,215,230}};
    const juce::String t[] = {"01  FOUNDATION","02  ARTIFACT REPAIR","03  VOCAL BODY","04  LEVEL & OUTPUT"};
    for (int i=0;i<4;++i) {
        g.setColour(panel); g.fillRoundedRectangle(a[i].toFloat(),12.0f);
        g.setColour(teal); g.setFont(juce::Font(12.0f,juce::Font::bold));
        g.drawText(t[i],a[i].reduced(12).removeFromTop(26),juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour(0xff75858a)); g.setFont(10.0f);
    g.drawText("AUTO suggests settings from level / spectral activity; it cannot restore missing words.",
               26,580,800,18,juce::Justification::centredLeft);
}
void VoxRestoreAudioProcessorEditor::resized()
{
    presetBox.setBounds(490,25,170,28); autoButton.setBounds(674,22,165,34);
    powerButton.setBounds(844,22,62,34); status.setBounds(26,75,560,20);
    for (int i=0;i<4;++i) {
        int x=30+(i%2)*103, y=165+(i/2)*190;
        sliders[i]->setBounds(x,y,88,105); labels[i]->setBounds(x-3,y-24,94,20);
    }
    for (int i=4;i<7;++i) {
        int x=260+(i-4)*137; sliders[i]->setBounds(x,158,112,116);
        labels[i]->setBounds(x-2,135,116,20);
    }
    for (int i=7;i<10;++i) {
        int x=260+(i-7)*137; sliders[i]->setBounds(x+12,330,88,96);
        labels[i]->setBounds(x,309,112,20);
    }
    for (int i=10;i<13;++i) {
        int x=694+((i-10)%2)*100, y=145+((i-10)/2)*105;
        sliders[i]->setBounds(x,y,88,82); labels[i]->setBounds(x-4,y-19,96,18);
    }
    for (int i=13;i<16;++i) {
        int x=694+((i-13)%2)*100, y=390+((i-13)/2)*105;
        sliders[i]->setBounds(x,y,88,82); labels[i]->setBounds(x-4,y-19,96,18);
    }
}
