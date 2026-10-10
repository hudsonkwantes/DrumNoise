
#include "daisy_seed.h"
#include "daisysp.h"
#include "touch/touch.h"

#include "engines/LowEnd.h"
#include "engines/Metal.h"
#include "engines/Static.h"
#include "engines/Feedback.h"
#include "engines/Sample.h"

#include "rhythm/Clock.h"
#include "rhythm/Transport.h"
#include "rhythm/Sequencer.h"

#include "effects/MasterReverb.h"

#include <cmath>
#include <cstdint>

using namespace daisy;
using namespace daisysp;
using namespace synthux;

// HARDWARE
static DaisySeed hw;
static Touch touch;


// SOUND ENGINES
static LowEnd lowEnd;
static Metal metal;
static Static staticVoice;
static Feedback feedbackVoice;
static Sample sampleVoice;

// MASTER REVERB
// Allocate reverb delay memory in external SDRAM.
static MasterReverb DSY_SDRAM_BSS masterReverb;

static volatile float reverbMix = 0.0f;

// RHYTHM
static Clock masterClock;
static Transport transport;
static Sequencer sequencer;

static volatile int requestedMode = Transport::STOP;
static volatile float bpm = 120.0f;

static uint32_t pendingHits = 0;
static uint32_t pendingClickToggles = 0;
static volatile uint32_t liveGateMask = 0;

// Independent loop settings
static volatile int loopStart[5] = {0, 0, 0, 0, 0};
static volatile int loopEnd[5] = {15, 15, 15, 15, 15};
static volatile int loopSpeed[5] = {2, 2, 2, 2, 2};

static volatile int currentStep = 0;
static volatile bool recordingActive = false;

// VOICE PARAMETERS
// LowEnd
static volatile float lowPitch = 150.0f;
static volatile float lowTexture = 0.5f;
static volatile float lowDrive = 5.0f;
// Metal
static volatile float metalPitch = 220.0f;
static volatile float metalTexture = 0.5f;
static volatile float metalDrive = 5.0f;
// Static
static volatile float staticPitch = 1500.0f;
static volatile float staticTexture = 0.5f;
static volatile float staticDrive = 5.0f;
// Feedback
static volatile float feedbackPitch = 220.0f;
static volatile float feedbackTexture = 0.5f;
static volatile float feedbackDrive = 5.0f;
// Sample
static volatile float samplePitch = 1.0f;
static volatile float sampleTexture = 0.0f;
static volatile float sampleDrive = 5.0f;

// GATES
static float gateLevel[5] = {};
static int selectedVoice = 0;

// CLICK TRACK
static bool clickEnabled = false;
static int clickSamples = 0;
static float clickPhase = 0.0f;

// AUDIO CALLBACK
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // ENGINE PARAMETERS
    lowEnd.SetFrequency(lowPitch);
    lowEnd.SetPitchEnvelope(lowTexture);
    lowEnd.SetDrive(lowDrive);

    metal.SetFrequency(metalPitch);
    metal.SetTexture(metalTexture);
    metal.SetDrive(metalDrive);

    staticVoice.SetFrequency(staticPitch);
    staticVoice.SetTexture(staticTexture);
    staticVoice.SetDrive(staticDrive);

    feedbackVoice.SetFrequency(feedbackPitch);
    feedbackVoice.SetTexture(feedbackTexture);
    feedbackVoice.SetDrive(feedbackDrive);

    sampleVoice.SetFrequency(samplePitch);
    sampleVoice.SetTexture(sampleTexture);
    sampleVoice.SetDrive(sampleDrive);

    masterReverb.SetMix(reverbMix);

    // INDEPENDENT LOOP SETTINGS

    for(int v = 0; v < 5; ++v){
        sequencer.SetLoop(v,loopStart[v],loopEnd[v]);
        sequencer.SetSpeed(v, loopSpeed[v]);
    }

    // CLICK TOGGLE
    uint32_t clickToggles = __atomic_exchange_n(
        &pendingClickToggles,
        0u,
        __ATOMIC_RELAXED
    );

    if(clickToggles & 1u)
        clickEnabled = !clickEnabled;

    // TRANSPORT

    Transport::Mode nextMode =
        static_cast<Transport::Mode>(requestedMode);

    Transport::Mode previousMode =
        transport.GetMode();

    if(nextMode != previousMode){
        // Starting from STOP resets the clock.
        if(previousMode == Transport::STOP && nextMode != Transport::STOP){
            masterClock.Reset();
            currentStep = 0;

            // Synchronize the first kick envelope.
            lowEnd.ClockStep(bpm);
            clickSamples = 960;
            clickPhase = 0.0f;
        }

        // Entering RECORD replaces patterns but
        // preserves individual loop settings.
        if(nextMode == Transport::RECORD){
            sequencer.ClearPatterns();
        }

        transport.SetMode(nextMode);
    }

    recordingActive = transport.Recording();

    const float sampleRate = hw.AudioSampleRate();

    // AUDIO PROCESSING
    for(size_t i = 0; i < size; ++i){

    // MASTER CLOCK

    if(transport.Running()){
        bool tick = masterClock.Process(bpm);

        if(tick){
            uint32_t ticks = masterClock.Ticks();

            // Every four ticks = one 16th note.//not correct******************
            if((ticks % 4u) == 0u){
                currentStep = masterClock.Step();
                // Kick pitch envelope follows
                // the master tempo and steps.
                lowEnd.ClockStep(bpm);
            }

            // Quarter-note click
            if((ticks % 16u) == 0u){
                clickSamples = 960;
                clickPhase = 0.0f;
            }
        }
    }

    // RECORD GATE EVENTS
        uint32_t hits = __atomic_exchange_n(&pendingHits, 0u, __ATOMIC_RELAXED);

        if(transport.Recording()){
            for(int v = 0; v < 5; ++v){

                if(hits & (1u << v)){

                    sequencer.Record(v,masterClock.Ticks());
                }
            }
        }

        // FULL-STEP GATES
        uint32_t liveMask = liveGateMask;

        for(int v = 0; v < 5; ++v){

            bool manualGate = (liveMask & (1u << v)) != 0;
            bool sequencedGate = false;

            if(transport.Running()){
                sequencedGate = sequencer.Gate(v, masterClock.Ticks());
            }

            bool gateOpen = manualGate || sequencedGate;
            float target = gateOpen ? 1.0f : 0.0f;

            // Smooth gate edges while keeping
            // the gate open for the entire step.
            gateLevel[v] += (target - gateLevel[v]) * 0.015f;
        }

        // CONTINUOUS SOUND ENGINES
        float bass = lowEnd.Process() * gateLevel[0];
        float clang = metal.Process() * gateLevel[1];
        float noise = staticVoice.Process() * gateLevel[2];
        float grinding = feedbackVoice.Process() * gateLevel[3];
        float radio = sampleVoice.Process() * gateLevel[4];

        // Mix all five voices.
        float dry = bass + clang + noise + grinding + radio;

        // CLICK TRACK
        if(clickSamples > 0){

            if(clickEnabled && transport.Running()){
                float envelope = (float)clickSamples / 960.0f;
                dry += sinf(clickPhase) * envelope * 0.15f;
            }

            clickPhase += 6.2831853f * 1000.0f / sampleRate;

            if(clickPhase >= 6.2831853f)
                clickPhase -= 6.2831853f;

            --clickSamples;
        }

        // MASTER REVERB

        dry = tanhf(dry);

        float left = 0.0f;
        float right = 0.0f;

        masterReverb.Process(dry, left, right);

        // Output limiting
        out[0][i] = tanhf(left);
        out[1][i] = tanhf(right);
    }
}

// MAIN
int main(void){

    hw.Init(true);
    hw.SetAudioBlockSize(16);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    touch.Init(hw);

    float sampleRate = hw.AudioSampleRate();

    // INITIALIZE ENGINES
    lowEnd.Init(sampleRate);
    metal.Init(sampleRate);
    staticVoice.Init(sampleRate);
    feedbackVoice.Init(sampleRate);
    sampleVoice.Init(sampleRate);

    // INITIALIZE EFFECTS
    masterReverb.Init(sampleRate);

    // INITIALIZE RHYTHM
    masterClock.Init(sampleRate);
    transport.Init();
    sequencer.Init();

    // TOUCH CALIBRATION
    for(int i = 0; i < 40; ++i){
        touch.Process();
        System::Delay(3);
    }

    bool previousPad[8] = {};

    // INITIAL KNOB POSITIONS
    float previousPitch = touch.knobs().s31();
    float previousTexture = touch.knobs().s33();
    float previousDrive = touch.knobs().s34();

    float previousStart = touch.knobs().s30();
    float previousEnd = touch.knobs().s35();
    float previousSpeed = touch.knobs().s37();

    // S36: Master tempo
    bpm = 60.0f + touch.knobs().s36() * 120.0f;

    // S32: Global reverb
    reverbMix = touch.knobs().s32();

    // Physical transport switch
    requestedMode = touch.switches().A();

    hw.StartAudio(AudioCallback);

    // MAIN CONTROL LOOP
    while(1)
    {
        touch.Process();

        // PHYSICAL TRANSPORT SWITCH
        int switchPosition = touch.switches().A();

        if(switchPosition >= 0 && switchPosition <= 2){
            requestedMode = switchPosition;
        }

        // TOUCH PADS
        bool pad[8];

        for(int p = 0; p < 8; ++p){
            pad[p] = touch.pads().IsTouched(p);
        }

        // Pad 0: Click toggle
        if(pad[0] && !previousPad[0]){
            __atomic_fetch_add(&pendingClickToggles, 1u, __ATOMIC_RELAXED);
        }

        // Pads 3–7: Five voices
        uint32_t mask = 0;

        for(int v = 0; v < 5; ++v){
            int padIndex = v + 3;

            if(pad[padIndex]) mask |= (1u << v);

            if(pad[padIndex] && !previousPad[padIndex]){
                
                selectedVoice = v;
                __atomic_fetch_or(&pendingHits, (1u << v), __ATOMIC_RELAXED);
            }
        }

        liveGateMask = mask;

        for(int p = 0; p < 8; ++p)
            previousPad[p] = pad[p];

        // GLOBAL CONTROLS
        // S36: Global BPM
        bpm = 60.0f + touch.knobs().s36() * 120.0f;

        // S32: Global reverb mix
        reverbMix = touch.knobs().s32();

        // SELECTED VOICE CONTROLS
        float pitch = touch.knobs().s31();
        float texture = touch.knobs().s33();
        float drive = touch.knobs().s34();

        float start = touch.knobs().s30();
        float end = touch.knobs().s35();
        float speed = touch.knobs().s37();

        int v = selectedVoice;

        // S30: LOOP START
        if(fabsf(start - previousStart) > 0.005f){
            int step = (int)(start * 16.0f);

            if(step > 15)
                step = 15;

            loopStart[v] = step;

            if(loopEnd[v] < step)
                loopEnd[v] = step;

            previousStart = start;
        }

        // ------------------------------------------
        // S35: LOOP END
        // ------------------------------------------

        if(fabsf(end - previousEnd) > 0.005f){
            int step = (int)(end * 16.0f);

            if(step > 15)
                step = 15;

            loopEnd[v] = step;

            if(loopStart[v] > step)
                loopStart[v] = step;

            previousEnd = end;
        }

        // ------------------------------------------
        // S37: LOOP SPEED
        // ------------------------------------------

        if(fabsf(speed - previousSpeed) > 0.005f)
        {
            int index = (int)(speed * 5.0f);

            if(index > 4)
                index = 4;

            loopSpeed[v] = index;

            previousSpeed = speed;
        }

        // ------------------------------------------
        // S31: PITCH
        // ------------------------------------------

        if(fabsf(pitch - previousPitch) > 0.005f)
        {
            if(v == 0)
                lowPitch =
                    90.0f + pitch * pitch * 210.0f;

            else if(v == 1)
                metalPitch =
                    80.0f + pitch * pitch * 1120.0f;

            else if(v == 2)
                staticPitch =
                    100.0f + pitch * pitch * 7900.0f;

            else if(v == 3)
                feedbackPitch =
                    80.0f + pitch * pitch * 1920.0f;

            else if(v == 4)
                samplePitch =
                    0.25f + pitch * pitch * 3.75f;

            previousPitch = pitch;
        }

        // ------------------------------------------
        // S33: TEXTURE
        // ------------------------------------------

        if(fabsf(texture - previousTexture) > 0.005f)
        {
            if(v == 0)
                lowTexture = texture;

            else if(v == 1)
                metalTexture = texture;

            else if(v == 2)
                staticTexture = texture;

            else if(v == 3)
                feedbackTexture = texture;

            else if(v == 4)
                sampleTexture = texture;

            previousTexture = texture;
        }

        // ------------------------------------------
        // S34: DRIVE
        // ------------------------------------------

        if(fabsf(drive - previousDrive) > 0.005f)
        {
            float value =
                1.0f + drive * 14.0f;

            if(v == 0)
                lowDrive = value;

            else if(v == 1)
                metalDrive = value;

            else if(v == 2)
                staticDrive = value;

            else if(v == 3)
                feedbackDrive = value;

            else if(v == 4)
                sampleDrive = value;

            previousDrive = drive;
        }

        // ------------------------------------------
        // LED
        // ------------------------------------------

        hw.SetLed(
            recordingActive || mask != 0
        );

        System::Delay(3);
    }
}
