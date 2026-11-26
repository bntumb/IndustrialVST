#pragma once
// FoundryChain: hard Auto-Tune style retuning followed by an industrial distortion chain.
// JUCE-free so it can be tested offline.
//
//   mono in -> pitch track + hard snap (+ sub / fifth layers)
//           -> ring mod -> low cut -> 4x oversampled shaper (clip / fold / fuzz)
//           -> bit + sample-rate crush -> brightness shelf -> parallel mix with the clean tuned voice
//           -> compressor ("slam") + soft clip -> slapback + short plate -> out

#include "HarmonizerEngine.h"
#include "Dsp.h"

#include <array>
#include <cmath>

class FoundryChain
{
public:
    static constexpr int chunk = 512;

    struct Params
    {
        HarmonizerEngine::Params tuner;
        int   shape    = 1;        // 0 hard clip, 1 wavefold, 2 fuzz
        float driveDb  = 16.0f;
        float bits     = 11.0f;    // 16 = off
        int   rate     = 3;        // sample-rate reduction factor, 1 = off
        float ring     = 0.2f;     // ring mod amount
        float ringHz   = 140.0f;
        float lowcutHz = 90.0f;
        float brightDb = 4.0f;
        float gritMix  = 0.85f;
        float slam     = 0.6f;
        float slap     = 0.15f;
        float room     = 0.12f;
        float outDb    = -3.0f;
    };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        engine.prepare (sampleRate);
        reverb.prepare (sampleRate);
        reverb.setSize (0.25f);
        comp.prepare (sr);

        for (int c = 0; c < 2; ++c)
        {
            os[c].reset();
            lowcut[c].reset();
            shelf[c].reset();
            cleanBuf[(size_t) c].fill (0.0f);
        }
        cleanPos = 0;
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f;
        holdCount = 0; heldL = heldR = 0.0f;
        ringPhase = 0.0f;

        slapSize = 1; while ((double) slapSize < 0.3 * sampleRate) slapSize <<= 1;
        slapL.assign ((size_t) slapSize, 0.0f);
        slapR.assign ((size_t) slapSize, 0.0f);
        slapW = 0;
        slapLpL = slapLpR = 0.0f;

        smCoef = 1.0f - std::exp (-1.0f / (0.015f * sr));
        snap = true;
        lastLowcut = lastBright = -1.0f;
    }

    int latencySamples() const { return engine.latencySamples() + Oversampler4x::latency; }
    float debugInputMidi() const { return engine.debugInputMidi(); }
    float debugLeadMidi()  const { return engine.debugLeadMidi(); }

    // in: mono, n <= chunk. Output is stereo.
    void process (const float* in, float* outL, float* outR, int n, const Params& p, const bool* held)
    {
        n = std::min (n, chunk);

        if (std::abs (p.lowcutHz - lastLowcut) > 0.01f)
        {
            lowcut[0].setHighPass (sr, p.lowcutHz); lowcut[1].setHighPass (sr, p.lowcutHz);
            lastLowcut = p.lowcutHz;
        }
        if (std::abs (p.brightDb - lastBright) > 0.01f)
        {
            shelf[0].setHighShelf (sr, 3500.0f, p.brightDb); shelf[1].setHighShelf (sr, 3500.0f, p.brightDb);
            lastBright = p.brightDb;
        }

        // 1. tune
        engine.process (in, tL.data(), tR.data(), n, p.tuner, held);

        const float driveT = std::pow (10.0f, p.driveDb / 20.0f);
        const float outT   = std::pow (10.0f, p.outDb / 20.0f);
        if (snap)
        {
            driveS = driveT; ringS = p.ring; mixS = p.gritMix; slapS = p.slap; roomS = p.room; outS = outT;
            snap = false;
        }
        const float ringInc = 6.2831853f * p.ringHz / sr;

        // 2. ring mod + low cut, and the clean (latency-matched) copy
        for (int i = 0; i < n; ++i)
        {
            driveS += smCoef * (driveT - driveS);
            ringS  += smCoef * (p.ring - ringS);
            mixS   += smCoef * (p.gritMix - mixS);
            slapS  += smCoef * (p.slap - slapS);
            roomS  += smCoef * (p.room - roomS);
            outS   += smCoef * (outT - outS);
            driveArr[(size_t) i] = driveS; mixArr[(size_t) i] = mixS;
            slapArr[(size_t) i] = slapS;   roomArr[(size_t) i] = roomS; outArr[(size_t) i] = outS;

            cleanBuf[0][(size_t) cleanPos] = tL[(size_t) i];
            cleanBuf[1][(size_t) cleanPos] = tR[(size_t) i];
            const int rp = (cleanPos - Oversampler4x::latency) & 63;
            cL[(size_t) i] = cleanBuf[0][(size_t) rp];
            cR[(size_t) i] = cleanBuf[1][(size_t) rp];
            cleanPos = (cleanPos + 1) & 63;

            ringPhase += ringInc; if (ringPhase > 6.2831853f) ringPhase -= 6.2831853f;
            const float mod = 1.0f - ringS + ringS * std::sin (ringPhase);
            gL[(size_t) i] = lowcut[0].process (tL[(size_t) i] * mod);
            gR[(size_t) i] = lowcut[1].process (tR[(size_t) i] * mod);
        }

        // 3. shaper at 4x the sample rate (keeps the distortion from aliasing into mush)
        os[0].up (gL.data(), n, upL.data(), tmp2.data());
        os[1].up (gR.data(), n, upR.data(), tmp2.data());
        const int total = 4 * n;
        for (int j = 0; j < total; ++j)
        {
            const float g = driveArr[(size_t) (j >> 2)];
            upL[(size_t) j] = shape (p.shape, upL[(size_t) j] * g) * 0.7f;
            upR[(size_t) j] = shape (p.shape, upR[(size_t) j] * g) * 0.7f;
        }
        os[0].down (upL.data(), n, gL.data(), tmp2.data());
        os[1].down (upR.data(), n, gR.data(), tmp2.data());

        // 4. crush, tone, mix, slam, space
        const float levels  = std::exp2 (std::clamp (p.bits, 2.0f, 16.0f) - 1.0f);
        const bool  crushOn = p.bits < 15.9f;
        const int   rate    = std::max (1, p.rate);

        const float thr    = -6.0f - 24.0f * p.slam;
        const float ratio  = 4.0f + 8.0f * p.slam;
        const float makeup = 0.5f * (-thr) * (1.0f - 1.0f / ratio);
        const bool  slamOn = p.slam > 0.01f;

        const int dL = (int) (0.087f * sr), dR = (int) (0.123f * sr);
        const float lpC = 1.0f - std::exp (-6.2831853f * 4000.0f / sr);

        for (int i = 0; i < n; ++i)
        {
            float l = gL[(size_t) i], r = gR[(size_t) i];

            // remove DC left by the asymmetric shaper
            const float yl = l - dcX[0] + 0.995f * dcY[0]; dcX[0] = l; dcY[0] = yl; l = yl;
            const float yr = r - dcX[1] + 0.995f * dcY[1]; dcX[1] = r; dcY[1] = yr; r = yr;

            if (rate > 1)
            {
                if (++holdCount >= rate) { holdCount = 0; heldL = l; heldR = r; }
                l = heldL; r = heldR;
            }
            if (crushOn)
            {
                l = std::nearbyint (l * levels) / levels;
                r = std::nearbyint (r * levels) / levels;
            }

            l = shelf[0].process (l);
            r = shelf[1].process (r);

            const float m = mixArr[(size_t) i];
            l = cL[(size_t) i] * (1.0f - m) + l * m;
            r = cR[(size_t) i] * (1.0f - m) + r * m;

            if (slamOn)
            {
                const float gdb = comp.gain (std::max (std::abs (l), std::abs (r)), thr, ratio, makeup);
                l *= gdb; r *= gdb;
            }
            l = std::tanh (l); r = std::tanh (r);                  // soft ceiling

            // slapback (two different delays, filtered feedback) + short plate
            const float wl = slapL[(size_t) ((slapW - dL) & (slapSize - 1))];
            const float wr = slapR[(size_t) ((slapW - dR) & (slapSize - 1))];
            slapLpL += lpC * (wl - slapLpL);
            slapLpR += lpC * (wr - slapLpR);
            slapL[(size_t) slapW] = l + 0.3f * slapLpL;
            slapR[(size_t) slapW] = r + 0.3f * slapLpR;
            slapW = (slapW + 1) & (slapSize - 1);

            float rl, rr;
            reverb.process (0.5f * (l + r), rl, rr);

            const float s = slapArr[(size_t) i], rm = roomArr[(size_t) i], og = outArr[(size_t) i];
            outL[i] = (l + s * slapLpL + rm * rl) * og;
            outR[i] = (r + s * slapLpR + rm * rr) * og;
        }
    }

private:
    static float shape (int type, float v)
    {
        switch (type)
        {
            case 0:  return shapeHardClip (v);
            case 2:  return shapeFuzz (v);
            default: return shapeFold (v);
        }
    }

    HarmonizerEngine engine;
    PlateReverb reverb;
    Compressor comp;
    Oversampler4x os[2];
    Biquad lowcut[2], shelf[2];

    float sr = 48000.0f, smCoef = 0.001f;
    float driveS = 6.0f, ringS = 0.0f, mixS = 1.0f, slapS = 0.0f, roomS = 0.0f, outS = 1.0f;
    float ringPhase = 0.0f, lastLowcut = -1.0f, lastBright = -1.0f;
    float dcX[2] = {}, dcY[2] = {}, heldL = 0.0f, heldR = 0.0f;
    int   holdCount = 0, cleanPos = 0;
    bool  snap = true;

    std::array<std::array<float, 64>, 2> cleanBuf {};
    std::vector<float> slapL, slapR;
    int slapSize = 1, slapW = 0;
    float slapLpL = 0.0f, slapLpR = 0.0f;

    std::array<float, chunk> tL {}, tR {}, gL {}, gR {}, cL {}, cR {};
    std::array<float, chunk> driveArr {}, mixArr {}, slapArr {}, roomArr {}, outArr {};
    std::array<float, chunk * 4> upL {}, upR {};
    std::array<float, chunk * 2> tmp2 {};
};
