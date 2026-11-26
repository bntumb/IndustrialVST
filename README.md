# Foundry - hard-tuned industrial vocal processor

Signal chain:
  mono vocal -> pitch tracking + instant snap to key/scale (hard "Auto-Tune" style effect)
  (+ optional octave-down Sub and fifth-up Stack layers)
  -> ring mod -> low cut -> 4x-oversampled shaper (hard clip / wavefold / fuzz)
  -> bit + sample-rate crush -> brightness shelf -> parallel mix with the clean tuned voice
  -> compressor ("Slam") + soft ceiling -> slapback + short plate -> output

## Quick start
1. Put it on a **mono vocal** track. Set **Key** and **Scale** to your song (default is C minor).
2. **Retune** at 1 ms and **Tune** at 1.0 = hard-snapped robotic pitch. Higher Retune = slower glide between notes.
3. **Voice Range**: pick the highest range that still covers your lowest note (lower latency). Low = 40 ms, Mid = 26 ms, High = 17 ms.
4. **Drive** pushes into the shaper. **Shape**: Hard clip = brutal, Wavefold = metallic/harsh, Fuzz = thick and asymmetric.
5. **Bits / Rate** = digital crunch (Bits 16 and Rate 1 = off). **Ring** + **Ring Hz** = metallic clang.
6. **Mix** blends the distorted path with the clean tuned vocal (keep some clean for intelligibility).
7. **Slam** = heavy compression. **Slap** = slapback echo. **Room** = short plate.
- **MIDI chords**: hold notes on a MIDI keyboard and the Sub/Stack layers sing those notes instead.

## Build on GitHub (no installs)
Repo root must contain CMakeLists.txt, all .h/.cpp files and the `.github/workflows` folder.
Push -> Actions tab -> download the **Foundry-VST3-Windows** artifact -> copy `Foundry.vst3` to
`C:\Program Files\Common Files\VST3\`.

JUCE licence: free for open-source / under its revenue threshold; check juce.com/juce-licence.
