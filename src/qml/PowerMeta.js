.pragma library

function buffName(type) {
    if (type === 1) return "GHOST"
    if (type === 2) return "SLOW"
    if (type === 3) return "MAGNET"
    if (type === 4) return "SHIELD"
    if (type === 5) return "PORTAL"
    if (type === 6) return "DOUBLE"
    if (type === 7) return "DIAMOND"
    if (type === 8) return "LASER"
    if (type === 9) return "MINI"
    return "NONE"
}

function powerGlyph(type) {
    if (type === 1) return "G"
    if (type === 2) return "S"
    if (type === 3) return "M"
    if (type === 4) return "H"
    if (type === 5) return "P"
    if (type === 6) return "2"
    if (type === 7) return "D"
    if (type === 8) return "L"
    if (type === 9) return "m"
    return "?"
}

function choiceGlyph(type) {
    if (type === 1) return "G"
    if (type === 2) return "S"
    if (type === 3) return "M"
    if (type === 4) return "H"
    if (type === 5) return "P"
    if (type === 6) return "2x"
    if (type === 7) return "3x"
    if (type === 8) return "L"
    if (type === 9) return "m"
    return "?"
}

function choiceName(type) {
    if (type === 1) return "Ghost"
    if (type === 2) return "Slow"
    if (type === 3) return "Magnet"
    if (type === 4) return "Shield"
    if (type === 5) return "Portal"
    if (type === 6) return "Double"
    if (type === 7) return "Diamond"
    if (type === 8) return "Laser"
    if (type === 9) return "Mini"
    return "Unknown"
}

function choiceDescription(type) {
    if (type === 1) return "Pass through self"
    if (type === 2) return "Drop speed by 1 tier"
    if (type === 3) return "Attract food"
    if (type === 4) return "One extra life"
    if (type === 5) return "Phase through walls"
    if (type === 6) return "Double points"
    if (type === 7) return "Triple points"
    if (type === 8) return "Break obstacle"
    if (type === 9) return "Shrink body"
    return "Debug preview"
}

function choiceSpec(type) {
    return {
        type: type,
        name: choiceName(type),
        description: choiceDescription(type)
    }
}

function rarityTier(type) {
    if (type === 7) return 4
    if (type === 6 || type === 8) return 3
    if (type === 4 || type === 5) return 2
    return 1
}

function rarityName(type) {
    const tier = rarityTier(type)
    if (tier === 4) return "EPIC"
    if (tier === 3) return "RARE"
    if (tier === 2) return "UNCOMMON"
    return "COMMON"
}
