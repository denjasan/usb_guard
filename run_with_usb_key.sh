#!/bin/bash
APP="./my_app"

# ждать ключ
while [ "$(cat /proc/usb_guard)" != "1" ]; do
  echo "Жду USB-ключ..."
  sleep 1
done

echo "Ключ найден. Запускаю..."
$APP &
APP_PID=$!

# следить за ключом
while kill -0 "$APP_PID" 2>/dev/null; do
  if [ "$(cat /proc/usb_guard)" != "1" ]; then
    echo "Ключ вынули. Закрываю приложение."
    kill "$APP_PID"
    exit 0
  fi
  sleep 1
done

