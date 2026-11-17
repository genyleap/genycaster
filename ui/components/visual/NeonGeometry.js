.pragma library

// Arc-length exact projection and perimeter traversal from the user-supplied
// Genyleap Neon UI Kit v0.4.5. Geometry is shared between Halo instances.
// No QML object captures or delayed callbacks live in this library.
function nearestBorderPoint(px, py, width, height, radius, power,
                            topLeft = true, topRight = true,
                            bottomRight = true, bottomLeft = true) {
    const w = Math.max(0, width)
    const h = Math.max(0, height)
    if (w <= 0 || h <= 0 || !Number.isFinite(px) || !Number.isFinite(py))
        return { x: w * 0.5, y: h * 0.5, distance: Infinity }

    const r = Math.min(Math.max(0, radius), w * 0.5, h * 0.5)
    const exponent = Math.max(2, power)
    const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v))
    let nearestX = 0
    let nearestY = 0
    let bestDistanceSq = Infinity
    function consider(x, y) {
        const dx = px - x
        const dy = py - y
        const distanceSq = dx * dx + dy * dy
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq
            nearestX = x
            nearestY = y
        }
        return distanceSq
    }
    const radii = [topLeft ? r : 0, topRight ? r : 0,
                   bottomRight ? r : 0, bottomLeft ? r : 0]
    consider(clamp(px, radii[0], w - radii[1]), 0)
    consider(w, clamp(py, radii[1], h - radii[2]))
    consider(clamp(px, radii[3], w - radii[2]), h)
    consider(0, clamp(py, radii[0], h - radii[3]))
    if (r > 0) {
        const corners = [
            [r, r, -1, -1, topLeft], [w - r, r, 1, -1, topRight],
            [w - r, h - r, 1, 1, bottomRight],
            [r, h - r, -1, 1, bottomLeft]
        ]
        const fraction = (Math.sqrt(5) - 1) * 0.5
        for (const corner of corners) {
            if (!corner[4]) continue
            const cx = corner[0]
            const cy = corner[1]
            const sx = corner[2]
            const sy = corner[3]
            const dx = px - clamp(px, cx + Math.min(0, sx * r), cx + Math.max(0, sx * r))
            const dy = py - clamp(py, cy + Math.min(0, sy * r), cy + Math.max(0, sy * r))
            if (dx * dx + dy * dy >= bestDistanceSq)
                continue
            function cornerDistanceSq(parameter) {
                const point = unitCorner(parameter, exponent)
                const x = cx + sx * r * point.x
                const y = cy - sy * r * point.y
                const ddx = px - x
                const ddy = py - y
                return ddx * ddx + ddy * ddy
            }
            let lower = 0
            let upper = 1
            let first = upper - fraction * (upper - lower)
            let second = lower + fraction * (upper - lower)
            let firstDistance = cornerDistanceSq(first)
            let secondDistance = cornerDistanceSq(second)
            for (let step = 0; step < 18; ++step) {
                if (firstDistance > secondDistance) {
                    lower = first
                    first = second
                    firstDistance = secondDistance
                    second = lower + fraction * (upper - lower)
                    secondDistance = cornerDistanceSq(second)
                } else {
                    upper = second
                    second = first
                    secondDistance = firstDistance
                    first = upper - fraction * (upper - lower)
                    firstDistance = cornerDistanceSq(first)
                }
            }
            const point = unitCorner((lower + upper) * 0.5, exponent)
            consider(cx + sx * r * point.x, cy - sy * r * point.y)
        }
    }
    return { x: nearestX, y: nearestY, distance: Math.sqrt(bestDistanceSq) }
}

// Cache dimensionless Lamé arcs; shape resizes only change their scale.
var cornerTables = ({})
function unitCorner(parameter, power) {
    const diagonal = Math.pow(0.5, 1 / power)
    if (parameter <= 0.5) {
        const x = parameter * 2 * diagonal
        return { x: x, y: -Math.pow(Math.max(0, 1 - Math.pow(x, power)), 1 / power) }
    }
    const y = (1 - parameter) * 2 * diagonal
    return { x: Math.pow(Math.max(0, 1 - Math.pow(y, power)), 1 / power), y: -y }
}
function cornerTable(power) {
    const exponent = Math.max(2, power)
    if (cornerTables[exponent])
        return cornerTables[exponent]
    const lengths = [0]
    let previous = unitCorner(0, exponent)
    for (let index = 1; index <= 256; ++index) {
        const point = unitCorner(index / 256, exponent)
        lengths.push(lengths[index - 1] + Math.hypot(point.x - previous.x,
                                                     point.y - previous.y))
        previous = point
    }
    const table = { lengths: lengths, length: lengths[256], power: exponent }
    cornerTables[exponent] = table
    return table
}
function borderLength(width, height, radius, power) {
    const r = Math.min(Math.max(0, radius), Math.max(0, width) / 2, Math.max(0, height) / 2)
    return 2 * Math.max(0, width - 2 * r) + 2 * Math.max(0, height - 2 * r)
         + 4 * r * cornerTable(power).length
}
function borderPoint(progress, width, height, radius, power) {
    const w = Math.max(0, width)
    const h = Math.max(0, height)
    if (w <= 0 || h <= 0 || !Number.isFinite(progress))
        return { x: w / 2, y: h / 2 }
    const r = Math.min(Math.max(0, radius), w / 2, h / 2)
    const table = cornerTable(power)
    const arcLength = r * table.length
    const horizontal = w - 2 * r
    const vertical = h - 2 * r
    const perimeter = 2 * horizontal + 2 * vertical + 4 * arcLength
    let distance = (((progress % 1) + 1) % 1 * perimeter + horizontal / 2) % perimeter
    const segments = [horizontal, arcLength, vertical, arcLength,
                      horizontal, arcLength, vertical, arcLength]
    for (let segment = 0; segment < 8; ++segment) {
        const length = segments[segment]
        if (distance > length || length === 0) {
            distance -= length
            continue
        }
        if (segment === 0) return { x: r + distance, y: 0 }
        if (segment === 2) return { x: w, y: r + distance }
        if (segment === 4) return { x: w - r - distance, y: h }
        if (segment === 6) return { x: 0, y: h - r - distance }
        const normalized = distance / r
        let lower = 0
        let upper = 256
        while (upper - lower > 1) {
            const middle = Math.floor((lower + upper) / 2)
            if (table.lengths[middle] < normalized) lower = middle
            else upper = middle
        }
        const fraction = (normalized - table.lengths[lower])
                       / (table.lengths[upper] - table.lengths[lower])
        const point = unitCorner((lower + fraction) / 256, table.power)
        if (segment === 1) return { x: w - r + r * point.x, y: r + r * point.y }
        if (segment === 3) return { x: w - r - r * point.y, y: h - r + r * point.x }
        if (segment === 5) return { x: r - r * point.x, y: h - r - r * point.y }
        return { x: r + r * point.y, y: r - r * point.x }
    }
    return { x: w / 2, y: 0 }
}

// Reactive mapping between sibling UI panes in the same untransformed scene.
// Bind x/y/parent through this function; no per-frame polling is required.
// Transformed callers should provide an explicit source-local rectangle.
function translatedRect(item, source) {
    if (!item) return Qt.rect(0, 0, 0, 0)
    if (!source) return Qt.rect(0, 0, item.width, item.height)
    let x = 0
    let y = 0
    let current = item
    while (current) {
        x += current.x
        y += current.y
        current = current.parent
    }
    current = source
    while (current) {
        x -= current.x
        y -= current.y
        current = current.parent
    }
    return Qt.rect(x, y, item.width, item.height)
}
