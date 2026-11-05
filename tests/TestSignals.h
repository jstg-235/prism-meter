#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

namespace prism::test
{

inline float dbToGain (double db) { return static_cast<float> (std::pow (10.0, db / 20.0)); }

/// Appends a sine of peak level levelDbfs to buffer.
inline void appendSine (std::vector<float>& buffer, double sampleRate, double hz, double levelDbfs,
                        double seconds, double phase = 0.0)
{
    const auto n = static_cast<size_t> (std::llround (seconds * sampleRate));
    const auto start = buffer.size();
    const float amp = dbToGain (levelDbfs);
    buffer.resize (start + n);

    for (size_t i = 0; i < n; ++i)
        buffer[start + i] = amp * static_cast<float> (std::sin (2.0 * std::numbers::pi * hz * static_cast<double> (i) / sampleRate + phase));
}

inline void appendSilence (std::vector<float>& buffer, double sampleRate, double seconds)
{
    buffer.resize (buffer.size() + static_cast<size_t> (std::llround (seconds * sampleRate)), 0.0f);
}

inline std::vector<float> whiteNoise (size_t n, float amplitude, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-amplitude, amplitude);
    std::vector<float> v (n);
    for (auto& x : v)
        x = dist (rng);
    return v;
}

/// Feeds a mono/stereo signal through a processor in uneven block sizes, like a host would.
template <typename Fn>
void processInBlocks (size_t totalSamples, Fn&& fn)
{
    static constexpr int sizes[] = { 512, 37, 1024, 1, 480, 2048, 64 };
    size_t pos = 0;
    int which = 0;

    while (pos < totalSamples)
    {
        const auto n = std::min (static_cast<size_t> (sizes[which++ % 7]), totalSamples - pos);
        fn (pos, static_cast<int> (n));
        pos += n;
    }
}

} // namespace prism::test
