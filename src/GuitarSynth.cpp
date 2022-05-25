#include <iostream>
#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

// for master branch
#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/app/al_App.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

using namespace al;

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
        rb[lastIndex % sizeTotal] = x;
        lastIndex++;
    }
    double dequeue(){
        sizeCurr--;
        firstIndex++;
        return rb[lastIndex % sizeTotal];
    }
    double peek(){
        return rb[lastIndex % sizeTotal];
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
        ring = new RingBuffer(44100/freq);
        for(int i = 0; i < 44100/freq; i++){
            ring->enqueue(0);
        }
        itemSize = 44100/freq;
    }
    void pluck(){
        for(int i = 0; i < itemSize; i++){
            ring->enqueue((rand()%2)/2.0 - 0.5);
        }
    }
    void tic(){
        double first = ring->dequeue();
        double second = ring->peek();
        ring->enqueue((first + second)/2.0);
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
            double sumStrings = 0;
//            for(int i = 0; i < notesPlayed.size(); i++){
//                sumStrings += notesPlayed.at(i).sample();
//            }
            float s1 = sumStrings * mAmpEnv() * getInternalParameterValue("amplitude");
            float s2;
            mPan(s1, s1, s2);
            io.out(0) += s1;
            io.out(1) += s2;
        }
        if (mAmpEnv.done())
            free();
    }

    void onTriggerOn() override {
//        notesPlayed.push_back(GuitarString(getInternalParameterValue("frequency")));
//        notesPlayed.at(notesPlayed.size()-1).pluck();
        mAmpEnv.reset();
    }

    void onTriggerOff() override { mAmpEnv.release(); }
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

    void onAnimate(double dt) override
    {
        // The GUI is prepared here
        imguiBeginFrame();
        // Draw a window that contains the synth control panel
        // synthManager.drawSynthControlPanel();
        imguiEndFrame();
//        for(int i = 0; i < synthManager.voice()->notesPlayed.size(); i++){
////            synthManager.voice()->notesPlayed.at(i).tic();
//        }
    }

    // The graphics callback function.
    void onDraw(Graphics &g) override
    {
        g.clear();
        // Render the synth's graphics
        //synthManager.render(g);

        // GUI is drawn here
        imguiDraw();
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
    GuyApp app;
    app.configureAudio(44100., 512, 2, 0);
    app.start();
}
