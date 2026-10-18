#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <set>
using namespace juce;

// ---------------------------------------------------------------- helpers
static float blep (double t, double dt)
{
    if (t < dt)     { t /= dt;           return (float) (t + t - t * t - 1.0); }
    if (t > 1 - dt) { t = (t - 1) / dt;  return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

static float oscWave (int w, double ph, double dt)
{
    switch (w)
    {
        case 0:  return (float) std::sin (ph * MathConstants<double>::twoPi);
        case 1:  return (float) (ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);
        case 2:  return (float) (2 * ph - 1) - blep (ph, dt);
        default: { float s = ph < 0.5 ? 1.0f : -1.0f;
                   s += blep (ph, dt);
                   s -= blep (std::fmod (ph + 0.5, 1.0), dt);
                   return s; }
    }
}

static float lfoWave (int shape, double ph)
{
    switch (shape)
    {
        case 0:  return (float) std::sin (ph * MathConstants<double>::twoPi);
        case 1:  return (float) (ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);
        case 2:  return (float) (2 * ph - 1);
        default: return ph < 0.5 ? 1.0f : -1.0f;
    }
}

// ---------------------------------------------------------------- time stretch (WSOLA, offline)
static AudioBuffer<float> wsola (const AudioBuffer<float>& in, double ratio)   // ratio = outLen / inLen
{
    const int N = in.getNumSamples(), C = in.getNumChannels();
    const int outLen = jmax (1, (int) std::lround (N * ratio));
    if (N < 4096 || std::abs (ratio - 1.0) < 1e-4) return in;

    AudioBuffer<float> out (C, outLen); out.clear();
    const int win = 2048, hop = 512, tol = 256, corr = 512;
    std::vector<float> w ((size_t) win), norm ((size_t) outLen + (size_t) win, 0.0f), mono ((size_t) N, 0.0f);
    for (int i = 0; i < win; ++i) w[(size_t) i] = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * i / win);
    for (int c = 0; c < C; ++c) for (int i = 0; i < N; ++i) mono[(size_t) i] += in.getSample (c, i) / C;

    int prev = 0;
    for (int k = 0;; ++k)
    {
        const int outStart = k * hop - win / 2;
        if (outStart >= outLen) break;
        const int nominal = (int) std::lround (outStart / ratio);
        int pos = nominal;
        const int target = prev + hop;
        if (k > 0 && target >= 0 && target + corr < N)
        {
            float bestC = -1e30f; int best = -1;
            float et = 1e-9f; for (int i = 0; i < corr; i += 2) et += mono[(size_t) (target + i)] * mono[(size_t) (target + i)];
            for (int d = -tol; d <= tol; d += 2)
            {
                const int cand = nominal + d;
                if (cand < 0 || cand + corr >= N) continue;
                float s = 0, e = 1e-9f;
                for (int i = 0; i < corr; i += 2) { const float x = mono[(size_t) (cand + i)]; s += mono[(size_t) (target + i)] * x; e += x * x; }
                s /= std::sqrt (e * et);
                if (s > bestC) { bestC = s; best = cand; }
            }
            if (best >= 0) pos = best;
        }
        prev = pos;
        for (int i = 0; i < win; ++i)
        {
            const int o = outStart + i, src = pos + i;
            if (o < 0 || src < 0) continue;
            if (o >= outLen || src >= N) break;
            for (int c = 0; c < C; ++c) out.getWritePointer (c)[o] += in.getReadPointer (c)[src] * w[(size_t) i];
            norm[(size_t) o] += w[(size_t) i];
        }
    }
    for (int c = 0; c < C; ++c)
    {
        float* d = out.getWritePointer (c);
        for (int i = 0; i < outLen; ++i) if (norm[(size_t) i] > 1e-3f) d[i] /= norm[(size_t) i];
    }
    return out;
}

// ---------------------------------------------------------------- banks
BankManager::BankManager()
{
    rootDir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("TRAPERETOR").getChildFile ("Banks");
    rootDir.createDirectory();
}

StringArray BankManager::names() const
{
    StringArray n;
    for (const auto& d : rootDir.findChildFiles (File::findDirectories, false)) n.add (d.getFileName());
    n.sortNatural();
    return n;
}

bool BankManager::create (const String& name)
{
    const auto n = File::createLegalFileName (name.trim());
    if (n.isEmpty()) return false;
    return bankDir (n).createDirectory().wasOk();
}

bool BankManager::remove (const String& name) { return bankDir (name).deleteRecursively(); }

String BankManager::relPath (const String& bank, const File& f) const
{
    return f.getRelativePathFrom (bankDir (bank)).replaceCharacter ('\\', '/');
}

std::vector<SoundEntry> BankManager::sounds (const String& bank) const
{
    std::vector<SoundEntry> v;
    const auto dir = bankDir (bank);
    if (! dir.isDirectory()) return v;
    std::map<String, double> bpms;
    const auto js = JSON::parse (dir.getChildFile ("bank.json"));
    if (auto* a = js["sounds"].getArray())
        for (auto& x : *a) bpms[x["file"].toString()] = (double) x["bpm"];
    auto files = dir.findChildFiles (File::findFiles, true, "*");
    files.sort();
    for (const auto& f : files)
        if (isAudioFile (f))
        {
            const String rel = relPath (bank, f);
            const String folder = rel.contains ("/") ? rel.upToLastOccurrenceOf ("/", false, false) : String();
            v.push_back ({ f.getFileNameWithoutExtension(), rel, folder, bpms.count (rel) ? bpms[rel] : 0.0 });
        }
    return v;
}

void BankManager::write (const String& bank, const std::vector<SoundEntry>& v) const
{
    Array<var> arr;
    for (const auto& e : v)
    {
        auto* o = new DynamicObject();
        o->setProperty ("name", e.name); o->setProperty ("file", e.file); o->setProperty ("bpm", e.bpm);
        arr.add (var (o));
    }
    auto* r = new DynamicObject();
    r->setProperty ("sounds", arr);
    bankDir (bank).getChildFile ("bank.json").replaceWithText (JSON::toString (var (r)));
}

Array<File> BankManager::addFiles (const String& bank, const Array<File>& files)
{
    Array<File> added;
    const auto dir = bankDir (bank);
    dir.createDirectory();
    for (const auto& f : files)
    {
        if (! isAudioFile (f)) continue;
        if (f.getParentDirectory() == dir) { added.add (f); continue; }
        auto dest = dir.getNonexistentChildFile (f.getFileNameWithoutExtension(), f.getFileExtension(), false);
        if (f.copyFileTo (dest)) added.add (dest);
    }
    write (bank, sounds (bank));
    return added;
}

int BankManager::addFolder (const String& bank, const File& source, int* foldersOut)
{
    const auto dir = bankDir (bank);
    dir.createDirectory();
    int n = 0;
    std::set<String> folders;
    for (const auto& f : source.findChildFiles (File::findFiles, true, "*"))
    {
        if (! isAudioFile (f)) continue;
        const String rel = f.getRelativePathFrom (source).replaceCharacter ('\\', '/');
        auto dest = dir.getChildFile (rel);
        if (dest == f) { ++n; continue; }
        dest.getParentDirectory().createDirectory();
        if (dest.existsAsFile())
            dest = dest.getParentDirectory().getNonexistentChildFile (dest.getFileNameWithoutExtension(), dest.getFileExtension(), false);
        if (f.copyFileTo (dest)) { ++n; if (rel.contains ("/")) folders.insert (rel.upToLastOccurrenceOf ("/", false, false)); }
    }
    write (bank, sounds (bank));
    if (foldersOut) *foldersOut = (int) folders.size();
    return n;
}

void BankManager::removeSound (const String& bank, const String& file)
{
    bankDir (bank).getChildFile (file).deleteFile();
    write (bank, sounds (bank));
}

void BankManager::setBpm (const String& bank, const String& file, double bpm)
{
    auto v = sounds (bank);
    for (auto& e : v) if (e.file == file) e.bpm = bpm;
    write (bank, v);
}

// ---------------------------------------------------------------- voice
void Voice::start (int n, float v, std::shared_ptr<Sample> smp, const Params& p, double sr, uint64_t a)
{
    active = true; gate = true; note = n; vel = v; age = a; s = smp; sampleDone = false; buf = nullptr;
    if (s)
    {
        buf = p.rev ? &s->rev : &s->fwd;
        const int total = buf->getNumSamples();
        startPos = p.start * total;
        endPos = startPos + (total - startPos) * p.len;
        pos = startPos;
        rate = std::pow (2.0, (n - 60 + p.pitch + p.fine / 100.0) / 12.0) * s->sr / sr;
        if (endPos - startPos < 2) sampleDone = true;
    }
    freq = 440.0 * std::pow (2.0, (n - 69 + p.pitch + p.fine / 100.0) / 12.0);
    uniN = jlimit (1, 5, p.uni);
    for (int u = 0; u < uniN; ++u)
    {
        const double cents = uniN > 1 ? ((double) u / (uniN - 1) - 0.5) * 2.0 * p.detune : 0.0;
        ratio[u] = std::pow (2.0, cents / 1200.0);
        ph[u] = 0.173 * u;
    }
    subPh = 0; lfoPh = 0;

    ADSR::Parameters ap { p.atk, p.dec, p.sus, p.rel };
    amp.setSampleRate (sr); amp.setParameters (ap); amp.noteOn();
    ADSR::Parameters fp { p.fatk, p.fdec, p.fsus, p.rel };
    fe.setSampleRate (sr); fe.setParameters (fp); fe.noteOn();

    flt.reset();
    flt.setType (p.ftype == 0 ? dsp::StateVariableTPTFilterType::lowpass
               : p.ftype == 1 ? dsp::StateVariableTPTFilterType::highpass
                              : dsp::StateVariableTPTFilterType::bandpass);
    flt.setResonance (p.res);
}

void Voice::render (float* L, float* R, int n, const Params& p, double sr, Random& rnd)
{
    const float d = p.ldepth;
    const float norm = p.osc * 0.5f / std::sqrt ((float) uniN);
    const double maxCut = jmin (20000.0, sr * 0.45);

    for (int i = 0; i < n; ++i)
    {
        const float a = amp.getNextSample();
        if (! amp.isActive()) { active = false; s.reset(); buf = nullptr; return; }
        const float fev = fe.getNextSample();

        float lfo = 0.0f;
        if (d > 0.0f)
        {
            lfo = lfoWave (p.lshape, lfoPh);
            lfoPh += p.lrate / sr;
            if (lfoPh >= 1.0) lfoPh -= 1.0;
        }

        float gain = a * vel;
        if (p.ltgt == 0 && d > 0.0f) gain *= 1.0f - d * 0.5f + lfo * d * 0.5f;
        const double pm = (p.ltgt == 2 && d > 0.0f) ? std::pow (2.0, lfo * d * 150.0 / 1200.0) : 1.0;

        if ((i & 15) == 0)
        {
            double c = p.cut * std::pow (2.0, p.fenv * 5.0 * fev + ((p.ltgt == 1 && d > 0.0f) ? lfo * d * 2.5 : 0.0));
            flt.setCutoffFrequency ((float) jlimit (30.0, maxCut, c));
        }

        float l = 0, r = 0;

        // sample layer
        if (p.smp > 0.0f && buf != nullptr && ! sampleDone)
        {
            if (pos >= endPos)
            {
                if (p.loop && endPos - startPos > 1) pos = startPos + std::fmod (pos - startPos, endPos - startPos);
                else sampleDone = true;
            }
            if (! sampleDone)
            {
                const int len = buf->getNumSamples();
                const int i0 = jmin ((int) pos, len - 1);
                int i1 = i0 + 1;
                if (i1 >= len) i1 = p.loop ? (int) startPos : i0;
                const float fr = (float) (pos - i0);
                const float* c0 = buf->getReadPointer (0);
                const float* c1 = buf->getReadPointer (jmin (1, buf->getNumChannels() - 1));
                l += (c0[i0] + (c0[i1] - c0[i0]) * fr) * p.smp;
                r += (c1[i0] + (c1[i1] - c1[i0]) * fr) * p.smp;
                pos += rate * pm;
            }
        }

        // oscillator layer
        if (p.osc > 0.0f)
        {
            for (int u = 0; u < uniN; ++u)
            {
                const double inc = freq * ratio[u] * pm / sr;
                ph[u] += inc; if (ph[u] >= 1.0) ph[u] -= 1.0;
                const float sm = oscWave (p.wave, ph[u], inc) * norm;
                const float pan = uniN > 1 ? (float) u / (uniN - 1) : 0.5f;
                l += sm * std::cos (pan * MathConstants<float>::halfPi) * 1.414f;
                r += sm * std::sin (pan * MathConstants<float>::halfPi) * 1.414f;
            }
        }
        if (p.sub > 0.0f)
        {
            subPh += freq * 0.5 * pm / sr; if (subPh >= 1.0) subPh -= 1.0;
            const float sm = (float) std::sin (subPh * MathConstants<double>::twoPi) * p.sub * 0.5f;
            l += sm; r += sm;
        }
        if (p.noise > 0.0f)
        {
            const float nz = (rnd.nextFloat() * 2.0f - 1.0f) * p.noise * 0.4f;
            l += nz; r += nz;
        }

        l = flt.processSample (0, l);
        r = flt.processSample (1, r);
        L[i] += l * gain;
        R[i] += r * gain;
    }
}

// ---------------------------------------------------------------- processor
AudioProcessorValueTreeState::ParameterLayout TraperetorProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout lay;
    auto F = [&] (const char* id, const char* name, float mn, float mx, float df, bool exp = false)
    {
        NormalisableRange<float> r (mn, mx);
        if (exp) r.setSkewForCentre (std::sqrt (mn * mx));
        lay.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, df));
    };
    auto I = [&] (const char* id, const char* name, int mn, int mx, int df)
    { lay.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, mn, mx, df)); };
    auto C = [&] (const char* id, const char* name, StringArray ch, int df)
    { lay.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, ch, df)); };
    auto B = [&] (const char* id, const char* name)
    { lay.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, false)); };

    F ("smp", "Sample Level", 0, 1, 0.9f);  F ("start", "Sample Start", 0, 1, 0);
    F ("len", "Sample Length", 0.02f, 1, 1, true);  I ("pitch", "Pitch", -24, 24, 0);
    F ("fine", "Fine", -100, 100, 0);  B ("loop", "Loop");  B ("rev", "Reverse");
    F ("osc", "Osc Level", 0, 1, 0);  I ("uni", "Unison", 1, 5, 1);  F ("detune", "Detune", 0, 60, 10);
    F ("sub", "Sub", 0, 1, 0);  F ("noise", "Noise", 0, 1, 0);
    C ("wave", "Wave", { "SIN", "TRI", "SAW", "SQR" }, 2);
    F ("cut", "Cutoff", 40, 18000, 18000, true);  F ("res", "Resonance", 0.1f, 20, 0.7f, true);
    C ("ftype", "Filter Type", { "LOWPASS", "HIGHPASS", "BANDPASS" }, 0);
    F ("atk", "Attack", 0.002f, 4, 0.002f, true);  F ("dec", "Decay", 0.01f, 4, 0.3f, true);
    F ("sus", "Sustain", 0, 1, 1);  F ("rel", "Release", 0.01f, 6, 0.15f, true);
    F ("fenv", "Filter Env Amount", -1, 1, 0);  F ("fatk", "Filter Attack", 0.002f, 4, 0.005f, true);
    F ("fdec", "Filter Decay", 0.01f, 4, 0.3f, true);  F ("fsus", "Filter Sustain", 0, 1, 0.3f);
    F ("lrate", "LFO Rate", 0.1f, 30, 4, true);  F ("ldepth", "LFO Depth", 0, 1, 0);
    C ("lshape", "LFO Shape", { "SIN", "TRI", "SAW", "SQR" }, 0);
    C ("ltgt", "LFO Target", { "AMP", "FILTER", "PITCH" }, 0);
    F ("drive", "Drive", 0, 1, 0);  F ("dtime", "Delay Time", 0.02f, 1, 0.3f, true);
    F ("dfb", "Delay Feedback", 0, 0.9f, 0.35f);  F ("dmix", "Delay Mix", 0, 1, 0);
    F ("rsize", "Reverb Size", 0.2f, 5, 1.8f);  F ("rmix", "Reverb Mix", 0, 1, 0.1f);
    F ("tone", "Tone", 500, 20000, 20000, true);  F ("vol", "Master", 0, 1, 0.7f);
    B ("bpmsel", "BPM Select");  B ("hostbpm", "Host BPM");  F ("tbpm", "Target BPM", 40, 300, 120);
    return lay;
}

TraperetorProcessor::TraperetorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRAPERETOR", createLayout())
{
    formats.registerBasicFormats();
    makeDemo();
}

void TraperetorProcessor::makeDemo()
{
    auto s = std::make_shared<Sample>();
    const double r = 44100.0; const int n = (int) (r * 1.2);
    s->fwd.setSize (1, n); s->sr = r; s->name = "808 DEMO";
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / r;
        ph += MathConstants<double>::twoPi * (45.0 + 110.0 * std::exp (-t * 18.0)) / r;
        s->fwd.setSample (0, i, (float) (std::tanh (std::sin (ph) * 2.2) * std::exp (-t * 3.2) * 0.9));
    }
    s->orig = s->fwd;
    s->rev.makeCopyOf (s->fwd); s->rev.reverse (0, n);
    slots[0] = s;
}

void TraperetorProcessor::prepareToPlay (double rate, int block)
{
    sr = rate;
    dsp::ProcessSpec spec { rate, (uint32) block, 2 };
    for (auto& v : voices) { v.flt.prepare (spec); v.active = false; }
    toneF.prepare (spec); toneF.setType (dsp::StateVariableTPTFilterType::lowpass);
    for (auto& b : dBuf) b.assign ((size_t) (rate * 2.0) + 4, 0.0f);
    dW = 0; reverb.setSampleRate (rate); reverb.reset();
}

bool TraperetorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

Params TraperetorProcessor::snapshot()
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Params p;
    p.smp = g ("smp"); p.start = g ("start"); p.len = g ("len"); p.pitch = g ("pitch"); p.fine = g ("fine");
    p.osc = g ("osc"); p.detune = g ("detune"); p.sub = g ("sub"); p.noise = g ("noise");
    p.cut = g ("cut"); p.res = g ("res"); p.atk = g ("atk"); p.dec = g ("dec"); p.sus = g ("sus"); p.rel = g ("rel");
    p.fenv = g ("fenv"); p.fatk = g ("fatk"); p.fdec = g ("fdec"); p.fsus = g ("fsus");
    p.lrate = g ("lrate"); p.ldepth = g ("ldepth"); p.drive = g ("drive"); p.dtime = g ("dtime");
    p.dfb = g ("dfb"); p.dmix = g ("dmix"); p.rsize = g ("rsize"); p.rmix = g ("rmix");
    p.tone = g ("tone"); p.vol = g ("vol");
    p.uni = (int) std::lround (g ("uni")); p.wave = (int) (g ("wave") + 0.5f); p.ftype = (int) (g ("ftype") + 0.5f);
    p.lshape = (int) (g ("lshape") + 0.5f); p.ltgt = (int) (g ("ltgt") + 0.5f);
    p.loop = g ("loop") > 0.5f; p.rev = g ("rev") > 0.5f;
    return p;
}

std::shared_ptr<Sample> TraperetorProcessor::getSlot (int i)
{
    const SpinLock::ScopedTryLockType lk (slotLock);
    return lk.isLocked() ? slots[(size_t) jlimit (0, 7, i)] : nullptr;
}

void TraperetorProcessor::startVoice (int note, float vel, const Params& p)
{
    Voice* v = nullptr;
    for (auto& x : voices) if (! x.active) { v = &x; break; }
    if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.age < v->age) v = &x; }
    v->start (note, vel, getSlot (selected.load()), p, sr, ++ageCounter);
}

void TraperetorProcessor::renderVoices (float* L, float* R, int n, const Params& p)
{
    for (auto& v : voices) if (v.active) v.render (L, R, n, p, sr, rnd);
}

void TraperetorProcessor::applyFx (float* L, float* R, int n, const Params& p)
{
    if (p.drive > 0.0f)
    {
        const float k = p.drive * 60.0f;
        for (int i = 0; i < n; ++i)
        {
            L[i] = (1 + k) * L[i] / (1 + k * std::abs (L[i]));
            R[i] = (1 + k) * R[i] / (1 + k * std::abs (R[i]));
        }
    }
    toneF.setCutoffFrequency ((float) jmin ((double) p.tone, sr * 0.45));
    const int size = (int) dBuf[0].size();
    const int ds = jlimit (1, size - 1, (int) (p.dtime * sr));
    float* ch[2] = { L, R };
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            const float x = toneF.processSample (c, ch[c][i]);
            const float dl = dBuf[c][(size_t) ((dW - ds + size) % size)];
            dBuf[c][(size_t) dW] = x + dl * p.dfb;
            ch[c][i] = x + dl * p.dmix;
        }
        if (++dW >= size) dW = 0;
    }
    if (p.rmix > 0.0f)
    {
        Reverb::Parameters rp;
        rp.roomSize = jmap (p.rsize, 0.2f, 5.0f, 0.2f, 0.97f);
        rp.damping = 0.5f; rp.width = 1.0f; rp.wetLevel = p.rmix * 0.4f; rp.dryLevel = 0.5f; rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }
    for (int i = 0; i < n; ++i)
    {
        L[i] = jlimit (-1.0f, 1.0f, L[i] * p.vol);
        R[i] = jlimit (-1.0f, 1.0f, R[i] * p.vol);
    }
}

void TraperetorProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();
    const int n = buffer.getNumSamples();
    if (auto* ph = getPlayHead())
        if (auto pp = ph->getPosition())
            if (auto b = pp->getBpm()) hostBpm = *b;
    keyState.processNextMidiBuffer (midi, 0, n, true);
    const Params p = snapshot();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int t = jlimit (0, n, meta.samplePosition);
        if (t > pos) { renderVoices (L + pos, R + pos, t - pos, p); pos = t; }
        if (m.isNoteOn())
            startVoice (m.getNoteNumber(), m.getFloatVelocity(), p);
        else if (m.isNoteOff())
        { for (auto& v : voices) if (v.active && v.gate && v.note == m.getNoteNumber()) v.release(); }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        { for (auto& v : voices) if (v.active) v.release(); }
    }
    if (pos < n) renderVoices (L + pos, R + pos, n - pos, p);
    applyFx (L, R, n, p);
}

double TraperetorProcessor::targetBpm()
{
    const bool host = apvts.getRawParameterValue ("hostbpm")->load() > 0.5f;
    const double hb = hostBpm.load();
    return (host && hb > 0.0) ? hb : (double) apvts.getRawParameterValue ("tbpm")->load();
}

bool TraperetorProcessor::bpmSelectOn() { return apvts.getRawParameterValue ("bpmsel")->load() > 0.5f; }

void TraperetorProcessor::retempo (int slot)
{
    std::shared_ptr<Sample> s;
    { const SpinLock::ScopedLockType lk (slotLock); s = slots[(size_t) slot]; }
    if (! s) return;
    const double want = (bpmSelectOn() && s->bpm > 0.0) ? targetBpm() : 0.0;
    if (std::abs (want - s->appliedTarget) < 1e-3 && std::abs (s->bpm - s->appliedOrig) < 1e-6) return;

    auto n = std::make_shared<Sample> (*s);
    n->fwd = want > 0.0 ? wsola (s->orig, s->bpm / want) : s->orig;
    n->rev.makeCopyOf (n->fwd); n->rev.reverse (0, n->fwd.getNumSamples());
    n->appliedTarget = want; n->appliedOrig = s->bpm;
    { const SpinLock::ScopedLockType lk (slotLock); slots[(size_t) slot] = n; }
    ++slotsVersion;
}

void TraperetorProcessor::retempoAll() { for (int i = 0; i < 8; ++i) retempo (i); }

void TraperetorProcessor::setSlotBpm (int slot, double bpm)
{
    std::shared_ptr<Sample> s;
    { const SpinLock::ScopedLockType lk (slotLock); s = slots[(size_t) jlimit (0, 7, slot)]; }
    if (! s) return;
    auto n = std::make_shared<Sample> (*s);
    n->bpm = bpm;
    { const SpinLock::ScopedLockType lk (slotLock); slots[(size_t) slot] = n; }
    if (n->bank.isNotEmpty()) banks.setBpm (n->bank, banks.relPath (n->bank, File (n->path)), bpm);
    retempo (slot);
    ++slotsVersion;
}

bool TraperetorProcessor::loadFile (const File& f, int slot, double bpmHint)
{
    std::unique_ptr<AudioFormatReader> rd (formats.createReaderFor (f));
    if (rd == nullptr || rd->lengthInSamples <= 0 || rd->lengthInSamples > (int64) (rd->sampleRate * 300.0)) return false;
    auto s = std::make_shared<Sample>();
    const int len = (int) rd->lengthInSamples;
    s->orig.setSize (jmin (2, (int) rd->numChannels), len);
    rd->read (&s->orig, 0, len, 0, true, true);
    s->fwd = s->orig;
    s->rev.makeCopyOf (s->fwd); s->rev.reverse (0, len);
    s->sr = rd->sampleRate; s->name = f.getFileNameWithoutExtension(); s->path = f.getFullPathName();
    for (auto a = f.getParentDirectory(); a != a.getParentDirectory(); a = a.getParentDirectory())
        if (a.getParentDirectory() == banks.root())
        {
            s->bank = a.getFileName();
            const String rel = banks.relPath (s->bank, f);
            for (const auto& e : banks.sounds (s->bank)) if (e.file == rel) s->bpm = e.bpm;
            break;
        }
    if (bpmHint > 0.0) s->bpm = bpmHint;
    { const SpinLock::ScopedLockType lk (slotLock); slots[(size_t) jlimit (0, 7, slot)] = s; }
    ++slotsVersion;
    retempo (slot);
    return true;
}

int TraperetorProcessor::loadFiles (const Array<File>& files, int first)
{
    int count = 0, slot = first;
    for (const auto& f : files)
    {
        if (count >= 8) break;
        if (loadFile (f, slot)) { slot = (slot + 1) % 8; ++count; }
    }
    return count;
}

void TraperetorProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    for (int i = 0; i < 8; ++i)
    {
        std::shared_ptr<Sample> s;
        { const SpinLock::ScopedLockType lk (slotLock); s = slots[(size_t) i]; }
        st.setProperty ("slot" + String (i), s ? s->path : String(), nullptr);
        st.setProperty ("slotbpm" + String (i), s ? s->bpm : 0.0, nullptr);
    }
    st.setProperty ("selected", selected.load(), nullptr);
    st.setProperty ("bank", currentBank, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void TraperetorProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = ValueTree::fromXml (*xml);
    apvts.replaceState (vt);
    currentBank = vt.getProperty ("bank").toString();
    for (int i = 0; i < 8; ++i)
    {
        const String path = vt.getProperty ("slot" + String (i)).toString();
        if (path.isNotEmpty() && File (path).existsAsFile())
        {
            const double b = (double) vt.getProperty ("slotbpm" + String (i), 0.0);
            loadFile (File (path), i, b);
        }
    }
    selected = jlimit (0, 7, (int) vt.getProperty ("selected", 0));
    ++slotsVersion;
}

AudioProcessorEditor* TraperetorProcessor::createEditor() { return new TraperetorEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TraperetorProcessor(); }
