from flask import Flask, request
from datetime import datetime

app = Flask(__name__)


@app.route('/sensor_data', methods=['POST'])
def receive_data():
    """Принимает данные с датчика и выводит в консоль"""
    data = request.get_json()

    device_id = data.get('device_id')
    temperature = data.get('temperature')
    humidity = data.get('humidity')
    pressure = data.get('pressure')
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    # Красивый вывод в консоль
    print("\n" + "=" * 60)
    print(f"📡 ПОЛУЧЕНЫ ДАННЫЕ ОТ {device_id}")
    print("=" * 60)
    print(f"   🌡️ Температура: {temperature} °C")
    print(f"   💧 Влажность:    {humidity} %")
    print(f"   📊 Давление:     {pressure} hPa")
    print(f"   🕐 Время:        {timestamp}")
    print("=" * 60)

    return "OK", 200


if __name__ == '__main__':
    print("=" * 60)
    print("🌡️ СЕРВЕР ПРИЕМА ДАННЫХ BME280")
    print("=" * 60)
    print("Сервер запущен на http://0.0.0.0:5000")
    print("Ожидание данных от датчика...")
    print("=" * 60)

    app.run(host='0.0.0.0', port=5000, debug=False)