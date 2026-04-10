import aiohttp
import asyncio
import time

SERVER_URL = "http://192.168.31.63:8000/event"

# ВАЖНО:
# Сырые данные ннформации устройств можно взять по этой ссылке http://192.168.31.63:8000/debug/state
# Токен доступа НЕ включён в пример.
# Чтобы получить токен, необходимо обратиться к организаторам.
ACCESS_TOKEN = "<ПОЛУЧИТЕ_У_ОРГАНИЗАТОРОВ>"

async def send_event(session, payload):
    headers = {
        "Content-Type": "application/json",
        "X-Access-Token": ACCESS_TOKEN
    }

    async with session.post(SERVER_URL, json=payload, headers=headers) as resp:
        text = await resp.text()
        print(f"→ {payload} | Ответ сервера: {text}")

async def main():
    async with aiohttp.ClientSession() as session:

        # === Тип 1: Голосовая команда ===
        await send_event(session, {
            "type": 1,
            "text": "Пример голосовой команды",
            "timestamp": int(time.time())
        })

        # === Тип 2: Звук ===
        await send_event(session, {
            "type": 2,
            "device_id": 1,
            "duration_ms": 800,
            "frequency_hz": 1500
        })

        # === Тип 3: RGB LED ===
        await send_event(session, {
            "type": 3,
            "device_id": 2,
            "color": {"r": 0, "g": 200, "b": 255}
        })

        # === Тип 4: RFID ===
        await send_event(session, {
            "type": 4,
            "device_id": 3,
            "rfid_code": "ABC123XYZ"
        })

        # === Тип 5: Препятствие ===
        await send_event(session, {
            "type": 5,
            "location_id": 7,
            "obstacle_type": "construction",
            "reroute_required": True,
            "message": "На маршруте ведутся работы"
        })

        # === Тип 6: Распознавание пользователя ===
        await send_event(session, {
            "type": 6,
            "device_id": 4,
            "user_id": "test_user",
            "confidence": 0.87
        })

asyncio.run(main())
