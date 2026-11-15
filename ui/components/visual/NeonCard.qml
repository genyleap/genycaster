import QtQuick
import Farcaster.Client

// The semantic, Kit-style card surface for onboarding, dashboards and
// inspectors. Decorative Superellipse shapes (icons, dividers, skeletons)
// must NOT be upgraded to Neon; only actual card/panel boundaries use this.
// All card consumers share the same pointer tracking, GPU rim and paint
// transitions through NeonSurface rather than duplicating shader layers.
NeonSurface {
    id: card
    interactive: true
    ambientNeon: true
    fillColor: Theme.surface
    borderWidth: Theme.strokeStandard
    borderColor: Theme.borderMuted
    curveDepth: Theme.curveContainer
    neonStrength: Theme.neonCardStrength
}
