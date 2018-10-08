// Tests borrowed and adapted from gsl-lite, licensed as follows:

//
// gsl-lite is based on GSL: Guideline Support Library.
// For more information see https://github.com/martinmoene/gsl-lite
//
// Copyright (c) 2015 Martin Moene
// Copyright (c) 2015 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "catch.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <type_traits>
#include <vector>

#define MG_CONTRACT_VIOLATION_THROWS 1
#include <mg/utils/mg_gsl.h>

inline bool is_little_endian()
{
    int n = 1;
    return *reinterpret_cast<char*>(&n) == 1;
}

// TODO add tests for deduction guides

namespace std {
template<typename T, std::size_t N>
inline std::ostream& operator<<(std::ostream& os, std::array<T, N> const& a)
{
    os << "[std::array[" << N;
    std::copy(a.begin(), a.end(), std::ostream_iterator<T>(os, ","));
    return os << "]";
}

inline std::ostream& operator<<(std::ostream& os, std::byte b)
{
    return os << std::hex << char(b);
}
} // namespace std

namespace Mg::gsl {
template<typename T> inline std::ostream& operator<<(std::ostream& os, span<T> s)
{
    os << "[";
    std::copy(s.begin(), s.end(), std::ostream_iterator<T>(os, ","));
    return os << "]";
}
} // namespace Mg::gsl

using namespace Mg::gsl;

static_assert(std::is_nothrow_move_constructible_v<span<int>>);
static_assert(std::is_nothrow_copy_constructible_v<span<int>>);
static_assert(std::is_nothrow_move_assignable_v<span<int>>);
static_assert(std::is_nothrow_copy_assignable_v<span<int>>);

TEST_CASE("Disallows construction from a temporary value")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    span<int> v = 42;
#endif
}

TEST_CASE("Disallows construction from a C array of incompatible type")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    short arr[] = {
        1,
        2,
        3,
    };
    span<int> v = arr;
#endif
}

TEST_CASE("Disallows construction from a std array of incompatible type")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    std::array<long, 3> arr = { {
        1L,
        2L,
        3L,
    } };
    span<int>           v(arr);
#endif
}

TEST_CASE("Disallows construction from xvalue container")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    span<const int> v(std::vector<int>({ 1, 2, 3 }));
#endif
}

TEST_CASE("Disallows slicing construction from container")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    struct A {
        int a;
        A(int _a) : a(_a) {}
    };
    struct B : A {
        int b;
        B(int _a, int _b) : A(_a), b(_b) {}
    };
    auto    vec = std::vector<B>({ { 1, 2 }, { 3, 4 } });
    span<A> v(vec);
#endif
}

TEST_CASE("Disallows slicing construction from span")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    struct A {
        int a;
        A(int _a) : a(_a) {}
    };
    struct B : A {
        int b;
        B(int _a, int _b) : A(_a), b(_b) {}
    };
    span<B> v1;
    span<A> v2(v1);
#endif
}

TEST_CASE("Disallows slicing construction from ptr")
{
#if gsl_CONFIG_CONFIRMS_COMPILATION_ERRORS
    struct A {
        int a;
        A(int _a) : a(_a) {}
    };
    struct B : A {
        int b;
        B(int _a, int _b) : A(_a), b(_b) {}
    };
    B       b(1, 2);
    span<A> v(&b, 1);
#endif
}

TEST_CASE("Terminates construction from a nullptr and a non zero size")
{
    struct F {
        static void blow() { span<int> v(nullptr, 42); }
    };

    REQUIRE_THROWS_AS(F::blow(), Mg::ContractViolation);
}

TEST_CASE("Terminates construction from two pointers in the wrong order")
{
    struct F {
        static void blow()
        {
            int       a[2];
            span<int> v(&a[1], &a[0]);
        }
    };

    REQUIRE_THROWS_AS(F::blow(), Mg::ContractViolation);
}

TEST_CASE("Terminates construction from a null pointer and a non zero size")
{
    struct F {
        static void blow()
        {
            int*      p = nullptr;
            span<int> v(p, 42);
        }
    };

    REQUIRE_THROWS_AS(F::blow(), Mg::ContractViolation);
}

TEST_CASE("Terminates creation of a subspan of the first n elements for n exceeding the span")
{
    struct F {
        static void blow()
        {
            int arr[] = {
                1,
                2,
                3,
            };
            span<int> v(arr);

            (void)v.first(4);
        }
    };

    REQUIRE_THROWS_AS(F::blow(), Mg::ContractViolation);
}

TEST_CASE("Terminates creation of a subspan of the last n elements for n exceeding the span")
{
    struct F {
        static void blow()
        {
            int arr[] = {
                1,
                2,
                3,
            };
            span<int> v(arr);

            (void)v.last(4);
        }
    };

    REQUIRE_THROWS_AS(F::blow(), Mg::ContractViolation);
}

TEST_CASE("Terminates creation of a subspan outside the span")
{
    struct F {
        static void blow_offset()
        {
            int arr[] = {
                1,
                2,
                3,
            };
            span<int> v(arr);

            (void)v.subspan(4);
        }
        static void blow_count()
        {
            int arr[] = {
                1,
                2,
                3,
            };
            span<int> v(arr);

            (void)v.subspan(1, 3);
        }
    };

    REQUIRE_THROWS_AS(F::blow_offset(), Mg::ContractViolation);
    REQUIRE_THROWS_AS(F::blow_count(), Mg::ContractViolation);
}

TEST_CASE("Terminates access outside the span")
{
    struct F {
        static void blow_ix(size_t i)
        {
            int arr[] = {
                1,
                2,
                3,
            };
            span<int> v(arr);
            (void)v[i];
        }
    };

    F::blow_ix(2);
    REQUIRE_THROWS_AS(F::blow_ix(3), Mg::ContractViolation);
}

TEST_CASE("Allows to default construct")
{
    span<int> v;

    REQUIRE(v.size() == size_t(0));
    // Can not use nullptr due to standard library defect (ambiguous overload for operator<<)
    REQUIRE(v.begin() == static_cast<void*>(nullptr));
    REQUIRE(v.end() == static_cast<void*>(nullptr));
}

TEST_CASE("Allows to construct from a nullptr and a zero size")
{
    span<int>       v(nullptr, 0);
    span<const int> w(nullptr, 0);

    REQUIRE(v.size() == size_t(0));
    REQUIRE(w.size() == size_t(0));
    // Can not use nullptr due to standard library defect (ambiguous overload for operator<<)
    REQUIRE(v.begin() == static_cast<void*>(nullptr));
    REQUIRE(v.end() == static_cast<void*>(nullptr));
}

TEST_CASE("Allows to construct from two pointers")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<int>       v(arr, arr + (sizeof(arr) / sizeof(arr[0])));
    span<const int> w(arr, arr + (sizeof(arr) / sizeof(arr[0])));

    REQUIRE(std::equal(v.begin(), v.end(), arr));
    REQUIRE(std::equal(w.begin(), w.end(), arr));
}

TEST_CASE("Allows to construct from two pointers to const")
{
    const int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<const int> v(arr, arr + (sizeof(arr) / sizeof(arr[0])));

    REQUIRE(std::equal(v.begin(), v.end(), arr));
}

TEST_CASE("Allows to construct from a non null pointer and a size")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<int>       v(arr, (sizeof(arr) / sizeof(arr[0])));
    span<const int> w(arr, (sizeof(arr) / sizeof(arr[0])));

    REQUIRE(std::equal(v.begin(), v.end(), arr));
    REQUIRE(std::equal(w.begin(), w.end(), arr));
}

TEST_CASE("Allows to construct from a non null pointer to const and a size")
{
    const int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<const int> v(arr, (sizeof(arr) / sizeof(arr[0])));

    REQUIRE(std::equal(v.begin(), v.end(), arr));
}

TEST_CASE("Allows to construct from a temporary pointer and a size")
{
    int x = 42;

    span<int>       v(&x, 1);
    span<const int> w(&x, 1);

    REQUIRE(std::equal(v.begin(), v.end(), &x));
    REQUIRE(std::equal(w.begin(), w.end(), &x));
}

TEST_CASE("Allows to construct from a temporary pointer to const and a size")
{
    const int x = 42;

    span<const int> v(&x, 1);

    REQUIRE(std::equal(v.begin(), v.end(), &x));
}

TEST_CASE("Allows to construct from any pointer and a zero size")
{
    struct F {
        static void null()
        {
            int*      p = nullptr;
            span<int> v(p, size_t(0));
        }
        static void nonnull()
        {
            int       i = 7;
            int*      p = &i;
            span<int> v(p, size_t(0));
        }
    };

    F::null();
    F::nonnull();
}

TEST_CASE("Allows to construct from a C array")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<int>       v(arr);
    span<const int> w(arr);

    REQUIRE(std::equal(v.begin(), v.end(), arr));
    REQUIRE(std::equal(w.begin(), w.end(), arr));
}

TEST_CASE("Allows to construct from a const C array")
{
    const int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<const int> v(arr);

    REQUIRE(std::equal(v.begin(), v.end(), arr));
}

TEST_CASE(
    "Allows to construct from a C array with size via decay to pointer (potentially dangerous)")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    {
        span<int>       v(arr, (sizeof(arr) / sizeof(arr[0])));
        span<const int> w(arr, (sizeof(arr) / sizeof(arr[0])));

        REQUIRE(std::equal(v.begin(), v.end(), arr));
        REQUIRE(std::equal(w.begin(), w.end(), arr));
    }

    {
        span<int>       v(arr, 3);
        span<const int> w(arr, 3);

        REQUIRE(std::equal(v.begin(), v.end(), arr, arr + 3));
        REQUIRE(std::equal(w.begin(), w.end(), arr, arr + 3));
    }
}

TEST_CASE(
    "Allows to construct from a const C array with size via decay to pointer (potentially "
    "dangerous)")
{
    const int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    {
        span<const int> v(arr, (sizeof(arr) / sizeof(arr[0])));
        REQUIRE(std::equal(v.begin(), v.end(), arr));
    }

    {
        span<const int> w(arr, 3);
        REQUIRE(std::equal(w.begin(), w.end(), arr, arr + 3));
    }
}

TEST_CASE("Allows to construct from a std array")
{
    std::array<int, 9> arr = { {
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
    } };

    span<int>       v(arr);
    span<const int> w(arr);

    REQUIRE(std::equal(v.begin(), v.end(), arr.begin()));
    REQUIRE(std::equal(w.begin(), w.end(), arr.begin()));
}

TEST_CASE("Allows to construct from a std array with const data")
{
    std::array<const int, 9> arr = { {
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
    } };

    span<const int> v(arr);
    REQUIRE(std::equal(v.begin(), v.end(), arr.begin()));
}

TEST_CASE("Allows to construct from a std vector")
{
    std::vector<int> vec = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<int>       v(vec);
    span<const int> w(vec);

    REQUIRE(std::equal(v.begin(), v.end(), vec.begin()));
    REQUIRE(std::equal(w.begin(), w.end(), vec.begin()));
}

TEST_CASE("Allows to construct from a const std vector")
{
    const auto      vec = std::vector<int>({ 1, 2, 3, 4, 5, 6, 7, 8, 9 });
    span<const int> v(vec);
}

TEST_CASE("Allows to copy construct from another of the same type")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    span<int>       v(arr);
    span<const int> w(arr);

    span<int>       x(v);
    span<const int> y(w);

    REQUIRE(std::equal(x.begin(), x.end(), arr));
    REQUIRE(std::equal(y.begin(), y.end(), arr));
}

TEST_CASE("Allows to copy construct from another of a compatible type")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    span<int>       v(arr);
    span<const int> w(arr);

    span<const volatile int> x(v);
    span<const volatile int> y(v);

    REQUIRE(std::equal(x.begin(), x.end(), arr));
    REQUIRE(std::equal(y.begin(), y.end(), arr));
}

TEST_CASE("Allows to move construct from another of the same type")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };

    span<int> v((span<int>(arr)));
    //  span<      int> w(( span<const int>( arr ) ));
    span<const int> x((span<int>(arr)));
    span<const int> y((span<const int>(arr)));

    REQUIRE(std::equal(v.begin(), v.end(), arr));
    REQUIRE(std::equal(x.begin(), x.end(), arr));
    REQUIRE(std::equal(y.begin(), y.end(), arr));
}

TEST_CASE("Allows to copy assign from another of the same type")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    span<int>       v(arr);
    span<int>       s;
    span<const int> t;

    s = v;
    t = v;

    REQUIRE(std::equal(s.begin(), s.end(), arr));
    REQUIRE(std::equal(t.begin(), t.end(), arr));
}

TEST_CASE("Allows to move assign from another of the same type")
{
    int arr[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9,
    };
    span<int>       v;
    span<const int> w;

    v = span<int>(arr);
    w = span<int>(arr);

    REQUIRE(std::equal(v.begin(), v.end(), arr));
    REQUIRE(std::equal(w.begin(), w.end(), arr));
}

TEST_CASE("Allows to create a subspan of the first n elements")
{
    int arr[] = {
        1, 2, 3, 4, 5,
    };
    span<int> v(arr);
    size_t    count = 3;

    span<int>       s = v.first(count);
    span<const int> t = v.first(count);

    REQUIRE(s.size() == count);
    REQUIRE(t.size() == count);
    REQUIRE(std::equal(s.begin(), s.end(), arr));
    REQUIRE(std::equal(t.begin(), t.end(), arr));
}

TEST_CASE("Allows to create a subspan of the last n elements")
{
    int arr[] = {
        1, 2, 3, 4, 5,
    };
    span<int> v(arr);
    size_t    count = 3;

    span<int>       s = v.last(count);
    span<const int> t = v.last(count);

    REQUIRE(s.size() == count);
    REQUIRE(t.size() == count);
    REQUIRE(std::equal(s.begin(), s.end(), arr + v.size() - count));
    REQUIRE(std::equal(t.begin(), t.end(), arr + v.size() - count));
}

TEST_CASE("Allows to create a subspan starting at a given offset")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);
    size_t    offset = 1;

    span<int>       s = v.subspan(offset);
    span<const int> t = v.subspan(offset);

    REQUIRE(s.size() == v.size() - offset);
    REQUIRE(t.size() == v.size() - offset);
    REQUIRE(std::equal(s.begin(), s.end(), arr + offset));
    REQUIRE(std::equal(t.begin(), t.end(), arr + offset));
}

TEST_CASE("Allows to create a subspan starting at a given offset with a given length")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);
    size_t    offset = 1;
    size_t    length = 1;

    span<int>       s = v.subspan(offset, length);
    span<const int> t = v.subspan(offset, length);

    REQUIRE(s.size() == length);
    REQUIRE(t.size() == length);
    REQUIRE(std::equal(s.begin(), s.end(), arr + offset));
    REQUIRE(std::equal(t.begin(), t.end(), arr + offset));
}

TEST_CASE("Allows to create an empty subspan at full offset")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);
    size_t    offset = v.size();

    span<int>       s = v.subspan(offset);
    span<const int> t = v.subspan(offset);

    REQUIRE(s.empty());
    REQUIRE(t.empty());
}

TEST_CASE("Allows to create an empty subspan at full offset with zero length")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);
    size_t    offset = v.size();
    size_t    length = 0;

    span<int>       s = v.subspan(offset, length);
    span<const int> t = v.subspan(offset, length);

    REQUIRE(s.empty());
    REQUIRE(t.empty());
}

TEST_CASE("Allows forward iteration")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);

    for (span<int>::iterator pos = v.begin(); pos != v.end(); ++pos) {
        REQUIRE(*pos == arr[std::distance(v.begin(), pos)]);
    }
}

TEST_CASE("Allows const forward iteration")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int> v(arr);

    for (span<int>::const_iterator pos = v.cbegin(); pos != v.cend(); ++pos) {
        REQUIRE(*pos == arr[std::distance(v.cbegin(), pos)]);
    }
}

TEST_CASE("Allows to observe an element via array indexing")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int>       v(arr);
    span<int> const w(arr);

    for (size_t i = 0; i < v.size(); ++i) {
        REQUIRE(v[i] == arr[i]);
        REQUIRE(w[i] == arr[i]);
    }
}

TEST_CASE("Allows to observe an element via data")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int>       v(arr);
    span<int> const w(arr);

    REQUIRE(*v.data() == *v.begin());
    REQUIRE(*w.data() == *v.begin());

    for (size_t i = 0; i < v.size(); ++i) {
        REQUIRE(v.data()[i] == arr[i]);
        REQUIRE(w.data()[i] == arr[i]);
    }
}

TEST_CASE("Allows to change an element via array indexing")
{
    int arr[] = {
        1,
        2,
        3,
    };
    span<int>       v(arr);
    span<int> const w(arr);

    v[1] = 22;
    w[2] = 33;

    REQUIRE(22 == arr[1]);
    REQUIRE(33 == arr[2]);
}

#if 0 // not implemented in Mg::gsl::span
    TEST_CASE("Allows to change an element via call indexing")
    {
        int arr[] = {
            1, 2, 3,
        };
        span<int>       v(arr);
        span<int> const w(arr);

        v(1) = 22;
        w(2) = 33;

        REQUIRE(22 == arr[1]);
        REQUIRE(33 == arr[2]);
    }
#endif

TEST_CASE("Allows to change an element via data")
{
    int arr[] = {
        1,
        2,
        3,
    };

    span<int>       v(arr);
    span<int> const w(arr);

    *v.data() = 22;
    REQUIRE(22 == *v.data());

    *w.data() = 33;
    REQUIRE(33 == *w.data());
}

TEST_CASE("Allows to test for empty via empty empty case")
{
    span<int> v;

    REQUIRE(v.empty());
}

TEST_CASE("Allows to test for empty via empty non empty case")
{
    int       a[] = { 1 };
    span<int> v(a);

    REQUIRE(!(v.empty()));
}

TEST_CASE("Allows to obtain the number of elements via size")
{
    int a[] = {
        1,
        2,
        3,
    };
    int b[] = {
        1, 2, 3, 4, 5,
    };

    span<int> z;
    span<int> va(a);
    span<int> vb(b);

    REQUIRE(va.size() == size_t((sizeof(a) / sizeof(a[0]))));
    REQUIRE(vb.size() == size_t((sizeof(b) / sizeof(b[0]))));
    REQUIRE(z.size() == size_t(0));
}

TEST_CASE("Allows to obtain the number of elements via length")
{
    int a[] = {
        1,
        2,
        3,
    };
    int b[] = {
        1, 2, 3, 4, 5,
    };

    span<int> z;
    span<int> va(a);
    span<int> vb(b);

    REQUIRE(va.length() == size_t((sizeof(a) / sizeof(a[0]))));
    REQUIRE(vb.length() == size_t((sizeof(b) / sizeof(b[0]))));
    REQUIRE(z.length() == size_t(0));
}

TEST_CASE("Allows to obtain the number of bytes via size bytes")
{
    int a[] = {
        1,
        2,
        3,
    };
    int b[] = {
        1, 2, 3, 4, 5,
    };

    span<int> z;
    span<int> va(a);
    span<int> vb(b);

    REQUIRE(va.size_bytes() == size_t((sizeof(a) / sizeof(a[0])) * sizeof(int)));
    REQUIRE(vb.size_bytes() == size_t((sizeof(b) / sizeof(b[0])) * sizeof(int)));
    REQUIRE(z.size_bytes() == size_t(0 * sizeof(int)));
}

TEST_CASE("Allows to obtain the number of bytes via length bytes")
{
    int a[] = {
        1,
        2,
        3,
    };
    int b[] = {
        1, 2, 3, 4, 5,
    };

    span<int> z;
    span<int> va(a);
    span<int> vb(b);

    REQUIRE(va.length_bytes() == size_t((sizeof(a) / sizeof(a[0])) * sizeof(int)));
    REQUIRE(vb.length_bytes() == size_t((sizeof(b) / sizeof(b[0])) * sizeof(int)));
    REQUIRE(z.length_bytes() == size_t(0 * sizeof(int)));
}

TEST_CASE("Allows to view the elements as read only bytes")
{
    using type = int32_t;

    REQUIRE(sizeof(type) == size_t(4));

    type a[] = {
        0x12345678,
    };
    std::byte be[] = {
        std::byte(0x12),
        std::byte(0x34),
        std::byte(0x56),
        std::byte(0x78),
    };
    std::byte le[] = {
        std::byte(0x78),
        std::byte(0x56),
        std::byte(0x34),
        std::byte(0x12),
    };

    std::byte* b = is_little_endian() ? le : be;

    span<int>             va(a);
    span<const std::byte> vb(va.as_bytes());
    span<const std::byte> vc(as_bytes(va));

    REQUIRE(vb[0] == b[0]);
    REQUIRE(vc[0] == b[0]);
    REQUIRE(vb[1] == b[1]);
    REQUIRE(vc[1] == b[1]);
    REQUIRE(vb[2] == b[2]);
    REQUIRE(vc[2] == b[2]);
    REQUIRE(vb[3] == b[3]);
    REQUIRE(vc[3] == b[3]);
}

TEST_CASE("Allows to copy a span to another span of the same element type")
{
    int a[] = {
        1,
        2,
        3,
    };
    int b[] = {
        0, 0, 0, 0, 0,
    };

    span<int> src(a);
    span<int> dst(b);

    std::copy(src.begin(), src.end(), dst.begin());

    span<int> cmp_span = dst.subspan(0, src.size());
    REQUIRE(std::equal(src.begin(), src.end(), cmp_span.begin()));
}

TEST_CASE("Allows to copy a span to another span of a different element type")
{
    char a[] = {
        'a',
        'b',
        'c',
    };
    int b[] = {
        0, 0, 0, 0, 0,
    };

    span<char> src(a);
    span<int>  dst(b);

    std::copy(src.begin(), src.end(), dst.begin());

    for (span<int>::size_type i = 0; i < src.size(); ++i) { REQUIRE(src[i] == dst[i]); }
}

#if (MG_HAVE_CLASS_TEMPLATE_DEDUCTION)

TEST_CASE("Deduces from pointer pair")
{
    std::array<int, 2> a{ { 0, 1 } };

    auto begin = &a[0];
    auto end   = begin + a.size();
    span v(begin, end);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from const pointer pair")
{
    std::array<const int, 2> a{ { 0, 1 } };

    auto begin = &a[0];
    auto end   = begin + a.size();
    span v(begin, end);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from pointer and length")
{
    std::array<int, 2> a{ { 0, 1 } };

    auto begin = &a[0];
    auto end   = begin + a.size();
    span v(begin, end);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from const pointer and length")
{
    std::array<const int, 2> a{ { 0, 1 } };

    auto begin = &a[0];
    auto end   = begin + a.size();
    span v(begin, end);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from other span")
{
    int       i;
    span<int> vi(&i, 1);
    span      v(vi);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from other const span")
{
    int             i;
    span<const int> vi(&i, 1);
    span            v(vi);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from array")
{
    int  a[] = { 1, 2, 3, 4 };
    span v(a);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from const array")
{
    const int a[] = { 1, 2, 3, 4 };
    span      v(a);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from std array")
{
    std::array<int, 4> a{ { 0, 1, 2, 3 } };
    span               v(a);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from const std array")
{
    const std::array<int, 4> a{ { 0, 1, 2, 3 } };
    span                     v(a);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from std array of const")
{
    std::array<const int, 4> a{ { 0, 1, 2, 3 } };
    span                     v(a);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

TEST_CASE("Deduces from std vector")
{
    std::vector<int> vec({ 1, 2, 3, 4, 5 });
    span             v(vec);
    REQUIRE((std::is_same_v<span<int>, decltype(v)>));
}

TEST_CASE("Deduces from const std vector")
{
    const std::vector<int> vec({ 1, 2, 3, 4, 5 });
    span                   v(vec);
    REQUIRE((std::is_same_v<span<const int>, decltype(v)>));
}

#endif // (MG_HAVE_CLASS_TEMPLATE_DEDUCTION)
