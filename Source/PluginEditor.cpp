#include "PluginEditor.h"
using namespace juce;

// ---------------------------------------------------------------- look
IndustrialLAF::IndustrialLAF()
{
    setColour (Slider::textBoxTextColourId, Colour (0xffffb27d));
    setColour (Slider::textBoxBackgroundColourId, ink());
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, Colour (0xffc4c6c9));
    setColour (ComboBox::textColourId, ink());
    setColour (ComboBox::outlineColourId, ink());
    setColour (ComboBox::arrowColourId, ink());
    setColour (PopupMenu::backgroundColourId, Colour (0xffc4c6c9));
    setColour (PopupMenu::textColourId, ink());
    setColour (PopupMenu::highlightedBackgroundColourId, plate());
    setColour (PopupMenu::highlightedTextColourId, Colour (0xffffb27d));
    setColour (Label::textColourId, ink());
}

void IndustrialLAF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) / 2.0f;
    const auto c = b.getCentre();
    Path t; t.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
    g.setColour (Colour (0x55000000)); g.strokePath (t, PathStrokeType (4.0f));
    Path v; v.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a0 + pos * (a1 - a0), true);
    g.setColour (orange()); g.strokePath (v, PathStrokeType (4.0f));
    const float dr = r * 0.72f;
    g.setColour (Colour (0xff2a2b2d)); g.fillEllipse (c.x - dr, c.y - dr, dr * 2, dr * 2);
    g.setColour (ink()); g.drawEllipse (c.x - dr, c.y - dr, dr * 2, dr * 2, 2.0f);
    g.setColour (Colour (0xff414245));
    for (int i = 0; i < 24; ++i)
    {
        const float a = i * MathConstants<float>::twoPi / 24.0f;
        g.drawLine (c.x + std::sin (a) * dr * 0.8f, c.y - std::cos (a) * dr * 0.8f,
                    c.x + std::sin (a) * dr * 0.97f, c.y - std::cos (a) * dr * 0.97f, 1.5f);
    }
    const float ang = a0 + pos * (a1 - a0);
    g.setColour (orange());
    g.drawLine (c.x + std::sin (ang) * dr * 0.3f, c.y - std::cos (ang) * dr * 0.3f,
                c.x + std::sin (ang) * dr * 0.92f, c.y - std::cos (ang) * dr * 0.92f, 3.5f);
}

void IndustrialLAF::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? plate() : Colour (0xffc4c6c9)); g.fillRect (r);
    g.setColour (ink()); g.drawRect (r, 2.0f);
    g.setColour (on ? orange() : Colour (0xff555555)); g.fillEllipse (r.getX() + 7, r.getCentreY() - 4, 8, 8);
    g.setColour (on ? Colour (0xffffb27d) : ink());
    g.setFont (Font (FontOptions (13.0f, Font::bold)));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (20).toNearestInt(), Justification::centredLeft);
}

void IndustrialLAF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState() || down;
    g.setColour (on ? plate() : Colour (0xffc4c6c9)); g.fillRect (r);
    g.setColour (on ? orange() : ink()); g.drawRect (r, 2.0f);
}

Font IndustrialLAF::getTextButtonFont (TextButton&, int) { return Font (FontOptions (14.0f, Font::bold)); }

// ---------------------------------------------------------------- waveform
void WaveView::paint (Graphics& g)
{
    auto r = getLocalBounds();
    g.fillAll (Colour (0xff1b1c1e));
    g.setColour (IndustrialLAF::ink()); g.drawRect (r, 2);
    auto s = proc.getSlot (proc.selected.load());
    const bool rv = proc.apvts.getRawParameterValue ("rev")->load() > 0.5f;
    if (s == nullptr)
    {
        g.setColour (Colours::grey); g.setFont (Font (FontOptions (16.0f)));
        g.drawText ("Empty slot: drop an audio file here", r, Justification::centred);
        return;
    }
    const int w = r.getWidth() - 4, h = r.getHeight() - 4;
    if (s != cached || rv != cachedRev || (int) peaks.size() != w)
    {
        cached = s; cachedRev = rv; peaks.assign ((size_t) w, 0.0f);
        const auto& b = rv ? s->rev : s->fwd;
        const int len = b.getNumSamples(), step = jmax (1, len / jmax (1, w));
        const float* d = b.getReadPointer (0);
        for (int x = 0; x < w; ++x)
        {
            float m = 0;
            for (int i = x * step; i < jmin (len, (x + 1) * step); i += 4) m = jmax (m, std::abs (d[i]));
            peaks[(size_t) x] = m;
        }
    }
    g.setColour (IndustrialLAF::orange());
    for (int x = 0; x < w; ++x)
    {
        const float m = peaks[(size_t) x];
        g.fillRect (2 + x, 2 + (int) (h * 0.5f - m * h * 0.5f), 1, jmax (1, (int) (m * h)));
    }
    const float st = proc.apvts.getRawParameterValue ("start")->load();
    const float ln = proc.apvts.getRawParameterValue ("len")->load();
    const float a = st * w, b2 = a + (1 - st) * ln * w;
    g.setColour (Colour (0xa6000000));
    g.fillRect (2.0f, 2.0f, a, (float) h);
    g.fillRect (2.0f + b2, 2.0f, w - b2, (float) h);
}

// ---------------------------------------------------------------- UI
void TraperetorUI::SoundModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= (int) ui.shown.size()) return;
    const auto& e = ui.cur[(size_t) ui.shown[(size_t) row]];
    if (sel) { g.setColour (IndustrialLAF::orange()); g.fillRect (0, 0, w, h); }
    else if (row % 2) { g.setColour (Colour (0x14ffffff)); g.fillRect (0, 0, w, h); }
    g.setColour (sel ? Colours::white : Colour (0xffdcdde0));
    g.setFont (Font (FontOptions (14.0f, Font::bold)));
    g.drawText (e.name, 8, 0, w - 70, h, Justification::centredLeft, true);
    g.setColour (sel ? Colours::white : Colour (0xffffb27d));
    g.setFont (Font (FontOptions (12.0f)));
    g.drawText (e.bpm > 0 ? String (e.bpm, 1) : String ("--"), w - 62, 0, 56, h, Justification::centredRight);
}

TraperetorUI::TraperetorUI (TraperetorProcessor& p)
    : proc (p), wave (p), keys (p.keyState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);

    int pn = addPanel ("SAMPLE", 0);
    addKnob (pn, "smp", "LEVEL"); addKnob (pn, "start", "START"); addKnob (pn, "len", "LENGTH");
    addKnob (pn, "pitch", "PITCH"); addKnob (pn, "fine", "FINE");
    addToggle (pn, "loop", "LOOP"); addToggle (pn, "rev", "REVERSE");

    pn = addPanel ("OSCILLATOR", 0);
    addKnob (pn, "osc", "LEVEL"); addKnob (pn, "uni", "UNISON"); addKnob (pn, "detune", "DETUNE");
    addKnob (pn, "sub", "SUB"); addKnob (pn, "noise", "NOISE"); addCombo (pn, "wave");

    pn = addPanel ("FILTER", 1);
    addKnob (pn, "cut", "CUTOFF"); addKnob (pn, "res", "RESO"); addCombo (pn, "ftype");

    pn = addPanel ("AMP ENV", 1);
    addKnob (pn, "atk", "ATTACK"); addKnob (pn, "dec", "DECAY"); addKnob (pn, "sus", "SUSTAIN"); addKnob (pn, "rel", "RELEASE");

    pn = addPanel ("FILTER ENV", 1);
    addKnob (pn, "fenv", "AMOUNT"); addKnob (pn, "fatk", "ATTACK"); addKnob (pn, "fdec", "DECAY"); addKnob (pn, "fsus", "SUSTAIN");

    pn = addPanel ("LFO", 2);
    addKnob (pn, "lrate", "RATE"); addKnob (pn, "ldepth", "DEPTH"); addCombo (pn, "lshape"); addCombo (pn, "ltgt");

    pn = addPanel ("EFFECTS", 2);
    addKnob (pn, "drive", "DRIVE"); addKnob (pn, "dtime", "DLY TIME"); addKnob (pn, "dfb", "DLY FDBK");
    addKnob (pn, "dmix", "DLY MIX"); addKnob (pn, "rsize", "REV SIZE"); addKnob (pn, "rmix", "REV MIX");

    pn = addPanel ("OUTPUT", 2);
    addKnob (pn, "tone", "TONE"); addKnob (pn, "vol", "MASTER");

    // slots
    for (int i = 0; i < 8; ++i)
    {
        auto* b = slotBtns.add (new TextButton());
        b->setClickingTogglesState (true); b->setRadioGroupId (1);
        b->onClick = [this, i] { proc.selected = i; refreshSlots(); wave.repaint(); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (wave);

    // tempo row
    bpmA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bpmsel", bpmSel);
    hostA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "hostbpm", hostBtn);
    auto lcd = [] (Label& l)
    {
        l.setEditable (true);
        l.setJustificationType (Justification::centred);
        l.setFont (Font (FontOptions (16.0f, Font::bold)));
        l.setColour (Label::backgroundColourId, IndustrialLAF::ink());
        l.setColour (Label::textColourId, Colour (0xffffb27d));
        l.setColour (Label::outlineColourId, Colour (0xff55585c));
        l.setColour (Label::backgroundWhenEditingColourId, Colour (0xff3a3b3e));
        l.setColour (Label::textWhenEditingColourId, Colours::white);
    };
    lcd (origBpm); lcd (tgtBpm);
    origBpm.onTextChange = [this]
    {
        const double v = origBpm.getText().getDoubleValue();
        if (v >= 20.0 && v <= 400.0) { proc.setSlotBpm (proc.selected.load(), v); refreshList(); say ("Original BPM set to " + String (v, 1) + "."); }
        refreshSlots();
    };
    tgtBpm.onTextChange = [this]
    {
        const float v = (float) jlimit (40.0, 300.0, tgtBpm.getText().getDoubleValue());
        if (auto* pr = proc.apvts.getParameter ("tbpm"))
        { pr->beginChangeGesture(); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); pr->endChangeGesture(); }
    };
    for (auto* l : { &origTag, &tgtTag }) { l->setFont (Font (FontOptions (13.0f, Font::bold))); l->setJustificationType (Justification::centredRight); }
    tempoHint.setText ("BPM SELECT off = the sound plays untouched. On = it is stretched to the target BPM (needs the ORIG BPM of the sound).", dontSendNotification);
    tempoHint.setFont (Font (FontOptions (12.0f)));
    for (auto* c : std::initializer_list<Component*> { &bpmSel, &hostBtn, &origTag, &tgtTag, &origBpm, &tgtBpm, &tempoHint }) addAndMakeVisible (c);

    // library browser
    bankBox.setTextWhenNothingSelected ("No banks yet");
    bankBox.onChange = [this] { proc.currentBank = bankBox.getText(); refreshList(); };
    newBankBtn.onClick = [this] { askNewBank(); };
    delBankBtn.onClick = [this] { askDeleteBank(); };
    addBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose oneshots", File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems,
                              [this] (const FileChooser& fc) { addToBank (fc.getResults()); });
    };
    addFolderBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose a folder (becomes a bank)", File());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                              [this] (const FileChooser& fc) { addToBank (fc.getResults()); });
    };
    remBtn.onClick = [this] { askRemoveSound(); };
    search.setTextToShowWhenEmpty ("Search sounds...", Colours::grey);
    search.setColour (TextEditor::backgroundColourId, Colour (0xff1b1c1e));
    search.setColour (TextEditor::textColourId, Colour (0xffffb27d));
    search.setColour (TextEditor::outlineColourId, IndustrialLAF::ink());
    search.setColour (TextEditor::focusedOutlineColourId, IndustrialLAF::orange());
    search.onTextChange = [this] { refreshList(); };
    list.setRowHeight (24);
    list.setColour (ListBox::backgroundColourId, Colour (0xff1b1c1e));
    list.setColour (ListBox::outlineColourId, IndustrialLAF::ink());
    list.setOutlineThickness (2);
    status.setFont (Font (FontOptions (13.0f)));
    status.setJustificationType (Justification::topLeft);
    status.setMinimumHorizontalScale (1.0f);
    say ("Drop sounds or a folder anywhere on the plugin to add them to a bank. Click a sound to load it into the selected slot.");
    for (auto* c : std::initializer_list<Component*> { &bankBox, &newBankBtn, &delBankBtn, &addBtn, &addFolderBtn, &remBtn, &search, &list, &status })
        addAndMakeVisible (c);

    keys.setAvailableRange (36, 96); keys.setLowestVisibleKey (48); keys.setKeyWidth (24.0f);
    keys.setColour (MidiKeyboardComponent::keyDownOverlayColourId, IndustrialLAF::orange());
    keys.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colour (0x44e8590c));
    addAndMakeVisible (keys);

    setSize (UI_W, UI_H);
    refreshBanks(); refreshList(); refreshSlots();
    startTimerHz (10);
}

TraperetorUI::~TraperetorUI() { setLookAndFeel (nullptr); }

int TraperetorUI::addPanel (const String& t, int row) { panels.push_back ({ t, {}, {}, row, {} }); return (int) panels.size() - 1; }

void TraperetorUI::addKnob (int pi, const char* id, const char* label)
{
    auto* k = knobs.add (new Knob());
    k->s.setSliderStyle (Slider::RotaryVerticalDrag);
    k->s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k->s.setTextBoxStyle (Slider::TextBoxBelow, false, 62, 15);
    k->s.setMouseDragSensitivity (160);
    k->a = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->s);
    k->l.setText (label, dontSendNotification);
    k->l.setJustificationType (Justification::centred);
    k->l.setFont (Font (FontOptions (13.0f, Font::bold)));
    addAndMakeVisible (k->s); addAndMakeVisible (k->l);
    panels[(size_t) pi].knobs.push_back (k);
}

void TraperetorUI::addToggle (int pi, const char* id, const char* label)
{
    auto* t = toggles.add (new ToggleButton (label));
    bAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, id, *t));
    addAndMakeVisible (t); panels[(size_t) pi].extras.push_back (t);
}

void TraperetorUI::addCombo (int pi, const char* id)
{
    auto* c = combos.add (new ComboBox());
    if (auto* cp = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (cp->choices, 1);
    cAtt.add (new AudioProcessorValueTreeState::ComboBoxAttachment (proc.apvts, id, *c));
    addAndMakeVisible (c); panels[(size_t) pi].extras.push_back (c);
}

// ---- library logic
void TraperetorUI::refreshBanks()
{
    bankBox.clear (dontSendNotification);
    const auto n = proc.banks.names();
    for (int i = 0; i < n.size(); ++i) bankBox.addItem (n[i], i + 1);
    const int idx = n.indexOf (proc.currentBank);
    if (idx >= 0) bankBox.setSelectedItemIndex (idx, dontSendNotification);
    else if (n.size() > 0) { proc.currentBank = n[0]; bankBox.setSelectedItemIndex (0, dontSendNotification); }
    else proc.currentBank = {};
}

void TraperetorUI::refreshList()
{
    cur = proc.banks.sounds (proc.currentBank);
    shown.clear();
    const String q = search.getText().trim();
    for (int i = 0; i < (int) cur.size(); ++i)
        if (q.isEmpty() || cur[(size_t) i].name.containsIgnoreCase (q)) shown.push_back (i);
    list.updateContent(); list.repaint();
}

void TraperetorUI::pick (int row)
{
    if (row < 0 || row >= (int) shown.size()) return;
    const auto& e = cur[(size_t) shown[(size_t) row]];
    const auto f = proc.banks.bankDir (proc.currentBank).getChildFile (e.file);
    if (proc.loadFile (f, proc.selected.load(), e.bpm))
        say ("Loaded \"" + e.name + "\" into slot " + String (proc.selected.load() + 1) + (e.bpm > 0 ? "" : ". Set its ORIG BPM to use BPM SELECT."));
    else say ("Could not read " + e.file + ".");
    refreshSlots();
}

void TraperetorUI::askNewBank()
{
    auto* aw = new AlertWindow ("New bank", "Name your bank:", MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "", "");
    aw->addButton ("Create", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    SafePointer<TraperetorUI> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw] (int r)
    {
        if (r == 1 && safe != nullptr) safe->createBank (aw->getTextEditorContents ("name"));
    }), true);
}

void TraperetorUI::askDeleteBank()
{
    if (proc.currentBank.isEmpty()) return;
    const String name = proc.currentBank;
    SafePointer<TraperetorUI> safe (this);
    AlertWindow::showAsync (MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::WarningIcon, "Delete bank",
                            "Delete bank \"" + name + "\" and all the sounds inside it?", "Delete", "Cancel", this),
                            [safe, name] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        safe->proc.banks.remove (name);
        safe->proc.currentBank = String();
        safe->refreshBanks(); safe->refreshList(); safe->say ("Bank deleted.");
    });
}

void TraperetorUI::askRemoveSound()
{
    const int row = list.getSelectedRow();
    if (row < 0 || row >= (int) shown.size()) return;
    const SoundEntry e = cur[(size_t) shown[(size_t) row]];
    const String bank = proc.currentBank;
    SafePointer<TraperetorUI> safe (this);
    AlertWindow::showAsync (MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::WarningIcon, "Remove sound",
                            "Remove \"" + e.name + "\" from this bank?", "Remove", "Cancel", this),
                            [safe, bank, e] (int r)
    {
        if (r != 1 || safe == nullptr) return;
        safe->proc.banks.removeSound (bank, e.file);
        safe->refreshList(); safe->say ("Sound removed.");
    });
}

void TraperetorUI::createBank (const String& name)
{
    if (proc.banks.create (name)) { proc.currentBank = File::createLegalFileName (name.trim()); refreshBanks(); refreshList(); say ("Bank \"" + proc.currentBank + "\" created. Add sounds to it."); }
    else say ("Could not create that bank.");
}

static Array<File> audioIn (const File& dir)
{
    Array<File> r;
    for (const auto& f : dir.findChildFiles (File::findFiles, true, "*")) if (isAudioFile (f)) r.add (f);
    r.sort();
    return r;
}

void TraperetorUI::addToBank (const Array<File>& paths)
{
    Array<File> loose, copiedAll;
    int total = 0;
    String lastBank;
    for (const auto& f : paths)
    {
        if (f.isDirectory())
        {
            const auto files = audioIn (f);
            if (files.isEmpty()) continue;
            const String b = File::createLegalFileName (f.getFileName());
            proc.banks.create (b);
            copiedAll.addArray (proc.banks.addFiles (b, files));
            total += files.size(); lastBank = b;
        }
        else loose.add (f);
    }
    if (loose.size() > 0)
    {
        if (proc.currentBank.isEmpty() && lastBank.isEmpty()) { proc.banks.create ("My Bank"); lastBank = "My Bank"; }
        const String b = lastBank.isNotEmpty() && proc.currentBank.isEmpty() ? lastBank : proc.currentBank;
        const auto added = proc.banks.addFiles (b, loose);
        copiedAll.addArray (added); total += added.size(); lastBank = b;
    }
    if (total == 0) { say ("No audio files found (WAV, AIFF, MP3, FLAC, OGG)."); return; }
    proc.currentBank = lastBank;
    refreshBanks(); refreshList();
    Array<File> toLoad; for (int i = 0; i < jmin (8, copiedAll.size()); ++i) toLoad.add (copiedAll[i]);
    proc.loadFiles (toLoad, proc.selected.load());
    refreshSlots();
    say (String (total) + " sound" + (total > 1 ? "s" : "") + " added to bank \"" + lastBank + "\". The first ones were loaded into the slots.");
}

void TraperetorUI::filesDropped (const StringArray& files, int, int)
{
    dragging = false;
    Array<File> fs; for (auto& f : files) fs.add (File (f));
    addToBank (fs); repaint();
}

void TraperetorUI::refreshSlots()
{
    lastVersion = proc.slotsVersion.load(); lastSel = proc.selected.load();
    for (int i = 0; i < slotBtns.size(); ++i)
    {
        auto s = proc.getSlot (i);
        slotBtns[i]->setButtonText (String (i + 1) + "  " + (s ? s->name.substring (0, 12) : String ("empty")));
        slotBtns[i]->setToggleState (i == lastSel, dontSendNotification);
    }
    if (! origBpm.isBeingEdited())
    {
        auto s = proc.getSlot (lastSel);
        origBpm.setText (s && s->bpm > 0 ? String (s->bpm, 1) : String ("--"), dontSendNotification);
    }
}

void TraperetorUI::timerCallback()
{
    if (lastVersion != proc.slotsVersion.load() || lastSel != proc.selected.load()) refreshSlots();

    const double eff = proc.targetBpm();
    const double key = proc.bpmSelectOn() ? eff : 0.0;
    if (std::abs (key - lastKey) > 0.01)
    {
        if (++pending >= 2) { proc.retempoAll(); lastKey = key; pending = 0; refreshSlots(); }
    }
    else pending = 0;

    const bool host = hostBtn.getToggleState();
    tgtBpm.setEditable (! host);
    if (! tgtBpm.isBeingEdited()) tgtBpm.setText (String (eff, 1), dontSendNotification);
    wave.repaint();
}

void TraperetorUI::resized()
{
    const int bx = 20, by = 70, bw = 270, bh = UI_H - 70 - 16;
    // library
    bankBox.setBounds (bx + 14, by + 36, bw - 28, 28);
    const int cw = (bw - 28 - 6) / 2;
    newBankBtn.setBounds (bx + 14, by + 70, cw, 24);       delBankBtn.setBounds (bx + 20 + cw, by + 70, cw, 24);
    addBtn.setBounds (bx + 14, by + 98, cw, 24);           addFolderBtn.setBounds (bx + 20 + cw, by + 98, cw, 24);
    remBtn.setBounds (bx + 14, by + 126, bw - 28, 24);
    search.setBounds (bx + 14, by + 158, bw - 28, 24);
    list.setBounds (bx + 14, by + 188, bw - 28, bh - 188 - 108);
    status.setBounds (bx + 14, by + bh - 100, bw - 28, 92);

    // slots strip
    const int rx = 302, rw = UI_W - 20 - rx, sy = 70;
    const int slotW = (rw - 28 - 7 * 6) / 8;
    for (int i = 0; i < slotBtns.size(); ++i) slotBtns[i]->setBounds (rx + 14 + i * (slotW + 6), sy + 34, slotW, 26);
    wave.setBounds (rx + 14, sy + 64, rw - 28, 50);
    int x = rx + 14; const int ty = sy + 120;
    bpmSel.setBounds (x, ty, 118, 24); x += 124;
    hostBtn.setBounds (x, ty, 72, 24); x += 84;
    origTag.setBounds (x, ty, 80, 24); x += 84; origBpm.setBounds (x, ty, 62, 24); x += 74;
    tgtTag.setBounds (x, ty, 92, 24); x += 96; tgtBpm.setBounds (x, ty, 62, 24); x += 74;
    tempoHint.setBounds (x, ty - 4, rx + rw - 14 - x, 32);

    // knob rows
    const int gap = 12, rowH = 158, y0 = 230;
    for (int row = 0; row < 3; ++row)
    {
        std::vector<Panel*> ps; int sum = 0;
        for (auto& p : panels) if (p.row == row)
        {
            ps.push_back (&p);
            int need = 24 + (int) p.knobs.size() * 78, ex = 0;
            for (auto* c : p.extras) ex += (dynamic_cast<ComboBox*> (c) ? 92 : 78) + 6;
            p.r.setWidth (jmax (need, 24 + ex)); sum += p.r.getWidth();
        }
        const int avail = rw - gap * ((int) ps.size() - 1);
        const int add = (avail - sum) / (int) ps.size();
        int px = rx;
        for (auto* p : ps)
        {
            p->r = { px, y0 + row * (rowH + 10), p->r.getWidth() + add, rowH };
            px += p->r.getWidth() + gap;
            int kx = p->r.getX() + 12;
            for (auto* k : p->knobs)
            {
                k->s.setBounds (kx, p->r.getY() + 32, 74, 78);
                k->l.setBounds (kx, p->r.getY() + 110, 74, 16);
                kx += 78;
            }
            int ex = p->r.getX() + 12;
            for (auto* c : p->extras)
            {
                const int w = dynamic_cast<ComboBox*> (c) ? 92 : 78;
                c->setBounds (ex, p->r.getY() + 130, w, 22); ex += w + 6;
            }
        }
    }
    keys.setBounds (rx, UI_H - 70, rw, 54);
}

void TraperetorUI::paint (Graphics& g)
{
    const Colour steel (0xffaeb1b4), ink = IndustrialLAF::ink();
    g.fillAll (Colour (0xff7b7e82));
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (20, 16, UI_W - 40, 14);
        g.fillAll (IndustrialLAF::plate());
        g.setColour (IndustrialLAF::orange());
        for (int x = -20; x < UI_W; x += 28)
        {
            Path p; p.addQuadrilateral ((float) x, 30.0f, (float) x + 14, 30.0f, (float) x + 28, 16.0f, (float) x + 14, 16.0f);
            g.fillPath (p);
        }
    }
    g.setColour (ink);
    g.setFont (Font (FontOptions (40.0f, Font::bold)));
    g.drawText ("TRAPERETOR", 20, 30, 600, 40, Justification::centredLeft);

    auto drawPanel = [&] (Rectangle<int> r, const String& title)
    {
        g.setColour (steel); g.fillRect (r);
        g.setColour (Colour (0x10ffffff));
        for (int x = r.getX(); x < r.getRight(); x += 3) g.drawVerticalLine (x, (float) r.getY(), (float) r.getBottom());
        g.setColour (ink); g.drawRect (r, 2);
        g.setColour (Colour (0xff2b2c2e));
        for (int sx : { r.getX() + 9, r.getRight() - 9 }) for (int sy : { r.getY() + 9, r.getBottom() - 9 })
            g.fillEllipse ((float) sx - 3, (float) sy - 3, 6.0f, 6.0f);
        g.setFont (Font (FontOptions (15.0f, Font::bold)));
        const int tw = (int) GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 22;
        g.setColour (IndustrialLAF::plate()); g.fillRect (r.getX() + 22, r.getY() + 7, tw, 20);
        g.setColour (Colour (0xffdcdde0)); g.drawText (title, r.getX() + 22, r.getY() + 7, tw, 20, Justification::centred);
    };

    drawPanel ({ 20, 70, 270, UI_H - 70 - 16 }, "LIBRARY");
    drawPanel ({ 302, 70, UI_W - 20 - 302, 150 }, "ONESHOT SLOTS");
    for (auto& p : panels) drawPanel (p.r, p.title);

    if (dragging)
    {
        g.setColour (Colour (0xdde8590c)); g.fillAll();
        g.setColour (Colours::white); g.setFont (Font (FontOptions (56.0f, Font::bold)));
        g.drawText ("DROP SOUNDS OR A FOLDER", getLocalBounds(), Justification::centred);
    }
}

// ---------------------------------------------------------------- resizable wrapper
TraperetorEditor::TraperetorEditor (TraperetorProcessor& p) : AudioProcessorEditor (&p), ui (p)
{
    addAndMakeVisible (ui);
    constrainer.setFixedAspectRatio ((double) UI_W / UI_H);
    constrainer.setSizeLimits (560, 350, 2560, 1600);
    setConstrainer (&constrainer);
    setResizable (true, true);

    double s = 1.0;
    if (auto* d = Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto a = d->userArea;
        s = jmin (1.0, (a.getHeight() - 110.0) / UI_H, (a.getWidth() - 30.0) / UI_W);
    }
    s = jmax (0.45, s);
    setSize ((int) (UI_W * s), (int) (UI_H * s));
}

void TraperetorEditor::resized()
{
    ui.setTransform (AffineTransform::scale ((float) getWidth() / (float) UI_W));
    ui.setBounds (0, 0, UI_W, UI_H);
}
