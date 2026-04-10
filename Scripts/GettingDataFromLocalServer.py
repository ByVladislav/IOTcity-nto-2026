import requests
from datetime import datetime

url = "http://192.168.31.105:5000/get_sensor_data"

try:
    response = requests.get(url, timeout=5)
    data = response.json()

    print(f"\n🌡️  ДАННЫЕ С ДАТЧИКА ({datetime.now().strftime('%H:%M:%S')})")
    print(f"   Температура: {data['temperature']:.1f} °C")
    print(f"   Влажность:   {data['humidity']:.1f} %")
    print(f"   Давление:    {data['pressure']:.1f} hPa")
    print(f"   Расстояние:  {data['distance']} см")

except Exception as e:
    print(f"❌ Ошибка: {e}")
