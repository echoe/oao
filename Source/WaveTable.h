#pragma once
#include <JuceHeader.h>

class WaveTable
{
public:
    static constexpr int tableSize = 2048; // Power of 2, good balance of quality vs memory

    void prepare()
    {
        const float twoPi = juce::MathConstants<float>::twoPi;

        // Sine
        for (int i = 0; i < tableSize; ++i)
            sineTable[i] = std::sin((float)i / tableSize * twoPi);

        // Triangle
        for (int i = 0; i < tableSize; ++i)
        {
            float phase = (float)i / tableSize;
            triangleTable[i] = 1.0f - 2.0f * std::abs(1.0f - phase * 2.0f);
        }

        // Sawtooth (band-limited via additive synthesis up to Nyquist)
        sawTable.fill(0.0f);
        for (int harmonic = 1; harmonic <= 64; ++harmonic)
        {
            float amp = 1.0f / harmonic;
            for (int i = 0; i < tableSize; ++i)
                sawTable[i] += amp * std::sin((float)i / tableSize * twoPi * harmonic);
        }
        normalizeTable(sawTable);

        // Square (band-limited, odd harmonics only)
        squareTable.fill(0.0f);
        for (int harmonic = 1; harmonic <= 64; harmonic += 2)
        {
            float amp = 1.0f / harmonic;
            for (int i = 0; i < tableSize; ++i)
                squareTable[i] += amp * std::sin((float)i / tableSize * twoPi * harmonic);
        }
        normalizeTable(squareTable);
    }

    float lookupSine     (float phase) const { return lookup(sineTable,     phase); }
    float lookupTriangle (float phase) const { return lookup(triangleTable,  phase); }
    float lookupSaw      (float phase) const { return lookup(sawTable,       phase); }
    float lookupSquare   (float phase) const { return lookup(squareTable,    phase); }

private:
    std::array<float, tableSize> sineTable;
    std::array<float, tableSize> triangleTable;
    std::array<float, tableSize> sawTable;
    std::array<float, tableSize> squareTable;

    float lookup(const std::array<float, tableSize>& table, float phase) const
    {
        // Normalize phase to [0, tableSize)
        float scaled = (phase / juce::MathConstants<float>::twoPi) * tableSize;
        int index0   = (int)scaled % tableSize;
        int index1   = (index0 + 1) % tableSize;
        float frac   = scaled - (int)scaled;

        // Linear interpolation between adjacent samples
        return table[index0] + frac * (table[index1] - table[index0]);
    }

    void normalizeTable(std::array<float, tableSize>& table)
    {
        float maxVal = *std::max_element(table.begin(), table.end(),
            [](float a, float b) { return std::abs(a) < std::abs(b); });
        if (maxVal > 0.0f)
            for (auto& s : table) s /= maxVal;
    }
};
