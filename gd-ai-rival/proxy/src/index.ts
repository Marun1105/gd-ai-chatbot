import Anthropic from "@anthropic-ai/sdk";
import type { BetaMessageParam } from "@anthropic-ai/sdk/resources/beta/messages/messages";
import { Guard, type LimitReason, type Limits } from "./guard";
import { buildSystemPrompt, cleanReply } from "./prompt";

export { Guard };

export interface Env {
  ANTHROPIC_API_KEY?: string;
  // Accepted as an alias, since "Claude API key" is what the Console calls it
  CLAUDE_API_KEY?: string;
  // Optional, e.g. a Cloudflare AI Gateway URL for logging/analytics
  ANTHROPIC_BASE_URL?: string;
  GUARD: DurableObjectNamespace<Guard>;
  MODEL?: string;
  ALLOWED_LEVEL_IDS?: string;
  ALLOW_CUSTOM_PERSONA?: string;
  DEFAULT_PERSONA?: string;
  PLAYER_PER_MINUTE?: string;
  IP_PER_MINUTE?: string;
  PLAYER_PER_DAY?: string;
  GLOBAL_PER_DAY?: string;
}

const DEFAULT_MODEL = "claude-opus-5-5";
const FALLBACK_PERSONA =
  "You are Rival, a cocky Geometry Dash player who thinks they are better than the player. " +
  "Trash-talk silly deaths with playful roasts, act annoyed when they make progress, and give grudging " +
  "respect when they get a new best or beat the level. Keep it playful, never hateful.";

const MAX_BODY_BYTES = 16_384;
const MAX_HISTORY = 20;
const MAX_TURN_CHARS = 500;
const MAX_MESSAGE_CHARS = 400;
const MAX_PERSONA_CHARS = 600;
const MAX_LEVEL_CHARS = 1200;
// Thinking can't be turned off and counts toward max_tokens, so leave room
// for it on top of the one or two sentences we actually want
const MAX_TOKENS = 1024;

const LIMIT_MESSAGES: Record<LimitReason, string> = {
  player_minute: "Slow down! Wait a bit before sending more.",
  ip_minute: "Too many messages from your network. Wait a bit.",
  player_day: "You've used up today's messages. Come back tomorrow.",
  global_day: "The AI is out of energy for today. Come back tomorrow.",
};

interface ChatRequest {
  playerId: string;
  levelId: number;
  persona: string;
  // Facts about the level from the game (difficulty, creator, song...)
  level: string;
  messages: BetaMessageParam[];
}

class BadRequest extends Error {}
class EmptyReply extends Error {}

export default {
  async fetch(request, env): Promise<Response> {
    const url = new URL(request.url);
    if (url.pathname === "/" && request.method === "GET") {
      return json({ status: "ok" });
    }
    if (url.pathname !== "/chat") {
      return json({ error: "not_found", message: "Not found." }, 404);
    }
    if (request.method !== "POST") {
      return json({ error: "method_not_allowed", message: "Use POST." }, 405);
    }
    return handleChat(request, env);
  },
} satisfies ExportedHandler<Env>;

async function handleChat(request: Request, env: Env): Promise<Response> {
  const apiKey = env.ANTHROPIC_API_KEY || env.CLAUDE_API_KEY;
  if (!apiKey) {
    console.error("ANTHROPIC_API_KEY is not set. Run: npx wrangler secret put ANTHROPIC_API_KEY");
    return json({ error: "proxy_misconfigured", message: "The AI proxy is misconfigured." }, 500);
  }

  let chat: ChatRequest;
  try {
    chat = await parseRequest(request, env);
  } catch (err) {
    if (err instanceof BadRequest) {
      return json({ error: "bad_request", message: err.message }, 400);
    }
    throw err;
  }

  const allowedLevels = parseIdList(env.ALLOWED_LEVEL_IDS);
  if (allowedLevels.size > 0 && !allowedLevels.has(chat.levelId)) {
    return json({ error: "level_not_allowed", message: "The AI isn't available on this level." }, 403);
  }

  const ip = request.headers.get("CF-Connecting-IP") ?? "unknown";
  const guard = env.GUARD.get(env.GUARD.idFromName("global"));
  const verdict = await guard.check(ip, chat.playerId, readLimits(env));
  if (!verdict.ok) {
    return json(
      {
        error: "rate_limited",
        reason: verdict.reason,
        message: LIMIT_MESSAGES[verdict.reason],
        retry_after: verdict.retryAfter,
      },
      429,
      { "Retry-After": String(verdict.retryAfter) },
    );
  }

  try {
    return json({ reply: await askClaude(env, apiKey, chat) });
  } catch (err) {
    return upstreamError(err);
  }
}

async function parseRequest(request: Request, env: Env): Promise<ChatRequest> {
  if (Number(request.headers.get("Content-Length") ?? 0) > MAX_BODY_BYTES) {
    throw new BadRequest("Request too large.");
  }
  const raw = await request.text();
  if (raw.length > MAX_BODY_BYTES) {
    throw new BadRequest("Request too large.");
  }

  let body: unknown;
  try {
    body = JSON.parse(raw);
  } catch {
    throw new BadRequest("Body must be JSON.");
  }
  if (!isObject(body)) {
    throw new BadRequest("Body must be a JSON object.");
  }

  const playerId = body.player_id;
  if (typeof playerId !== "string" || !/^[A-Za-z0-9_-]{8,64}$/.test(playerId)) {
    throw new BadRequest("Invalid player_id.");
  }

  const levelId = body.level_id;
  if (typeof levelId !== "number" || !Number.isInteger(levelId) || levelId < 0) {
    throw new BadRequest("Invalid level_id.");
  }

  const message = body.message;
  if (typeof message !== "string" || !message.trim() || message.length > MAX_MESSAGE_CHARS) {
    throw new BadRequest(`Message must be 1-${MAX_MESSAGE_CHARS} characters.`);
  }

  const history = body.history ?? [];
  if (!Array.isArray(history) || history.length > MAX_HISTORY) {
    throw new BadRequest(`History must be a list of at most ${MAX_HISTORY} messages.`);
  }

  const messages: BetaMessageParam[] = [];
  for (const turn of history) {
    const role = isObject(turn) ? turn.role : undefined;
    const content = isObject(turn) ? turn.content : undefined;
    if (
      (role !== "user" && role !== "assistant") ||
      typeof content !== "string" ||
      !content.trim() ||
      content.length > MAX_TURN_CHARS
    ) {
      throw new BadRequest("Invalid history entry.");
    }
    messages.push({ role, content });
  }
  // The API needs the conversation to start with a user turn
  while (messages.length > 0 && messages[0].role !== "user") {
    messages.shift();
  }
  messages.push({ role: "user", content: message });

  let persona = env.DEFAULT_PERSONA?.trim() || FALLBACK_PERSONA;
  const customAllowed = (env.ALLOW_CUSTOM_PERSONA ?? "true").toLowerCase() !== "false";
  if (customAllowed && typeof body.persona === "string" && body.persona.trim()) {
    if (body.persona.length > MAX_PERSONA_CHARS) {
      throw new BadRequest(`Personality must be at most ${MAX_PERSONA_CHARS} characters.`);
    }
    persona = body.persona.trim();
  }

  // Optional; older mod versions don't send it. Clipped rather than rejected,
  // since it's game data, not something the player typed.
  const level = typeof body.level === "string" ? body.level.trim().slice(0, MAX_LEVEL_CHARS) : "";

  return { playerId, levelId, persona, level, messages };
}

async function askClaude(env: Env, apiKey: string, chat: ChatRequest): Promise<string> {
  const client = new Anthropic({
    apiKey,
    baseURL: env.ANTHROPIC_BASE_URL || undefined,
    // The mod gives up after 30 seconds, so don't retry for longer than that
    timeout: 12_000,
    maxRetries: 1,
  });

  const response = await client.beta.messages.create({
    model: env.MODEL || DEFAULT_MODEL,
    max_tokens: MAX_TOKENS,
    system: buildSystemPrompt(chat.persona, chat.level),
    messages: chat.messages,
    // Short banter: keep thinking brief so replies arrive while the moment lasts
    output_config: { effort: "low" },
    // If the model declines, let the API retry on its recommended fallback model
    betas: ["server-side-fallback-2026-07-01"],
    fallbacks: "default",
  });

  console.log(JSON.stringify({
    model: response.model,
    stop_reason: response.stop_reason,
    input_tokens: response.usage.input_tokens,
    output_tokens: response.usage.output_tokens,
  }));

  if (response.stop_reason === "refusal") {
    // Every model in the fallback chain declined; stay in the game instead of erroring
    return "Let's keep our eyes on the level.";
  }

  const reply = cleanReply(
    response.content.flatMap((block) => (block.type === "text" ? [block.text] : [])).join(" "),
  );
  if (!reply) {
    throw new EmptyReply(`No text in response (stop_reason: ${response.stop_reason})`);
  }
  return reply;
}

function upstreamError(err: unknown): Response {
  if (err instanceof Anthropic.AuthenticationError || err instanceof Anthropic.PermissionDeniedError) {
    console.error("Anthropic rejected the API key:", err.message);
    return json({ error: "proxy_misconfigured", message: "The AI proxy is misconfigured." }, 500);
  }
  if (err instanceof Anthropic.RateLimitError) {
    console.warn("Anthropic rate limit:", err.message);
    return json({ error: "upstream_busy", message: "The AI is busy right now. Try again in a moment." }, 503);
  }
  if (err instanceof Anthropic.BadRequestError) {
    console.error("Anthropic rejected the request:", err.message);
    return json({ error: "upstream_rejected", message: "The AI couldn't handle that message." }, 502);
  }
  if (err instanceof Anthropic.APIConnectionTimeoutError) {
    console.warn("Anthropic request timed out");
    return json({ error: "upstream_timeout", message: "The AI took too long to answer." }, 504);
  }
  if (err instanceof Anthropic.APIConnectionError) {
    console.error("Couldn't reach Anthropic:", err.message);
    return json({ error: "upstream_unreachable", message: "Couldn't reach the AI." }, 502);
  }
  if (err instanceof Anthropic.APIError) {
    console.error(`Anthropic error ${err.status}:`, err.message);
    return json({ error: "upstream_error", message: "The AI is having trouble right now." }, 503);
  }
  if (err instanceof EmptyReply) {
    console.warn(err.message);
    return json({ error: "empty_reply", message: "The AI had nothing to say." }, 502);
  }
  console.error("Unexpected error:", err);
  return json({ error: "internal", message: "Something went wrong." }, 500);
}

function readLimits(env: Env): Limits {
  return {
    playerPerMinute: positiveInt(env.PLAYER_PER_MINUTE, 8),
    ipPerMinute: positiveInt(env.IP_PER_MINUTE, 20),
    playerPerDay: positiveInt(env.PLAYER_PER_DAY, 200),
    globalPerDay: positiveInt(env.GLOBAL_PER_DAY, 1000),
  };
}

function positiveInt(value: string | undefined, fallback: number): number {
  const parsed = Number.parseInt(value ?? "", 10);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
}

function parseIdList(value: string | undefined): Set<number> {
  return new Set((value ?? "").split(/[^0-9]+/).filter(Boolean).map(Number));
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function json(data: unknown, status = 200, headers: Record<string, string> = {}): Response {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json", ...headers },
  });
}
