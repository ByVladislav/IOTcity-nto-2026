import json
import requests
from datetime import datetime
from collections import defaultdict

# URL для получения данных
url = "http://192.168.31.63:8000/debug/state"

# Интересующие ID только для device_type1 и device_type2
target_device_ids = {19, 25, 27, 32, 34}


def parse_timestamp(ts):
    """Преобразует timestamp в читаемый формат"""
    return datetime.fromtimestamp(ts).strftime('%Y-%m-%d %H:%M:%S')


def get_latest_entries(data_dict, filter_ids=None):
    """
    Оставляет только самые свежие записи для каждого ID
    filter_ids: если указан, фильтрует только эти ID, иначе берет все
    """
    latest = {}
    for item_id, info in data_dict.items():
        # Применяем фильтр только если он задан
        if filter_ids is not None and int(item_id) not in filter_ids:
            continue

        current_ts = info.get('last_update') or info.get('timestamp')
        if current_ts:
            if item_id not in latest or current_ts > latest[item_id]['timestamp']:
                latest[item_id] = {**info, 'timestamp': current_ts}
    return latest


def process_events(events_list, filter_ids=None):
    """Фильтрует события и оставляет самые свежие по timestamp"""
    latest = {}
    for event in events_list:
        device_id = event.get('device_id')
        if filter_ids is not None and device_id not in filter_ids:
            continue

        ts = event.get('timestamp')
        if device_id not in latest or ts > latest[device_id]['timestamp']:
            latest[device_id] = {**event, 'timestamp': ts}
    return latest


def get_latest_voice_queue(voice_queue):
    """Для голосовой очереди берем последние 5 сообщений (или все, если меньше)"""
    if not voice_queue:
        return []
    # Возвращаем последние 5 сообщений (самые свежие)
    return voice_queue[-5:]


# Загружаем данные
response = requests.get(url)
data = response.json()

# Обрабатываем данные
result = {
    # Для девайсов 1 и 2 типа - фильтруем по target_device_ids
    "devices_type1": get_latest_entries(data.get("devices_type1", {}), filter_ids=target_device_ids),
    "devices_type2": get_latest_entries(data.get("devices_type2", {}), filter_ids=target_device_ids),

    # Для остальных категорий - берем все ID с самыми свежими данными
    "buses": get_latest_entries(data.get("buses", {})),
    "obstacles": get_latest_entries(data.get("obstacles", {})),
    "rfid_events": process_events(data.get("events", {}).get("rfid", [])),
    "face_events": process_events(data.get("events", {}).get("face", []))
}

# Удаляем пустые категории
result = {k: v for k, v in result.items() if v}

# Добавляем читаемые timestamp
for category in ['devices_type1', 'devices_type2', 'buses', 'obstacles']:
    for dev_id in result.get(category, {}):
        if 'last_update' in result[category][dev_id]:
            result[category][dev_id]['last_update_readable'] = parse_timestamp(result[category][dev_id]['last_update'])
        if 'timestamp' in result[category][dev_id]:
            result[category][dev_id]['timestamp_readable'] = parse_timestamp(result[category][dev_id]['timestamp'])

for event_type in ['rfid_events', 'face_events']:
    for dev_id in result.get(event_type, {}):
        result[event_type][dev_id]['timestamp_readable'] = parse_timestamp(result[event_type][dev_id]['timestamp'])

# Сохраняем в файл
output_file = "filtered_latest_data.json"
with open(output_file, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"✅ Данные сохранены в {output_file}")

# Выводим результат на экран
print("\n" + "=" * 60)
print("📊 ОТФОРМАТИРОВАННЫЙ РЕЗУЛЬТАТ:")
print("=" * 60)
print(json.dumps(result, ensure_ascii=False, indent=2))

# Дополнительная информация по ID
print("\n" + "=" * 60)
print("📋 СТАТИСТИКА:")
print("=" * 60)
print(f"devices_type1 (только ID {sorted(target_device_ids)}): {len(result.get('devices_type1', {}))} записей")
print(f"devices_type2 (только ID {sorted(target_device_ids)}): {len(result.get('devices_type2', {}))} записей")
print(f"buses (все ID): {len(result.get('buses', {}))} записей")
print(f"obstacles (все ID): {len(result.get('obstacles', {}))} записей")
print(f"rfid_events (все ID): {len(result.get('rfid_events', {}))} записей")
print(f"face_events (все ID): {len(result.get('face_events', {}))} записей")
print(f"voice_queue (последние сообщения): {len(result.get('voice_queue', []))} сообщений")
