import requests
import time
import json
import asyncio
import aiohttp
from datetime import datetime
from flask import Flask, request, jsonify
from flask_cors import CORS  # Добавляем CORS поддержку
import threading

app = Flask(__name__)
CORS(app)  # Разрешаем кросс-доменные запросы

# ========== НАСТРОЙКИ ==========
SERVER_URL = "http://192.168.31.63:8000/debug/state"
EVENT_SERVER_URL = "http://192.168.31.63:8000/event"
ACCESS_TOKEN = "citizens3846"

# ========== ХРАНЕНИЕ ДАННЫХ ==========
sound_commands = {
    '32': {'frequency': 0, 'duration': 0, 'pending': False},
    '25': {'frequency': 0, 'duration': 0, 'pending': False},
    '27': {'frequency': 0, 'duration': 0, 'pending': False},
    '19': {'frequency': 0, 'duration': 0, 'pending': False}
}

camera_colors = {
    '34': {'r': 0, 'g': 0, 'b': 0}
}

camera_detections = {
    '32': {'detections': [], 'top_detection': 'none', 'confidence': 0, 'timestamp': 0},
    '25': {'detections': [], 'top_detection': 'none', 'confidence': 0, 'timestamp': 0},
    '27': {'detections': [], 'top_detection': 'none', 'confidence': 0, 'timestamp': 0},
    '19': {'detections': [], 'top_detection': 'none', 'confidence': 0, 'timestamp': 0},
    '34': {'detections': [], 'top_detection': 'none', 'confidence': 0, 'timestamp': 0}
}

# Данные с датчика (инициализируем тестовыми значениями)
sensor_data = {
    'temperature': 22.5,
    'humidity': 55.0,
    'pressure': 1013.25,
    'distance': 250,
    'timestamp': time.time()
}

# Данные расстояния
distance_data = {
    '32': 250,
    '25': 250,
    '27': 250,
    '19': 250
}

vibromotor_state = 0

last_timestamps = {
    '19': None,
    '25': None,
    '27': None,
    '32': None,
    '34': None
}

# Блокировки
commands_lock = threading.Lock()
colors_lock = threading.Lock()
detections_lock = threading.Lock()
sensor_lock = threading.Lock()
vibromotor_lock = threading.Lock()
distance_lock = threading.Lock()


# ========== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==========
def parse_timestamp(ts):
    if ts is None:
        return "None"
    return datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')


def get_server_data():
    try:
        response = requests.get(SERVER_URL, timeout=5)
        data = response.json()
        return data
    except Exception as e:
        print(f"❌ Ошибка подключения к серверу: {e}")
        return None


async def send_camera_detection_to_server(device_id, detection, confidence):
    headers = {
        "Content-Type": "application/json",
        "X-Access-Token": ACCESS_TOKEN
    }

    payload = {
        "type": 6,
        "device_id": int(device_id),
        "user_id": detection,
        "confidence": confidence,
        "timestamp": int(time.time())
    }

    try:
        async with aiohttp.ClientSession() as session:
            async with session.post(EVENT_SERVER_URL, json=payload, headers=headers) as resp:
                text = await resp.text()
                print(f"📤 Камера {device_id}: {detection} ({confidence * 100:.1f}%)")
                return True
    except Exception as e:
        print(f"❌ Ошибка отправки данных камеры {device_id}: {e}")
        return False


def process_devices(data):
    if not data:
        return

    print(f"\n{'=' * 70}")
    print(f"📡 ПРОВЕРКА ГЛАВНОГО СЕРВЕРА - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'=' * 70}")

    for device_id, info in data.get('devices_type1', {}).items():
        if device_id in ['19', '25', '27', '32']:
            current_timestamp = info.get('last_update')
            last_timestamp = last_timestamps[device_id]

            if current_timestamp != last_timestamp:
                freq = info.get('frequency_hz', 0)
                dur = info.get('duration_ms', 0)

                print(f"\n🔊 МОДУЛЬ {device_id}")
                print(f"   Частота: {freq} Hz | Длительность: {dur} ms")

                if freq > 0 and dur > 0:
                    with commands_lock:
                        sound_commands[device_id]['frequency'] = freq
                        sound_commands[device_id]['duration'] = dur
                        sound_commands[device_id]['pending'] = True
                    print(f"   ✅ КОМАНДА ГОТОВА")
                    last_timestamps[device_id] = current_timestamp

    for device_id, info in data.get('devices_type2', {}).items():
        if device_id == '34':
            current_timestamp = info.get('last_update')
            last_timestamp = last_timestamps['34']

            if current_timestamp != last_timestamp:
                color = info.get('color', {})
                r = color.get('r', 0)
                g = color.get('g', 0)
                b = color.get('b', 0)

                print(f"\n💡 МОДУЛЬ {device_id}")
                print(f"   RGB: ({r}, {g}, {b})")

                with colors_lock:
                    camera_colors[device_id]['r'] = r
                    camera_colors[device_id]['g'] = g
                    camera_colors[device_id]['b'] = b
                print(f"   ✅ ЦВЕТ ОБНОВЛЕН")
                last_timestamps[device_id] = current_timestamp


# ========== HTTP ЭНДПОИНТЫ ==========

@app.route('/get_command/<device_id>', methods=['GET'])
def get_command(device_id):
    if device_id not in sound_commands:
        return "Device not found", 404

    with commands_lock:
        cmd = sound_commands[device_id]
        with vibromotor_lock:
            vib_state = vibromotor_state

        if cmd['pending']:
            cmd['pending'] = False
            response = f"{cmd['frequency']},{cmd['duration']},{vib_state}"
            print(f"📤 Отправлена команда на {device_id}: {response}")
            return response, 200
        else:
            response = f"0,0,{vib_state}"
            return response, 200


@app.route('/get_color/<device_id>', methods=['GET'])
def get_color(device_id):
    if device_id not in camera_colors:
        return "Device not found", 404

    with colors_lock:
        color = camera_colors[device_id]
        response = f"{color['r']},{color['g']},{color['b']}"
    return response, 200


@app.route('/get_sensor_data', methods=['GET'])
def get_sensor_data():
    """Возвращает данные с датчика в формате JSON"""
    with sensor_lock:
        response = {
            "temperature": sensor_data.get('temperature', 22.5),
            "humidity": sensor_data.get('humidity', 55.0),
            "pressure": sensor_data.get('pressure', 1013.25),
            "distance": sensor_data.get('distance', 250)
        }

    print(
        f"📡 Запрос данных датчика: T={response['temperature']:.1f}°C, H={response['humidity']:.1f}%, P={response['pressure']:.1f}hPa, D={response['distance']}см")
    return jsonify(response), 200


@app.route('/update_detection', methods=['POST', 'GET'])
def update_detection():
    if request.method == 'POST':
        data = request.get_json()
    else:
        data = {
            'device_id': request.args.get('device_id'),
            'top_detection': request.args.get('top_label'),
            'confidence': float(request.args.get('top_confidence', 0)),
            'detections': []
        }

        for i in range(1, 4):
            label = request.args.get(f'label{i}')
            conf = request.args.get(f'conf{i}')
            if label and conf:
                data['detections'].append({
                    'label': label,
                    'confidence': float(conf),
                    'rank': i
                })

    device_id = str(data.get('device_id', '32'))
    timestamp = time.time()
    detections = data.get('detections', [])
    top_detection = data.get('top_detection', 'none')
    confidence = data.get('confidence', 0)

    with detections_lock:
        camera_detections[device_id] = {
            'detections': detections,
            'top_detection': top_detection,
            'confidence': confidence,
            'timestamp': timestamp
        }

    print(f"\n📷 КАМЕРА {device_id}: {top_detection} ({confidence * 100:.1f}%)")

    if top_detection != 'none' and confidence > 0.5:
        asyncio.run(send_camera_detection_to_server(device_id, top_detection, confidence))

    return "OK", 200


@app.route('/sensor_data', methods=['POST'])
def receive_sensor_data():
    """Принимает данные с датчика"""
    data = request.get_json()

    temperature = data.get('temperature', 22.5)
    humidity = data.get('humidity', 55.0)
    pressure = data.get('pressure', 1013.25)
    timestamp = time.time()

    with sensor_lock:
        sensor_data['temperature'] = temperature
        sensor_data['humidity'] = humidity
        sensor_data['pressure'] = pressure
        sensor_data['timestamp'] = timestamp

    print(f"\n🌡️ ДАННЫЕ ДАТЧИКА: {temperature}°C, {humidity}%, {pressure}hPa")

    comfort = "✅ Комфортно"
    if temperature < 18:
        comfort = "❄️ Холодно"
    elif temperature > 26:
        comfort = "🔥 Жарко"

    print(f"   📝 Оценка: {comfort}")

    return "OK", 200


@app.route('/distance/<device_id>', methods=['GET'])
def receive_distance(device_id):
    distance_value = request.args.get('value', 250)
    try:
        distance = int(distance_value)
    except:
        distance = 250

    with distance_lock:
        distance_data[device_id] = distance
        with sensor_lock:
            sensor_data['distance'] = distance

    print(f"📏 Расстояние {device_id}: {distance} мм ({distance / 10:.1f} см)")
    return "OK", 200


@app.route('/get_distance/<device_id>', methods=['GET'])
def get_distance(device_id):
    if device_id not in distance_data:
        return "Device not found", 404

    with distance_lock:
        distance = distance_data.get(device_id, 250)

    response = {
        "device_id": device_id,
        "distance_mm": distance,
        "distance_cm": distance / 10.0
    }
    return jsonify(response), 200


@app.route('/set/vibromotor/<int:state>', methods=['GET'])
def set_vibromotor(state):
    if state not in [0, 1]:
        return "Invalid state. Use 0 or 1", 400

    with vibromotor_lock:
        global vibromotor_state
        vibromotor_state = state

    state_text = "ВКЛЮЧЕН" if state == 1 else "ВЫКЛЮЧЕН"
    print(f"\n📳 ВИБРОМОТОР: {state_text}")
    return f"Vibromotor set to {state_text}", 200


@app.route('/get/vibromotor', methods=['GET'])
def get_vibromotor():
    with vibromotor_lock:
        state = vibromotor_state

    response = {
        "state": state,
        "status": "ON" if state == 1 else "OFF"
    }
    return jsonify(response), 200


@app.route('/status', methods=['GET'])
def get_status():
    status = {
        'sound_modules': {},
        'led_module': {},
        'cameras': {},
        'sensor': {},
        'vibromotor': {},
        'distances': {}
    }

    with commands_lock:
        for device_id, cmd in sound_commands.items():
            status['sound_modules'][device_id] = {
                'frequency': cmd['frequency'],
                'duration': cmd['duration'],
                'pending': cmd['pending']
            }

    with colors_lock:
        status['led_module']['34'] = {'color': dict(camera_colors['34'])}

    with detections_lock:
        for device_id, det in camera_detections.items():
            status['cameras'][device_id] = {
                'top_detection': det.get('top_detection', 'none'),
                'confidence': det.get('confidence', 0)
            }

    with sensor_lock:
        status['sensor'] = {
            'temperature': sensor_data.get('temperature', 0),
            'humidity': sensor_data.get('humidity', 0),
            'pressure': sensor_data.get('pressure', 0),
            'distance': sensor_data.get('distance', 0)
        }

    with vibromotor_lock:
        status['vibromotor'] = {'state': vibromotor_state}

    with distance_lock:
        status['distances'] = distance_data.copy()

    return jsonify(status), 200


@app.route('/test_data', methods=['GET'])
def test_data():
    """Эндпоинт для тестирования - генерирует случайные данные"""
    import random
    test_sensor_data = {
        "temperature": round(random.uniform(15, 30), 1),
        "humidity": round(random.uniform(30, 80), 1),
        "pressure": round(random.uniform(990, 1030), 1),
        "distance": random.randint(50, 400)
    }
    return jsonify(test_sensor_data), 200


# ========== ФОНОВЫЙ ПОТОК ==========
def monitor_server():
    print("=" * 70)
    print("🔄 ЗАПУСК МОНИТОРИНГА ГЛАВНОГО СЕРВЕРА")
    print("=" * 70)
    print(f"📡 Сервер данных: {SERVER_URL}")
    print(f"🌐 HTTP сервер запущен на порту 5000")
    print("=" * 70)
    print("📋 Доступные эндпоинты:")
    print("   - GET  /get_sensor_data     → данные датчика (JSON)")
    print("   - GET  /status              → статус всех модулей")
    print("   - GET  /test_data           → тестовые данные")
    print("   - GET  /set/vibromotor/<0/1> → управление вибромотором")
    print("   - GET  /get/vibromotor      → состояние вибромотора")
    print("=" * 70)

    while True:
        try:
            data = get_server_data()
            if data:
                process_devices(data)
            time.sleep(2)
        except Exception as e:
            print(f"❌ Ошибка в мониторинге: {e}")
            time.sleep(5)


# ========== ЗАПУСК СЕРВЕРА ==========
if __name__ == "__main__":
    try:
        print("\n" + "=" * 70)
        print("🚀 ЗАПУСК ЛОКАЛЬНОГО СЕРВЕРА УПРАВЛЕНИЯ")
        print("=" * 70)

        # Запускаем мониторинг
        monitor_thread = threading.Thread(target=monitor_server, daemon=True)
        monitor_thread.start()

        # Запускаем Flask сервер
        print("\n🌐 Запуск HTTP сервера...")
        print("📍 Адрес: http://0.0.0.0:5000")
        print("📍 Для доступа с других устройств используйте IP: http://192.168.31.105:5000")
        print("\n" + "=" * 70 + "\n")

        app.run(host='0.0.0.0', port=5000, debug=False, threaded=True)

    except KeyboardInterrupt:
        print("\n\n👋 СЕРВЕР ОСТАНОВЛЕН")
