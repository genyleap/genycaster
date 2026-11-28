#pragma once

namespace genycaster::ui::detail {

// Descending contiguous erasure keeps earlier indexes valid. The predicate
// observes the current sequence; the visitor removes exactly [first,last].
// A nonpositive budget preserves synchronous native callers. A positive
// rangeLimit allows a cooperative deadline to be checked between individual
// removals. Always make one unit of progress, even if it alone exceeds the
// deadline. The callbacks own any timing policy; no allocation or Qt here.
template<class IsDesired, class EraseRange, class ShouldYield>
[[nodiscard]] constexpr int evictDepartedRanges(
    const int count, const int budget, IsDesired&& desired, EraseRange&& erase,
    const int rangeLimit, ShouldYield&& shouldYield)
{
    int removed = 0;
    for (int last = count - 1; last >= 0;) {
        if (desired(last)) {
            --last;
            continue;
        }
        if ((budget > 0 && removed >= budget) || (removed > 0 && shouldYield()))
            break;
        int first = last;
        while (first > 0 && !desired(first - 1)
               && (budget <= 0 || last - first + 1 < budget - removed)
               && (rangeLimit <= 0 || last - first + 1 < rangeLimit))
            --first;
        erase(first, last);
        removed += last - first + 1;
        last = first - 1;
    }
    return removed;
}

template<class IsDesired, class EraseRange>
[[nodiscard]] constexpr int evictDepartedRanges(
    const int count, const int budget, IsDesired&& desired, EraseRange&& erase)
{
    return evictDepartedRanges(count, budget, desired, erase, 0,
        [] { return false; });
}

} // namespace genycaster::ui::detail
