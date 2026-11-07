import { DurableObject } from "cloudflare:workers";

// One global instance of this Durable Object sees every request, so its
// counters are exact. Per-minute windows live in memory (losing them on an
// eviction only makes the limit briefly more lenient); daily counters are
// persisted so the spending cap survives restarts.

export interface Limits {
  playerPerMinute: number;
  ipPerMinute: number;
  playerPerDay: number;
  globalPerDay: number;
}

export type LimitReason = "player_minute" | "ip_minute" | "player_day" | "global_day";

export type GuardResult =
  | { ok: true }
  | { ok: false; reason: LimitReason; retryAfter: number };

const MINUTE_MS = 60_000;
const DELETE_BATCH = 128;

export class Guard extends DurableObject {
  private windows = new Map<string, number[]>();
  private currentDay = "";
  private checksSincePrune = 0;

  async check(ip: string, playerId: string, limits: Limits): Promise<GuardResult> {
    const now = Date.now();
    const windowStart = now - MINUTE_MS;

    const ipHits = this.recentHits(`ip:${ip}`, windowStart);
    if (ipHits.length >= limits.ipPerMinute) {
      return { ok: false, reason: "ip_minute", retryAfter: secondsUntil(ipHits[0] + MINUTE_MS, now) };
    }
    const playerHits = this.recentHits(`player:${playerId}`, windowStart);
    if (playerHits.length >= limits.playerPerMinute) {
      return { ok: false, reason: "player_minute", retryAfter: secondsUntil(playerHits[0] + MINUTE_MS, now) };
    }

    const day = new Date(now).toISOString().slice(0, 10);
    if (day !== this.currentDay) {
      await this.deleteOldDays(day);
      this.currentDay = day;
    }

    const globalKey = `day:${day}:global`;
    const playerKey = `day:${day}:player:${playerId}`;
    const counts = await this.ctx.storage.get<number>([globalKey, playerKey]);
    const globalCount = counts.get(globalKey) ?? 0;
    const playerCount = counts.get(playerKey) ?? 0;

    if (globalCount >= limits.globalPerDay) {
      return { ok: false, reason: "global_day", retryAfter: secondsUntilUtcMidnight(now) };
    }
    if (playerCount >= limits.playerPerDay) {
      return { ok: false, reason: "player_day", retryAfter: secondsUntilUtcMidnight(now) };
    }

    ipHits.push(now);
    playerHits.push(now);
    await this.ctx.storage.put({ [globalKey]: globalCount + 1, [playerKey]: playerCount + 1 });

    if (++this.checksSincePrune >= 200) {
      this.pruneWindows(windowStart);
      this.checksSincePrune = 0;
    }
    return { ok: true };
  }

  private recentHits(key: string, windowStart: number): number[] {
    const hits = (this.windows.get(key) ?? []).filter((t) => t > windowStart);
    this.windows.set(key, hits);
    return hits;
  }

  private pruneWindows(windowStart: number): void {
    for (const [key, hits] of this.windows) {
      if (hits.every((t) => t <= windowStart)) this.windows.delete(key);
    }
  }

  private async deleteOldDays(today: string): Promise<void> {
    // Keys sort by date, so everything before today's prefix is stale
    const stale = await this.ctx.storage.list({ prefix: "day:", end: `day:${today}` });
    const keys = [...stale.keys()];
    for (let i = 0; i < keys.length; i += DELETE_BATCH) {
      await this.ctx.storage.delete(keys.slice(i, i + DELETE_BATCH));
    }
  }
}

function secondsUntil(time: number, now: number): number {
  return Math.max(1, Math.ceil((time - now) / 1000));
}

function secondsUntilUtcMidnight(now: number): number {
  const midnight = new Date(now);
  midnight.setUTCHours(24, 0, 0, 0);
  return secondsUntil(midnight.getTime(), now);
}
