export class GenyReactionMatcher {
  constructor(config) {
    this.config = config;
    this.rules = [...config.rules].sort((a, b) => b.priority - a.priority);
  }

  match(text) {
    const input = String(text ?? "");
    for (const rule of this.rules) {
      if (this.#matchesRule(input, rule)) {
        return { id: rule.id, icon: rule.icon, priority: rule.priority };
      }
    }
    return { id: "default-like", icon: this.config.defaultIcon, priority: -1 };
  }

  #matchesRule(input, rule) {
    const spec = rule.match ?? {};
    const source = spec.caseSensitive ? input : input.toLocaleLowerCase();
    const normalize = value => spec.caseSensitive ? value : value.toLocaleLowerCase();

    for (const phrase of spec.phrases ?? []) {
      if (source.includes(normalize(phrase))) return true;
    }

    for (const token of spec.tokens ?? []) {
      const escaped = normalize(token).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      const pattern = new RegExp(
        `(^|[^\\p{L}\\p{N}_])${escaped}(?=$|[^\\p{L}\\p{N}_])`, "u"
      );
      if (pattern.test(source)) return true;
    }
    return false;
  }
}
