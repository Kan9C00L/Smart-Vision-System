import cv2
from ultralytics import YOLO
import socket
import threading

# ================= 1. 硬件网络通信部分 =================
esp32_socket = None

def start_server():
    global esp32_socket
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', 8888))
    server.listen(1)
    
    while True:  # 死循环，保证断线后能重新接受连接
        print("📡 等待 ESP32 连接...")
        try:
            client, addr = server.accept()
            esp32_socket = client
            print(f"✅ ESP32 已连接！来自 IP: {addr}")
            
            # 保持接收数据的状态
            while True:
                data = client.recv(1024).decode('utf-8').strip()
                if not data: 
                    break  # 如果收到空数据，说明断开连接
                if data.startswith("DATA,"):
                    pass
        except Exception as e:
            print(f"❌ 连接断开，等待重连...")
            esp32_socket = None # 清理死连接

threading.Thread(target=start_server, daemon=True).start()

# ================= 2. 视觉感知部分 =================
print("正在加载 YOLO 模型...")
model = YOLO('yolov8s.pt')
cap = cv2.VideoCapture('http://192.168.1.2:81/stream')

alert_state = False
person_frames = 0

# 封装一个安全的发送函数，防止发送到死连接报错
def send_to_esp32(msg):
    global esp32_socket
    if esp32_socket:
        try:
            esp32_socket.sendall(msg.encode('utf-8'))
        except Exception:
            print("⚠️ 发送失败，ESP32 可能已断开")
            esp32_socket = None

while True:
    ret, frame = cap.read()
    if not ret: break
    results = model(frame, classes=[0], conf=0.4, iou=0.4, verbose=False) 
    annotated_frame = results[0].plot()
    
    # 只有当置信度 > 0.4 且框的面积足够大时，才算一个人
    person_count = 0
    for box in results[0].boxes:
        # box.conf[0] 是置信度
        if box.conf[0] > 0.4: 
            person_count += 1
    
    if person_count > 0:
        person_frames += 1
    else:
        person_frames = max(0, person_frames - 1)

    if person_frames >= 3 and not alert_state:
        alert_state = True
        print("🚨 视觉警报：画面中出现人类！向 ESP32 发送开启指令")
        #   send_to_esp32("ALERT_ON\n")
            
    elif person_frames == 0 and alert_state:
        alert_state = False
        print("🟢 警报解除：画面安全。向 ESP32 发送关闭指令")
        #   send_to_esp32("ALERT_OFF\n")

    status_text = "ALERT!" if alert_state else "SAFE"
    status_color = (0, 0, 255) if alert_state else (0, 255, 0)
    cv2.putText(annotated_frame, f"Status: {status_text}", (20, 50), 
                cv2.FONT_HERSHEY_SIMPLEX, 1, status_color, 2)
    cv2.imshow("Smart Sensing System (Vision + Hardware)", annotated_frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()