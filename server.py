import discord
import asyncio
import time
import json
import re
import urllib.request
import urllib.parse
from aiohttp import web

# ════════════════════════════════════════════════════════
#  KONFIGURASI
# ════════════════════════════════════════════════════════
BOT_TOKEN   = "MTQ5Mjc1NDI3Njc4NTcyMTU0NA.GDt6ri.piXoYGkVv1YXpZ7MSELjFC-onWFkid8gukYju0"
USER_ID     = 713597471414026240        # ganti dengan Discord user ID kamu
PORT        = 3000
LYRIC_LEAD  = 2.4              

# ════════════════════════════════════════════════════════
#  STATE
# ════════════════════════════════════════════════════════
current = {}      # data lagu sekarang
lyrics  = {       # cache lirik
    "key":      "",
    "lines":    [],
    "duration": 0,
}

# ════════════════════════════════════════════════════════
#  FETCH LIRIK — lrclib.net
# ════════════════════════════════════════════════════════
def fetch_lyrics(artist, title):
    url = (
        "https://lrclib.net/api/get"
        f"?artist_name={urllib.parse.quote(artist)}"
        f"&track_name={urllib.parse.quote(title)}"
    )
    try:
        req = urllib.request.Request(
            url, headers={"User-Agent": "ESP32-Spotify/1.0"}
        )
        with urllib.request.urlopen(req, timeout=5) as res:
            data = json.loads(res.read())

        synced   = data.get("syncedLyrics", "") or ""
        plain    = data.get("plainLyrics",  "") or ""
        duration = data.get("duration", 0)
        lines    = []

        if synced:
            for line in synced.split("\n"):
                m = re.match(r'\[(\d+):(\d+\.\d+)\](.*)', line)
                if m:
                    secs = int(m.group(1)) * 60 + float(m.group(2))
                    text = m.group(3).strip()
                    if text:
                        lines.append({"time": secs, "text": text})
        elif plain:
            lines = [
                {"time": 0, "text": l.strip()}
                for l in plain.split("\n") if l.strip()
            ]

        return lines, duration

    except Exception as e:
        print(f"[lrclib] error: {e}")
        return [], 0

# ════════════════════════════════════════════════════════
#  AMBIL LIRIK SESUAI POSISI
# ════════════════════════════════════════════════════════
def get_lyric(progress_secs):
    lines    = lyrics["lines"]
    duration = lyrics["duration"]

    if not lines:
        return {"prev": "", "curr": "", "next": ""}

    # plain lyrics — tidak ada timestamp
    if lines[0]["time"] == 0:
        total = duration or 200
        i = min(int((progress_secs / total) * len(lines)), len(lines) - 1)
        return {
            "prev": lines[i-1]["text"] if i > 0          else "",
            "curr": lines[i]["text"],
            "next": lines[i+1]["text"] if i+1 < len(lines) else "",
        }

    # synced lyrics — ada timestamp
    idx = 0
    for i, line in enumerate(lines):
        if line["time"] <= progress_secs:
            idx = i
        else:
            break

    return {
        "prev": lines[idx-1]["text"] if idx > 0             else "",
        "curr": lines[idx]["text"],
        "next": lines[idx+1]["text"] if idx+1 < len(lines)  else "",
    }

# ════════════════════════════════════════════════════════
#  DISCORD BOT
# ════════════════════════════════════════════════════════
intents          = discord.Intents.default()
intents.presences = True
intents.members   = True

bot = discord.Client(intents=intents)

@bot.event
async def on_ready():
    print(f"[bot] logged in as {bot.user}")
    print(f"[bot] watching user ID: {USER_ID}")

@bot.event
async def on_presence_update(before, after):
    # abaikan kalau bukan user kita
    if after.id != USER_ID:
        return

    # cari activity Spotify
    spotify = next(
        (a for a in after.activities if isinstance(a, discord.Spotify)),
        None
    )

    if not spotify:
        current.update({"playing": False})
        print("[bot] spotify stopped")
        return

    artist = ", ".join(spotify.artists)
    title  = spotify.title
    key    = f"{artist}|{title}"

    # fetch lirik kalau lagu baru
    if lyrics["key"] != key:
        print(f"[bot] new track: {artist} - {title}")
        lines, duration = await asyncio.get_event_loop().run_in_executor(
            None, fetch_lyrics, artist, title
        )
        lyrics.update({"key": key, "lines": lines, "duration": duration})
        print(f"[lrclib] {len(lines)} lines, {duration}s")

    current.update({
        "playing":     True,
        "title":       title,
        "artist":      artist,
        "album":       spotify.album,
        "start_epoch": spotify.start.timestamp(),
        "end_epoch":   spotify.end.timestamp(),
        "duration_ms": int((spotify.end - spotify.start).total_seconds() * 1000),
        "track_id":    spotify.track_id,
    })

# ════════════════════════════════════════════════════════
#  HTTP SERVER — endpoint untuk ESP32
# ════════════════════════════════════════════════════════
async def handle_now_playing(request):
    if not current.get("playing"):
        return web.json_response({"playing": False})

    now      = time.time()
    pos_ms   = int((now - current["start_epoch"]) * 1000)
    pos_secs = pos_ms / 1000 + LYRIC_LEAD   # lead sama seperti Node.js lama

    lyric = get_lyric(pos_secs)

    return web.json_response({
        "title":       current["title"],
        "artist":      current["artist"],
        "playing":     True,
        "position_ms": pos_ms,
        "duration_ms": current["duration_ms"],
        "lyric_prev":  lyric["prev"],
        "lyric_curr":  lyric["curr"],
        "lyric_next":  lyric["next"],
    })

async def handle_debug(request):
    return web.json_response({
        "current": current,
        "lyrics": {
            "key":      lyrics["key"],
            "total":    len(lyrics["lines"]),
            "duration": lyrics["duration"],
            "sample":   lyrics["lines"][:5],
        }
    })

# ════════════════════════════════════════════════════════
#  MAIN — jalankan bot + HTTP server bersamaan
# ════════════════════════════════════════════════════════
async def main():
    # setup HTTP server
    app = web.Application()
    app.router.add_get("/now-playing", handle_now_playing)
    app.router.add_get("/debug",       handle_debug)

    runner = web.AppRunner(app)
    await runner.setup()
    await web.TCPSite(runner, "0.0.0.0", PORT).start()
    print(f"[server] http://0.0.0.0:{PORT}/now-playing")

    # jalankan Discord bot
    await bot.start(BOT_TOKEN)

asyncio.run(main())