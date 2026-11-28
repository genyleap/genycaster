#include "../../ui/app/social/viewport_work_budget.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

namespace {
void require(bool condition)
{
    if (!condition) {
        std::fputs("Viewport retirement budget invariant failed\n", stderr);
        std::abort();
    }
}
}

int main()
{
    using genycaster::ui::detail::evictDepartedRanges;
    std::mt19937 random{0x6e79u};
    for (int iteration = 0; iteration < 20000; ++iteration) {
        const int count = static_cast<int>(random() % 256);
        const auto size = static_cast<std::size_t>(count);
        std::vector<int> items(size);
        std::iota(items.begin(), items.end(), 0);
        std::vector<bool> desired(size);
        for (std::size_t i = 0; i < size; ++i) desired[i] = (random() % 3) == 0;
        std::vector<int> expected;
        for (std::size_t i = 0; i < size; ++i)
            if (desired[i]) expected.push_back(static_cast<int>(i));
        const int budget = static_cast<int>(random() % 9);
        int passes = 0;
        while (items != expected) {
            const auto before = items.size();
            const int removed = evictDepartedRanges(static_cast<int>(items.size()), budget,
                [&](int index) {
                    return desired.at(static_cast<std::size_t>(
                        items.at(static_cast<std::size_t>(index))));
                },
                [&](int first, int last) {
                    require(first >= 0 && first <= last && last < static_cast<int>(items.size()));
                    for (int i = first; i <= last; ++i)
                        require(!desired.at(static_cast<std::size_t>(
                            items.at(static_cast<std::size_t>(i)))));
                    items.erase(items.begin() + first, items.begin() + last + 1);
                });
            require(removed > 0 && before - items.size() == static_cast<std::size_t>(removed));
            require(budget == 0 || removed <= budget);
            require(std::is_sorted(items.begin(), items.end()));
            require(++passes <= count);
        }
        require(evictDepartedRanges(static_cast<int>(items.size()), budget,
            [](int) { return true; }, [](int, int) { require(false); }) == 0);
    }
}
