/*
 * Copyright (C) 2026 The symbian-webkit authors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "config.h"

#if ENABLE(WEB_AUDIO) && OS(SYMBIAN)

#include "FFTFrame.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace WebCore {

namespace {

constexpr unsigned minimumFFTSize = 4;
// Enough for Web Audio's largest analyser and the existing convolution stages.
// Per-frame plan plus packed data stays below 513 KiB at this bound.
constexpr unsigned maximumFFTSize = 32768;

unsigned checkedFFTSize(unsigned size)
{
    RELEASE_ASSERT(size >= minimumFFTSize && size <= maximumFFTSize && std::has_single_bit(size));
    return size;
}

} // namespace

// All storage and trigonometric work belongs to construction. Rendering only
// does bounded butterfly work; it does not allocate or use a global plan cache.
class SymbianFFTPlan {
public:
    explicit SymbianFFTPlan(unsigned size)
        : real(size)
        , imaginary(size)
        , cosine(size / 2)
        , sine(size / 2)
    {
        for (unsigned i = 0; i < size / 2; ++i) {
            double angle = -2 * std::numbers::pi * i / size;
            cosine[i] = static_cast<float>(std::cos(angle));
            sine[i] = static_cast<float>(std::sin(angle));
        }
    }

    void transform(bool inverse)
    {
        unsigned size = real.size();
        for (unsigned i = 1, reversed = 0; i < size; ++i) {
            unsigned bit = size >> 1;
            for (; reversed & bit; bit >>= 1)
                reversed ^= bit;
            reversed ^= bit;
            if (i < reversed) {
                std::swap(real[i], real[reversed]);
                std::swap(imaginary[i], imaginary[reversed]);
            }
        }

        for (unsigned width = 2; width <= size; width <<= 1) {
            unsigned halfWidth = width / 2;
            unsigned stride = size / width;
            for (unsigned base = 0; base < size; base += width) {
                for (unsigned i = 0; i < halfWidth; ++i) {
                    unsigned upper = base + i;
                    unsigned lower = upper + halfWidth;
                    unsigned twiddle = i * stride;
                    float wr = cosine[twiddle];
                    float wi = inverse ? -sine[twiddle] : sine[twiddle];
                    float tr = wr * real[lower] - wi * imaginary[lower];
                    float ti = wr * imaginary[lower] + wi * real[lower];
                    float ur = real[upper];
                    float ui = imaginary[upper];
                    real[upper] = ur + tr;
                    imaginary[upper] = ui + ti;
                    real[lower] = ur - tr;
                    imaginary[lower] = ui - ti;
                }
            }
        }
    }

    AudioFloatArray real;
    AudioFloatArray imaginary;
    AudioFloatArray cosine;
    AudioFloatArray sine;
};

FFTFrame::FFTFrame(unsigned size)
    : m_FFTSize(checkedFFTSize(size))
    , m_log2FFTSize(std::countr_zero(m_FFTSize))
    , m_realData(m_FFTSize / 2)
    , m_imagData(m_FFTSize / 2)
    , m_symbianPlan(std::make_unique<SymbianFFTPlan>(m_FFTSize))
{
}

FFTFrame::FFTFrame()
    : m_FFTSize(0)
    , m_log2FFTSize(0)
{
}

FFTFrame::FFTFrame(const FFTFrame& other)
    : m_FFTSize(other.m_FFTSize)
    , m_log2FFTSize(other.m_log2FFTSize)
    , m_realData(m_FFTSize / 2)
    , m_imagData(m_FFTSize / 2)
    , m_symbianPlan(m_FFTSize ? std::make_unique<SymbianFFTPlan>(m_FFTSize) : nullptr)
{
    std::copy(other.realData().span().begin(), other.realData().span().end(), m_realData.span().begin());
    std::copy(other.imagData().span().begin(), other.imagData().span().end(), m_imagData.span().begin());
}

FFTFrame::~FFTFrame() = default;

void FFTFrame::initialize()
{
}

void FFTFrame::doFFT(std::span<const float> data)
{
    RELEASE_ASSERT(m_symbianPlan && data.size() >= m_FFTSize);
    auto& plan = *m_symbianPlan;
    std::copy_n(data.begin(), m_FFTSize, plan.real.span().begin());
    plan.imaginary.zero();
    plan.transform(false);

    // FFTFrame's common multiply/delay code uses packed DC and Nyquist. The
    // zeroth imaginary slot holds the real Nyquist bin, not Im(X[0]).
    m_realData[0] = plan.real[0];
    m_imagData[0] = plan.real[m_FFTSize / 2];
    for (unsigned i = 1; i < m_FFTSize / 2; ++i) {
        m_realData[i] = plan.real[i];
        m_imagData[i] = plan.imaginary[i];
    }
}

void FFTFrame::doInverseFFT(std::span<float> data)
{
    RELEASE_ASSERT(m_symbianPlan && data.size() >= m_FFTSize);
    auto& plan = *m_symbianPlan;
    plan.real[0] = m_realData[0];
    plan.imaginary[0] = 0;
    plan.real[m_FFTSize / 2] = m_imagData[0];
    plan.imaginary[m_FFTSize / 2] = 0;
    for (unsigned i = 1; i < m_FFTSize / 2; ++i) {
        plan.real[i] = plan.real[m_FFTSize - i] = m_realData[i];
        plan.imaginary[i] = m_imagData[i];
        plan.imaginary[m_FFTSize - i] = -m_imagData[i];
    }
    plan.transform(true);
    float scale = 1.0f / m_FFTSize;
    for (unsigned i = 0; i < m_FFTSize; ++i)
        data[i] = plan.real[i] * scale;
}

int FFTFrame::minFFTSize()
{
    return minimumFFTSize;
}

int FFTFrame::maxFFTSize()
{
    return maximumFFTSize;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO) && OS(SYMBIAN)
