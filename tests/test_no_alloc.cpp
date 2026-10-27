// F6: process_frame() must not touch the heap. This executable replaces global operator
// new/delete with counting versions and checks the counter does not move across calls.
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

#include "quietroom/pipeline.hpp"
#include "signals.hpp"

namespace {
std::atomic<std::size_t> g_allocations{0};
}

// The whole family is replaced, including the nothrow forms the standard library uses
// internally: mixing our free() with another allocator's operator new is an ASan error.
void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return operator new(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size, std::nothrow);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

// A volatile sink makes the allocation observable, so the compiler may not elide new/delete.
int* volatile g_sink = nullptr;

TEST(NoAllocation, HookActuallyCountsAllocations) {
    const std::size_t before = g_allocations.load();
    g_sink = new int(1);
    delete g_sink;
    EXPECT_GT(g_allocations.load(), before);
}

TEST(NoAllocation, ProcessFrameDoesNotAllocate) {
    using namespace quietroom;
    const auto clip = testsig::make_speechlike(3.0, 1);
    const auto noisy =
        testsig::mix(clip.samples, testsig::white_noise(clip.samples.size(), 5), 5.0);
    std::vector<Sample> out(noisy.size());
    Pipeline p{Config{}};  // the constructor may allocate; the frame path may not

    const std::size_t before = g_allocations.load();
    for (std::size_t h = 0; h + kHop <= noisy.size(); h += kHop) {
        p.process_frame(&noisy[h], &out[h]);
    }
    EXPECT_EQ(g_allocations.load() - before, 0u);
}
