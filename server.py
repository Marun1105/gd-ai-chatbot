# server.py — бекендът. Пази API ключа, говори с Anthropic, отговаря на браузъра.
#
# Локално:   python -m uvicorn server:app --reload
# На Render:  Render използва Start Command-а автоматично (виж по-долу).
#
# Инсталиране локално:  pip install fastapi "uvicorn[standard]" anthropic

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from anthropic import Anthropic

app = FastAPI()
client = Anthropic()  # чете ANTHROPIC_API_KEY от средата

# Браузърът и сървърът са на различни адреси, затова браузърът се нуждае
# от разрешение да вика сървъра. Когато качиш страницата на твоя домейн,
# за по-сигурно замени "*" с адреса на сайта ти,
# напр. "https://martin.hristov.website".
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

SYSTEM = """Ти си учител по математика. Пред теб е снимка на страница от български учебник.
Помагаш на ученик да реши задачите. Обяснявай на български, стъпка по стъпка, ясно.
Не давай само отговора — покажи как се стига до него.
Можеш да използваш Markdown и LaTeX между $...$ — отговорът се показва в браузър."""


class Ask(BaseModel):
    image_base64: str
    question: str


@app.get("/")
def health():
    # Проста проверка, че сървърът е жив — отваряш адреса и виждаш това.
    return {"status": "ok"}


@app.post("/ask")
def ask(body: Ask):
    resp = client.messages.create(
        model="claude-haiku-4-5-20251001",
        max_tokens=1500,
        system=SYSTEM,
        messages=[{
            "role": "user",
            "content": [
                {"type": "image", "source": {
                    "type": "base64",
                    "media_type": "image/jpeg",
                    "data": body.image_base64,
                }},
                {"type": "text", "text": body.question},
            ],
        }],
    )
    answer = "".join(b.text for b in resp.content if b.type == "text")
    return {"answer": answer}
