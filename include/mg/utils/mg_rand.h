//**************************************************************************************************
// This file is part of Mg Engine. Copyright (c) 2020, Magnus Bergsten.
// Mg Engine is made available under the terms of the 3-Clause BSD License.
// See LICENSE.txt in the project's root directory.
//**************************************************************************************************

/** @file mg_rand.h
 * Pseudo-random number generator.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace Mg {

/** Pseudo-random number generator. */
class Random {
public:
    /** Construct new generator with the given seed. */
    explicit Random(const uint64_t seed) : m_state(seed) {}

    /** Get random float. */
    float f32()
    {
        constexpr uint32_t float_one_bits = 0x3F800000u;
        const uint32_t bits = float_one_bits | static_cast<uint32_t>(next()) >> 9u;
        float result = 0.0f;
        std::memcpy(&result, &bits, sizeof(result));
        return result - 1.0f;
    }

    /** Get random double. */
    double f64()
    {
        constexpr uint64_t double_one_bits = 0x3FF0000000000000u;
        const uint64_t bits = double_one_bits | next() >> 12u;
        double result = 0.0;
        std::memcpy(&result, &bits, sizeof(result));
        return result - 1.0;
    }

    /** Get random 32-bit signed integer. */
    int32_t i32()
    {
        const auto bits = static_cast<uint32_t>(next());
        int32_t result = 0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    /** Get random 32-bit unsigned integer. */
    uint32_t u32() { return static_cast<uint32_t>(next()); }

    /** Get random 64-bit signed integer. */
    int64_t i64()
    {
        const auto bits = next();
        int64_t result = 0;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    /** Get random 64-bit unsigned integer. */
    uint64_t u64() { return next(); }

    /** Get random integer in range [low, high]. Precondition: high - low < UINT32_MAX */
    template<typename IntT> IntT range(IntT low, IntT high)
    {
        static_assert(std::is_integral_v<IntT>);
        const IntT diff = high - low;
        return low + static_cast<IntT>(bounded_u32(diff + 1));
    }

    /** Get random float in range [low, high]. */
    float range(const float low, const float high) { return low + f32() * (high - low); }

    /** Get random double in range [low, high]. */
    double range(const double low, const double high) { return low + f64() * (high - low); }

    /** Get random float with normal distribution. */
    float normal_distributed(float mean, float deviation);

    /** Get random float with normal distribution. */
    double normal_distributed(double mean, double deviation);

    /** Generate a uniformly distributed number such that 0 <= result < bound. */
    uint32_t bounded_u32(const uint32_t bound)
    {
        // This function's code, including comments, is borrowed and adapted from:
        // https://github.com/imneme/pcg-c-basic/blob/master/pcg_basic.c
        //
        // Note that (as of writing) Mg Engine does not use PCG, but to my understanding, this
        // technique applies as well to other generators.
        //
        // Original license info:
        /*
         * PCG Random Number Generation for C.
         *
         * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
         *
         * Licensed under the Apache License, Version 2.0 (the "License");
         * you may not use this file except in compliance with the License.
         * You may obtain a copy of the License at
         *
         *     http://www.apache.org/licenses/LICENSE-2.0
         *
         * Unless required by applicable law or agreed to in writing, software
         * distributed under the License is distributed on an "AS IS" BASIS,
         * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
         * See the License for the specific language governing permissions and
         * limitations under the License.
         *
         * For additional information about the PCG random number generation scheme,
         * including its license and other licensing options, visit
         *
         *       http://www.pcg-random.org
         */

        // To avoid bias, we need to make the range of the RNG a multiple of
        // bound, which we do by dropping output less than a threshold.
        // A naive scheme to calculate the threshold would be to do
        //
        //     uint32_t threshold = 0x100000000ull % bound;
        //
        // but 64-bit div/mod is slower than 32-bit div/mod (especially on
        // 32-bit platforms).  In essence, we do
        //
        //     uint32_t threshold = (0x100000000ull-bound) % bound;
        //
        // because this version will calculate the same modulus, but the LHS
        // value is less than 2^32.
        const uint32_t threshold = -bound % bound;

        // Uniformity guarantees that this loop will terminate.  In practice, it
        // should usually terminate quickly; on average (assuming all bounds are
        // equally likely), 82.25% of the time, we can expect it to require just
        // one iteration.  In the worst case, someone passes a bound of 2^31 + 1
        // (i.e., 2147483649), which invalidates almost 50% of the range.  In
        // practice, bounds are typically small and only a tiny amount of the range
        // is eliminated.
        for (;;) {
            const auto r = static_cast<uint32_t>(next());
            if (r >= threshold) return r % bound;
        }
    }

private:
    // Get next random value and update state.
    uint64_t next()
    {
        // Pseudo-random number generation using the SplitMix64 generator.
        // Adapted from https://xoshiro.di.unimi.it/splitmix64.c
        // Original license:
        /*  Written in 2015 by Sebastiano Vigna (vigna@acm.org)
            To the extent possible under law, the author has dedicated all copyright
            and related and neighboring rights to this software to the public domain
            worldwide. This software is distributed without any warranty.
            See <http://creativecommons.org/publicdomain/zero/1.0/>.
        */

        uint64_t z = (m_state += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }

    uint64_t m_state;
};

} // namespace Mg
