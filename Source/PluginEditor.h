#pragma once
#include "PluginProcessor.h"

constexpr int UI_W = 1280, UI_H = 800;

struct IndustrialLAF : public juce::LookAndFeel_V4
{
    IndustrialLAF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawTreeviewPlusMinusBox (juce::Graphics&, const juce::Rectangle<float>&, juce::Colour, bool isOpen, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    static juce::Colour orange() { return juce::Colour (0xffe8590c); }
    static juce::Colour ink()    { return juce::Colour (0xff17181a); }
    static juce::Colour plate()  { return juce::Colour (0xff232427); }
};

class WaveView : public juce::Component
{
public:
    explicit WaveView (TraperetorProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TraperetorProcessor& proc;
    std::shared_ptr<Sample> cached;
    bool cachedRev = false;
    std::vector<float> peaks;
};

class TraperetorUI : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::Timer
{
public:
    explicit TraperetorUI (TraperetorProcessor&);
    ~TraperetorUI() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragging = false; repaint(); }

private:
    struct Knob { juce::Slider s; juce::Label l; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; };
    struct Panel { juce::String title; std::vector<Knob*> knobs; std::vector<juce::Component*> extras; int row; juce::Rectangle<int> r; };

    void timerCallback() override;
    void refreshSlots();
    void refreshBanks();
    void refreshList();
    void rebuildTree();
    void pickEntry (const SoundEntry&);
    void addToBank (const juce::Array<juce::File>&);
    void createBank (const juce::String&);
    void askNewBank();
    void askDeleteBank();
    void askRemoveSound();
    void say (const juce::String& s) { status.setText (s, juce::dontSendNotification); }
    int addPanel (const juce::String& t, int row);
    void addKnob (int panel, const char* id, const char* label);
    void addToggle (int panel, const char* id, const char* label);
    void addCombo (int panel, const char* id);

    TraperetorProcessor& proc;
    IndustrialLAF laf;
    WaveView wave;
    juce::OwnedArray<juce::TextButton> slotBtns;
    juce::ToggleButton bpmSel { "BPM SELECT" }, hostBtn { "HOST" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bpmA, hostA;
    juce::Label origTag { {}, "ORIG BPM" }, tgtTag { {}, "TARGET BPM" }, origBpm, tgtBpm, tempoHint;

    juce::ComboBox bankBox;
    juce::TextButton newBankBtn { "NEW BANK" }, delBankBtn { "DELETE BANK" }, addBtn { "ADD SOUNDS" },
                     addFolderBtn { "ADD FOLDER" }, remBtn { "REMOVE SOUND" };
    juce::TextEditor search;
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> rootItem;
    bool copying = false;
    juce::Label status;
    std::vector<SoundEntry> cur;

    juce::MidiKeyboardComponent keys;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<Panel> panels;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<juce::ToggleButton> toggles;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    int lastVersion = -1, lastSel = -1, pending = 0;
    double lastKey = -1.0;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TraperetorUI)
};

// Thin, resizable wrapper: the whole UI is drawn at 1280x800 and scaled to fit the window/screen.
class TraperetorEditor : public juce::AudioProcessorEditor
{
public:
    explicit TraperetorEditor (TraperetorProcessor&);
    void resized() override;
private:
    TraperetorUI ui;
    juce::ComponentBoundsConstrainer constrainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TraperetorEditor)
};
