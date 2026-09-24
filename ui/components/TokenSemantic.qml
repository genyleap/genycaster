pragma Singleton
import QtQuick

QtObject {
    function decoded(value) {
        const text = String(value || "").trim()
        if (text.length === 0)
            return ""
        try {
            return decodeURIComponent(text)
        } catch (error) {
            return text
        }
    }

    function normalizedAddress(value) {
        const match = String(value || "").match(/0x[0-9a-fA-F]{40}/)
        return match && match.length > 0 ? String(match[0]).toLowerCase() : ""
    }

    function networkFromChainId(value) {
        const chainId = String(value || "").trim()
        if (chainId === "8453")
            return "base"
        if (chainId === "1")
            return "eth"
        return ""
    }

    function canonicalLocator(value) {
        const text = decoded(value)
        if (text.length === 0)
            return ""

        let match = text.match(/^\s*(base|eth|ethereum)\s*[:/|]\s*(0x[0-9a-fA-F]{40})\s*$/i)
        if (match && match.length >= 3) {
            const network = String(match[1]).toLowerCase() === "base" ? "base" : "eth"
            return network + ":" + String(match[2]).toLowerCase()
        }

        // Farcaster token tickers use CAIP-19 parent URLs such as
        // eip155:8453/erc20:0x… . Keep this parser deterministic and limited to
        // the EVM networks that Genycaster's market provider supports.
        match = text.match(/^\s*(?:chain:\/\/)?eip155:(\d+)\/erc20:(0x[0-9a-fA-F]{40})(?:$|[/?#])/i)
        if (match && match.length >= 3) {
            const network = networkFromChainId(match[1])
            return network.length > 0 ? network + ":" + String(match[2]).toLowerCase() : ""
        }
        return ""
    }

    function locatorFromUrl(value) {
        const text = decoded(value)
        if (text.length === 0)
            return ""

        const direct = canonicalLocator(text)
        if (direct.length > 0)
            return direct

        // Farcaster publishes token embeds as
        // https://farcaster.xyz/~/c/<chain>:<contract>. Treat that route as a
        // canonical token identity, not as a generic OpenGraph link. This is
        // essential because the same cast can also carry a $TICKER hint; both
        // must collapse to one token card.
        const farcasterToken = text.match(
                    /^https?:\/\/(?:www\.)?farcaster\.xyz\/~\/c\/(base|eth|ethereum):(0x[0-9a-fA-F]{40})(?:$|[/?#])/i)
        if (farcasterToken && farcasterToken.length >= 3) {
            const network = String(farcasterToken[1]).toLowerCase() === "base" ? "base" : "eth"
            return network + ":" + String(farcasterToken[2]).toLowerCase()
        }

        const address = normalizedAddress(text)
        if (address.length === 0)
            return ""

        const lower = text.toLowerCase()
        let network = ""

        let chainMatch = lower.match(/[?&](?:chainid|chain_id)=(\d+)(?:&|$)/)
        if (chainMatch && chainMatch.length >= 2)
            network = networkFromChainId(chainMatch[1])

        if (network.length === 0) {
            chainMatch = lower.match(/eip155[:%3a]+(8453|1)(?:[^0-9]|$)/)
            if (chainMatch && chainMatch.length >= 2)
                network = networkFromChainId(chainMatch[1])
        }

        if (network.length === 0 && (lower.indexOf("chain=base") >= 0
                || lower.indexOf("geckoterminal.com/base/") >= 0
                || lower.indexOf("/networks/base/tokens/") >= 0
                || lower.indexOf("basescan.org/token/") >= 0
                || lower.indexOf("nounspace.com/t/base/") >= 0))
            network = "base"

        if (network.length === 0 && (lower.indexOf("chain=ethereum") >= 0
                || lower.indexOf("chain=eth") >= 0
                || lower.indexOf("/networks/eth/tokens/") >= 0
                || lower.indexOf("/networks/ethereum/tokens/") >= 0
                || lower.indexOf("etherscan.io/token/") >= 0))
            network = "eth"

        // Farcaster's token route may carry the CAIP asset in a query/path
        // component. Do not assume a network merely from the farcaster.xyz host;
        // the chain must still be explicit.
        return network.length > 0 ? network + ":" + address : ""
    }

    function locatorFromEmbed(embed) {
        const value = embed || ({})
        const explicit = canonicalLocator(value.tokenLocator || value.locator || "")
        if (explicit.length > 0)
            return explicit

        const kind = String(value.kind || "").toLowerCase()
        const address = normalizedAddress(value.contractAddress || value.tokenAddress || "")
        if (address.length > 0) {
            const chain = String(value.network || value.chain || "").toLowerCase()
            if (chain === "base" || chain === "8453")
                return "base:" + address
            if (chain === "eth" || chain === "ethereum" || chain === "1")
                return "eth:" + address
        }

        const target = String(value.url || value.value || "")
        const fromUrl = locatorFromUrl(target)
        if (fromUrl.length > 0)
            return fromUrl

        // `token-address` is authoritative only when it carries a valid locator;
        // malformed token embeds remain generic instead of guessing a chain.
        if (kind === "token-address")
            return ""
        return ""
    }

    function sameLocator(left, right) {
        const a = canonicalLocator(left)
        const b = canonicalLocator(right)
        return a.length > 0 && b.length > 0 && a === b
    }
}
