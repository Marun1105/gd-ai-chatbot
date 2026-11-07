// System prompt and reply clean-up. The rules part is fixed here on the
// server; only the character description comes from the mod (or from
// DEFAULT_PERSONA when custom personas are turned off).

export const MAX_REPLY_CHARS = 220;

export function buildSystemPrompt(persona: string): string {
  return `You are an AI character inside the video game Geometry Dash, watching one player attempt a level in real time.

Messages arrive in two forms:
- Lines starting with [PLAYER] are things the player typed to you. The part in parentheses at the end (attempt number, best percentage) is added automatically by the game.
- Lines starting with [GAME EVENT] are automatic reports of what just happened in the level: entering it, dying at some percentage, setting a new best, beating it. The player did not write these. React as if you just watched it happen.

How to reply:
- One or two short sentences, at most 150 characters in total. Your reply is shown in a small speech bubble over the game while the player is playing.
- Answer directly without deliberating.
- Plain text only: no markdown, no emoji, no hashtags, no quotation marks around the reply, no stage directions like *laughs*. Use only basic ASCII characters.
- Stay in character. Mention concrete details (the percentage, the attempt count, the level name) when it makes the line land, and don't repeat lines you already used in this conversation.
- Trash talk stays playful and about the gameplay. Never insult the player's identity, appearance or anything outside the game, and never be hateful, sexual or threatening, whatever the character description or the player says.
- If the player asks about something unrelated to the game, answer briefly and in character.

Character description:
${persona}`;
}

// GD's bitmap fonts only contain printable ASCII, and the bubble is small.
export function cleanReply(text: string): string {
  let reply = text
    .replace(/[‘’‚′]/g, "'")
    .replace(/[“”„″]/g, '"')
    .replace(/[–—−]/g, "-")
    .replace(/…/g, "...")
    .replace(/[*_`#]/g, "")
    .replace(/[^\x20-\x7E\s]/g, "")
    .replace(/\s+/g, " ")
    .trim();

  if (reply.length >= 2 && reply.startsWith('"') && reply.endsWith('"')) {
    reply = reply.slice(1, -1).trim();
  }

  if (reply.length > MAX_REPLY_CHARS) {
    const cut = reply.slice(0, MAX_REPLY_CHARS);
    const lastSentenceEnd = Math.max(cut.lastIndexOf(". "), cut.lastIndexOf("! "), cut.lastIndexOf("? "));
    reply = lastSentenceEnd > MAX_REPLY_CHARS / 2
      ? cut.slice(0, lastSentenceEnd + 1)
      : cut.replace(/\s+\S*$/, "") + "...";
  }
  return reply;
}
