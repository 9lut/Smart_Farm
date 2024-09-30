from flask import Flask, Response, render_template, jsonify, request
import cv2
import threading
import requests
import numpy as np
import time
import datetime
import odroid_wiringpi as wp  # Import WiringPi for Odroid

app = Flask(__name__)

# โหลดโมเดลตรวจจับใบหน้า
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# เปิดกล้องด้วยความละเอียดที่ลดลง
cam = cv2.VideoCapture(0)
cam.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
cam.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)

# LINE Notify access token
LINE_NOTIFY_TOKEN = 'rflYgaNiQZTmak28iBN1cNtIBOfEFxxOyxzoBsao67Y'
LINE_NOTIFY_URL = 'https://notify-api.line.me/api/notify'

# ตั้งค่า Throttle เพื่อลดการแจ้งเตือนบ่อยเกินไป
LAST_SENT_TIME = 0
THROTTLE_INTERVAL = 10  # ส่งแจ้งเตือนทุก 10 วินาที

# Global flags to track face presence and detection state
face_in_frame = False
previous_face_count = 0  # เก็บค่าจำนวนใบหน้าที่ตรวจจับได้ครั้งล่าสุด
detection_active = True  # ตัวแปรเพื่อควบคุมการตรวจจับใบหน้า
frame_count = 0  # นับจำนวนเฟรมเพื่อข้ามเฟรมที่ไม่ต้องตรวจจับ

# GPIO pin setup for Ultrasonic Sensor (WiringPi numbering)
TRIG = 3  # WiringPi pin 3 (Physical pin 15, GPIO 22)
ECHO = 4  # WiringPi pin 4 (Physical pin 16, GPIO 23)

# Setup GPIO using Odroid WiringPi
wp.wiringPiSetup()
wp.pinMode(TRIG, wp.OUTPUT)
wp.pinMode(ECHO, wp.INPUT)

# Ensure the trigger pin is low at the start
wp.digitalWrite(TRIG, wp.LOW)
time.sleep(0.02)

def get_distance():
    # ส่งสัญญาณ Trigger
    wp.digitalWrite(TRIG, wp.HIGH)
    time.sleep(0.00001)
    wp.digitalWrite(TRIG, wp.LOW)

    # รอ Echo Pin ไปที่ HIGH
    while wp.digitalRead(ECHO) == wp.LOW:
        pass
    start_time = time.time()

    # รอ Echo Pin ไปที่ LOW
    while wp.digitalRead(ECHO) == wp.HIGH:
        pass
    stop_time = time.time()

    # คำนวณระยะทาง
    elapsed_time = stop_time - start_time
    distance = (elapsed_time * 34300) / 2  # ความเร็วเสียง = 343 m/s
    return distance

def send_image_to_line(image):
    global LAST_SENT_TIME
    current_time = time.time()
    if current_time - LAST_SENT_TIME < THROTTLE_INTERVAL:
        print("Rate limit exceeded. Waiting before sending next image.")
        return
    
    _, img_encoded = cv2.imencode('.jpg', image)
    if not _:
        print("Failed to encode image.")
        return
    
    img_bytes = img_encoded.tobytes()
    headers = {
        'Authorization': f'Bearer {LINE_NOTIFY_TOKEN}'
    }
    # ส่งข้อความพร้อมเวลา
    message = f"Face detected! Time: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
    data = {
        'message': message
    }
    files = {
        'imageFile': ('image.jpg', img_bytes, 'image/jpeg')
    }
    try:
        response = requests.post(LINE_NOTIFY_URL, headers=headers, data=data, files=files)
        if response.status_code == 200:
            print("Image sent to LINE successfully!")
            LAST_SENT_TIME = current_time  # อัปเดตเวลาที่ส่งครั้งล่าสุด
        else:
            print("Failed to send image to LINE:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

def detect_faces(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
    return faces

def generate_frames():
    global face_in_frame, previous_face_count, detection_active, frame_count
    while True:
        # อ่านเฟรมจากกล้อง
        success, frame = cam.read()
        if not success:
            break

        frame_count += 1

        # วัดระยะจากเซ็นเซอร์อัลตราโซนิก
        distance = get_distance()
        print(f"Distance: {distance:.2f} cm")

        # ตรวจจับใบหน้าเฉพาะเมื่อระยะน้อยกว่า 100 ซม.
        if distance < 100:
            if detection_active and frame_count % 5 == 0:  # ตรวจจับทุก 5 เฟรม
                threading.Thread(target=detect_faces, args=(frame,)).start()

                # ตรวจจับใบหน้า
                faces = detect_faces(frame)

                # วาดกรอบรอบใบหน้า
                for (x, y, w, h) in faces:
                    cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)

                # จำนวนใบหน้าที่ตรวจจับได้
                face_count = len(faces)
                cv2.putText(frame, f"Faces detected: {face_count}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)

                # ถ้าตรวจจับได้ใบหน้าใหม่ ส่งภาพไปยัง LINE
                if face_count > 0 and (not face_in_frame or face_count > previous_face_count):
                    face_in_frame = True
                    send_image_to_line(frame)
                elif face_count == 0:
                    face_in_frame = False

                previous_face_count = face_count
        else:
            # แสดงโหมด SleepMode เมื่อระยะเกิน 100 ซม.
            frame = np.zeros_like(frame)
            cv2.putText(frame, "SleepMode", (100, 200), cv2.FONT_HERSHEY_SIMPLEX, 2, (0, 255, 0), 5, cv2.LINE_AA)

        # เข้ารหัสเฟรมเป็น JPEG
        _, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/status')
def status():
    return jsonify({"active": detection_active})

@app.route('/start_detection')
def start_detection():
    global detection_active
    detection_active = True
    return jsonify({"status": "Detection started"})

@app.route('/stop_detection')
def stop_detection():
    global detection_active
    detection_active = False
    return jsonify({"status": "Detection stopped"})

@app.route('/send_test_message')
def send_test_message():
    send_message_to_line("Test message")
    return "Test message sent!"

def send_message_to_line(message):
    headers = {
        'Authorization': f'Bearer {LINE_NOTIFY_TOKEN}'
    }
    data = {
        'message': message
    }
    try:
        response = requests.post(LINE_NOTIFY_URL, headers=headers, data=data)
        if response.status_code == 200:
            print("Message sent to LINE successfully!")
        else:
            print("Failed to send message to LINE:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
