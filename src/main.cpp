
#include <iostream>
#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"
#include <random>
#include "al/app/al_App.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"

using namespace al;


class SineEnv : public al::SynthVoice
{
public:
    // Unit generators
    gam::Pan<> mPan;
    gam::Sine<> mOsc;
    gam::Sine<> mOsc1;
    gam::Sine<> mOsc2;
    gam::Sine<> mOsc3;
    gam::Sine<> mOsc4;
    gam::Sine<> mOsc5;
    gam::Sine<> mOsc6;
    gam::Sine<> mOsc7;
    gam::Sine<> mOsc8;
    gam::Sine<> mOsc9;
    gam::Sine<> mOsc10;
    gam::Sine<> mOsc11;
    gam::Sine<> mOsc12;
    gam::Env<3> mAmpEnv;
    void init() override
    {
        // Intialize envelope
        mAmpEnv.curve(0); // make segments lines
        mAmpEnv.levels(0, 1, 1, 0);
        mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
        createInternalTriggerParameter("releaseTime", 0.1, 0.1, 10.0);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
    }

    // The audio processing function
    void onProcess(AudioIOData &io) override
    {
        float f = getInternalParameterValue("frequency");
        mOsc.freq(f);
        mOsc1.freq(f * 2);
        mOsc2.freq(f * 3);
        mOsc3.freq(f * 4);
        mOsc4.freq(f * 5);
        mOsc5.freq(f * 6);

        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
        mPan.pos(getInternalParameterValue("pan"));
        while (io())
        {
            float s1 = (mOsc() + (mOsc1()/2) + (mOsc2()/3) + (mOsc3()/4) + (mOsc4()/5) + (mOsc5()/6)) * mAmpEnv() * getInternalParameterValue("amplitude");
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        if (mAmpEnv.done())
            free();
    }

    void onTriggerOn() override { mAmpEnv.reset(); }

    void onTriggerOff() override { mAmpEnv.release(); }
};

struct SynthParams{
    float attack;
    float volume;
    float tempo;
};

class RingBuffer {
public:
    RingBuffer(int capacity){
        sizeTotal = capacity;
        sizeCurr = 0;
        rb = new double[capacity];
        firstIndex = 0;
        lastIndex = 1;
    }
    int size(){
        return sizeCurr;
    }
    bool isEmpty(){
        return (sizeCurr == 0);
    }
    bool isFull(){
        return (sizeCurr == sizeTotal);
    }
    void enqueue(double x){
        sizeCurr++;
        int index = lastIndex%sizeTotal;
        index = (index == 0)? index = sizeTotal-1 : index - 1;
        rb[index] = x;
        lastIndex++;
    }

    double dequeue(){
        sizeCurr--;
        int prevFirstIndex = firstIndex;
        firstIndex++;
        return rb[prevFirstIndex%sizeTotal];
    }

    double peek(){
        return rb[firstIndex%sizeTotal];
    }
private:
    int firstIndex;            // rb[first]  = first item in the buffer
    int lastIndex;             // rb[last-1] = last  item in the buffer
    int sizeTotal;             // current number of items in the buffer
    int sizeCurr;             // current number of items in the buffer
    double * rb;          // items in the buffer
};

class GuitarString {
public:
    GuitarString(double freq){
        itemSize = (int)(44100/freq);
        std::cout << "item size " << itemSize << std::endl;
        ring = new RingBuffer(itemSize);
        for(int i = 0; i < itemSize; i++){
            ring->enqueue(0);
        }
    }
    void pluck(){
        for(int i = 0; i < itemSize; i++){
            ring->dequeue();
        }
        for(int i = 0; i < itemSize; i++){
            double randVal = ((double) rand() / (RAND_MAX)) - 0.5;
            ring->enqueue(randVal);
        }
    }
    void tic(){
        double first = ring->dequeue();
        double second = ring->peek();
        ring->enqueue(((first + second)/2.0) * .996);
    }
    double sample(){
        return ring->peek();
    }
private:
    RingBuffer * ring;
    int itemSize;
};

class GuitarVoice : public SynthVoice
{
public:
    // Unit generators
    int sampleRate;

    gam::Pan<> mPan;
    gam::Sine<> mOsc;
    gam::Env<3> mAmpEnv;
    std::vector<GuitarString> notesPlayed;
    void init() override
    {
        mAmpEnv.curve(0); // make segments lines
        mAmpEnv.levels(0, 1, 1, 0);
        mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

        createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
        createInternalTriggerParameter("frequency", 60, 20, 5000);
        createInternalTriggerParameter("attackTime", 0.1, 0.01, 3.0);
        createInternalTriggerParameter("releaseTime", 0.1, 0.1, 10.0);
        createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
    }

    // The audio processing function
    void onProcess(AudioIOData &io) override
    {
        float f = getInternalParameterValue("frequency");
        mOsc.freq(f);

        mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
        mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
        mPan.pos(getInternalParameterValue("pan"));
        while (io())
        {
            float sumStrings = 0;
            for(auto & i : notesPlayed){
                sumStrings += i.sample();
            }
            float s1 = sumStrings * mAmpEnv() * getInternalParameterValue("amplitude") * 3;
            for(auto & i : notesPlayed){
                i.tic();
            }
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        if (mAmpEnv.done())
            free();
    }

    void onTriggerOn() override {
        mAmpEnv.reset();
        notesPlayed.emplace_back(GuitarString(getInternalParameterValue("frequency")));
        std::cout << "new note created with f=" << getInternalParameterValue("frequency") << std::endl;
        notesPlayed.at(notesPlayed.size()-1).pluck();
    }

    void onTriggerOff() override {
        std::cout << " note released" << std::endl;
        mAmpEnv.release();
    }
    void update(double dt) override{
    }
};

struct MyApp : public App {
    SynthGUIManager<GuitarVoice> sine;
    SynthParams synthParams1;
    SynthParams synthParams2;
    SynthParams synthParams3;
    SynthSequencer seq1;
    SynthSequencer seq2;
    SynthSequencer seq3;
    std::vector<SynthParams> params;
    std::vector<SynthVoice *> voices1;
    std::vector<SynthVoice *> voices2;
    std::vector<SynthVoice *> voices3;

    void onCreate() override {
        gam::sampleRate(audioIO().framesPerSecond());
        SynthParams params1{};
        SynthParams params2{};
        SynthParams params3{};

        params1.attack = 10.0f;
        params1.volume = 0.8f;
        params1.tempo = 80.0f;
        params.push_back(params1);
        params2.attack = 10.0f;
        params2.volume = 0.8f;
        params2.tempo = 80.0f;
        params.push_back(params2);
        params3.attack = 10.0f;
        params3.volume = 0.8f;
        params3.tempo = 80.0f;
        params.push_back(params3);
        for(float i = 0; i < 80; i+=4.0f){
            voices1.push_back(createNotes(261.63f, i, 2.0f, &seq1, 0));
            voices2.push_back(createNotes(293.66f, i, 2.0f, &seq2, 1));
            voices3.push_back(createNotes(392.00f, i, 2.0f, &seq3, 2));
            voices1.push_back(createNotes(293.66f, i+2.0f, 2.0f, &seq1, 0));
            voices2.push_back(createNotes(493.88, i+2.5f, 1.5f, &seq2, 1));
            voices3.push_back(createNotes(587.33, i+3.0f, 1.0f, &seq3, 2));
        }
        seq1.playSequence();
        seq2.playSequence();
        seq3.playSequence();

    }

    void onSound(AudioIOData &io) override {
        seq1.render(io);
        seq2.render(io);
        seq3.render(io);
    }

    SynthVoice* createNotes(float freq, float start, float duration, SynthSequencer * seq, int instrumentNum){
        SynthVoice *voice;
        voice = sine.synth().getVoice<GuitarVoice>();
        voice->setInternalParameterValue("amplitude", params[instrumentNum].volume);
        voice->setInternalParameterValue("attackTime", params[instrumentNum].attack);
        voice->setInternalParameterValue("frequency", freq/2);
        seq->addVoiceFromNow(voice, start, duration);
        return voice;
    }

    bool onKeyDown(Keyboard const &k) override {
        std::cout << k.key() << std::endl;
        int keyNum = k.key();
        switch (keyNum) {
            case 270: // Arrow Up
                for(int i = 0; i < voices1.size(); i++){
                    voices1.at(i)->setInternalParameterValue("amplitude",
                                                             voices1.at(i)->getInternalParameterValue("amplitude") + .1);
                }
                break;
            case 272: // Arrow Down
                for(int i = 0; i < voices1.size(); i++) {

                    voices1.at(i)->setInternalParameterValue("amplitude",
                                                             voices1.at(i)->getInternalParameterValue("amplitude") -
                                                             .1);
                }
                break;
            case 269: // Arrow Left
                for(int i = 0; i < voices1.size(); i++){
                    voices1.at(i)->setInternalParameterValue("attackTime",
                                                             voices1.at(i)->getInternalParameterValue("attackTime") + .1);
                }
                break;
            case 271: // Arrow Right
                for(int i = 0; i < voices1.size(); i++) {

                    voices1.at(i)->setInternalParameterValue("attackTime",
                                                             voices1.at(i)->getInternalParameterValue("attackTime") -
                                                             .1);
                }
                break;

            case 46: // '.' key
                for(int i = 0; i < voices2.size(); i++) {
                    voices2.at(i)->setInternalParameterValue("amplitude",
                                                             voices2.at(i)->getInternalParameterValue("amplitude") +
                                                             .1);
                }
                break;
            case 47: // '/' key
                for(int i = 0; i < voices2.size(); i++) {
                    voices2.at(i)->setInternalParameterValue("amplitude",
                                                             voices2.at(i)->getInternalParameterValue("amplitude") -
                                                             .1);
                }
                break;
            case 59: // ';' key
                for(int i = 0; i < voices3.size(); i++) {
                    voices3.at(i)->setInternalParameterValue("amplitude",
                                                             voices3.at(i)->getInternalParameterValue("amplitude") +
                                                             .1);
                }
                break;
            case 39: // '"' key
                for(int i = 0; i < voices3.size(); i++) {
                    voices3.at(i)->setInternalParameterValue("amplitude",
                                                             voices3.at(i)->getInternalParameterValue("amplitude") -
                                                             .1);
                }
                break;
            case 32: // Space Bar
                break;
        }
        return true;
    }
};

// We make an app.
class GuyApp : public App
{
public:
    // GUI manager for SineEnv voices
    // The name provided determines the name of the directory
    // where the presets and sequences are stored
    SynthGUIManager<GuitarVoice> synthManager{"GuitarEnv"};

    // This function is called right after the window is created
    // It provides a graphics context to initialize ParameterGUI
    // It's also a good place to put things that should
    // happen once at startup.
    void onCreate() override
    {
        navControl().active(false); // Disable navigation via keyboard, since we
        // will be using keyboard for note triggering

        // Set sampling rate for Gamma objects from app's audio
        gam::sampleRate(audioIO().framesPerSecond());

        imguiInit();

        // Play example sequence. Comment this line to start from scratch
        // synthManager.synthSequencer().playSequence("synth1.synthSequence");
        synthManager.synthRecorder().verbose(true);
    }

    // The audio callback function. Called when audio hardware requires data
    void onSound(AudioIOData &io) override
    {
        synthManager.render(io); // Render audio
    }

    void onAnimate(double dt) override {
        // The GUI is prepared here
        imguiBeginFrame();
        // Draw a window that contains the synth control panel
        // synthManager.drawSynthControlPanel();
        imguiEndFrame();
        synthManager.synth().update(dt);
    }

    // Whenever a key is pressed, this function is called
    bool onKeyDown(Keyboard const &k) override
    {
        // const float A4 = 432.f;
        const float A4 = 440.f;
        if (ParameterGUI::usingKeyboard())
        { // Ignore keys if GUI is using
            // keyboard
            return true;
        }
        if (k.shift())
        {
            // If shift pressed then keyboard sets preset
            int presetNumber = asciiToIndex(k.key());
            synthManager.recallPreset(presetNumber);
        }
        else
        {
            // Otherwise trigger note for polyphonic synth
            int midiNote = asciiToMIDI(k.key());
            if (midiNote > 0)
            {
                synthManager.voice()->setInternalParameterValue(
                        "frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * A4);
                synthManager.triggerOn(midiNote);
            }
        }
        return true;
    }

    // Whenever a key is released this function is called
    bool onKeyUp(Keyboard const &k) override
    {
        int midiNote = asciiToMIDI(k.key());
        if (midiNote > 0)
        {
            // synthManager.triggerOff(midiNote);
        }
        return true;
    }

    void onExit() override { imguiShutdown(); }
};

int main() {
    MyApp app;
    app.configureAudio(44100., 512, 2, 0);
    app.start();
}

