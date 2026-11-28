#pragma once

namespace genycaster::ui::detail {

// Descending contiguous erasure keeps earlier indexes valid. The predicate
// observes the current sequence; the visitor removes exactly [first,last].
// A nonpositive budget preserves synchronous native callers. No allocation,
// ownership transfer, Qt dependency or clocks on the normal path.
template<class IsDesired, class EraseRange>
[[nodiscard]] constexpr int evictDepartedRanges(
    const int count, const int budget, IsDesired&& desired, EraseRange&& erase)
{
    int removed = 0;
    for (int last = count - 1; last >= 0;) {
        if (desired(last)) {
            --last;
            continue;
        }
        if (budget > 0 && removed >= budget)
            break;
        int first = last;
        while (first > 0 && !desired(first - 1)
               && (budget <= 0 || last - first + 1 < budget - removed))
            --first;
        erase(first, last);
        removed += last - first + 1;
        last = first - 1;
    }
    return removed;
}

} // namespace genycaster::ui::detail
