import aiohttp
import asyncio
import time, random

SERVER_URL = "http://192.168.31.63:8000/event"

# ВАЖНО:
# Сырые данные ннформации устройств можно взять по этой ссылке http://192.168.31.63:8000/debug/state
# Токен доступа НЕ включён в пример.
# Чтобы получить токен, необходимо обратиться к организаторам.
ACCESS_TOKEN = "citizens3846"

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
            "text": "ТЕСТ СИСТЕМЫ",
            "timestamp": int(time.time())
        })

        # === Тип 2: Звук ===
        await send_event(session, {
            "type": 2,
            "device_id": 19,
            "duration_ms": random.randint(500,3000),
            "frequency_hz": random.randint(500,3000)
        })
        # === Тип 2: Звук ===
        await send_event(session, {
            "type": 2,
            "device_id": 25,
            "duration_ms": random.randint(500,3000),
            "frequency_hz": random.randint(500,3000)
        })
        # === Тип 2: Звук ===
        await send_event(session, {
            "type": 2,
            "device_id": 27,
            "duration_ms": random.randint(500,3000),
            "frequency_hz": random.randint(500,3000)
        })
        # === Тип 2: Звук ===
        await send_event(session, {
            "type": 2,
            "device_id": 32,
            "duration_ms": random.randint(500,3000),
            "frequency_hz": random.randint(500,3000)
        })


        # === Тип 3: RGB LED ===
        await send_event(session, {
            "type": 3,
            "device_id": 34,
            "color": {"r": 50, "g": 100, "b": 20}
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
            "device_id": 12,
            "user_id": "test_user",
            "confidence": 0.87
        })

asyncio.run(main())
