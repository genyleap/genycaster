#include "../../ui/app/social/viewport_work_budget.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <random>
#include <vector>

int main()
{
    using genycaster::ui::detail::evictDepartedRanges;
    std::mt19937 random{0x6e79u};
    for (int iteration = 0; iteration < 20000; ++iteration) {
        const int count = static_cast<int>(random() % 256);
        std::vector<int> items(count);
        std::iota(items.begin(), items.end(), 0);
        std::vector<bool> desired(count);
        for (int i = 0; i < count; ++i) desired[i] = (random() % 3) == 0;
        std::vector<int> expected;
        for (int i = 0; i < count; ++i) if (desired[i]) expected.push_back(i);
        const int budget = static_cast<int>(random() % 9);
        int passes = 0;
        while (items != expected) {
            const auto before = items.size();
            const int removed = evictDepartedRanges(static_cast<int>(items.size()), budget,
                [&](int index) { return desired[items.at(index)]; },
                [&](int first, int last) {
                    assert(first >= 0 && first <= last && last < static_cast<int>(items.size()));
                    for (int i = first; i <= last; ++i) assert(!desired[items[i]]);
                    items.erase(items.begin() + first, items.begin() + last + 1);
                });
            assert(removed > 0 && before - items.size() == static_cast<unsigned>(removed));
            assert(budget == 0 || removed <= budget);
            assert(std::is_sorted(items.begin(), items.end()));
            assert(++passes <= count);
        }
        assert(evictDepartedRanges(static_cast<int>(items.size()), budget,
            [](int) { return true; }, [](int, int) { assert(false); }) == 0);
    }
}
