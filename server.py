# server.py — the backend. Holds the API key, talks to Anthropic, answers the browser.
#
# Run it with:   uvicorn server:app --reload
# It listens on   http://localhost:8000
#
# Install once:   pip install fastapi "uvicorn[standard]" anthropic

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from anthropic import Anthropic

app = FastAPI()
client = Anthropic()  # reads ANTHROPIC_API_KEY from your environment

# The browser and the server run on different ports during development,
# so the browser needs permission to call the server. This grants it.
# For a real deployed app, replace "*" with your actual website's address.
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


# This describes the shape of what the browser sends: an image and a question.
class Ask(BaseModel):
    image_base64: str
    question: str


@app.post("/ask")
def ask(body: Ask):
    resp = client.messages.create(
        model="claude-haiku-4-5-20251001",   # cheap model for the constant Q&A
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