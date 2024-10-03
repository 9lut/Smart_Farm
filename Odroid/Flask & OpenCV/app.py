from flask import Flask, Response, render_template, jsonify
import cv2
import requests
import numpy as np
import time
import odroid_wiringpi as wp

app = Flask(__name__)

# Load the pre-trained face detection model
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Open the webcam
def open_camera():
    cam = cv2.VideoCapture(0)
    return cam

# LINE Notify access token
LINE_NOTIFY_TOKEN = 'rflYgaNiQZTmak28iBN1cNtIBOfEFxxOyxzoBsao67Y'
LINE_NOTIFY_URL = 'https://notify-api.line.me/api/notify'

# Throttle settings
LAST_SENT_TIME = 0
THROTTLE_INTERVAL = 1  # Increase this interval to reduce rate limit issues

# Global flags
face_in_frame = False
previous_face_count = 0  # To store the last detected face count
detection_enabled = False  # Flag to control face detection

# GPIO setup
TRIG = 3  # WiringPi pin 3
ECHO = 4  # WiringPi pin 4
wp.wiringPiSetup()
wp.pinMode(TRIG, wp.OUTPUT)
wp.pinMode(ECHO, wp.INPUT)

def get_distance():
    # Send a trigger pulse
    wp.digitalWrite(TRIG, wp.HIGH)
    time.sleep(0.00001)  # 10 microseconds trigger pulse
    wp.digitalWrite(TRIG, wp.LOW)

    # Wait for echo to start (echo pin goes HIGH)
    while wp.digitalRead(ECHO) == wp.LOW:
        pass
    start_time = time.time()

    # Wait for echo to stop (echo pin goes LOW)
    while wp.digitalRead(ECHO) == wp.HIGH:
        pass
    stop_time = time.time()

    # Calculate the distance based on the time the signal took
    elapsed_time = stop_time - start_time
    distance = (elapsed_time * 34300) / 2  # Speed of sound = 343 m/s
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
    data = {
        'message': 'Face detected!'  # Add a message here
    }
    files = {
        'imageFile': ('image.jpg', img_bytes, 'image/jpeg')
    }
    try:
        response = requests.post(LINE_NOTIFY_URL, headers=headers, data=data, files=files)
        if response.status_code == 200:
            print("Image sent to LINE successfully!")
            LAST_SENT_TIME = current_time  # Update last sent time on success
        else:
            print("Failed to send image to LINE:", response.status_code, response.text)
    except requests.exceptions.RequestException as e:
        print(f"Request failed: {e}")

def generate_frames():
    global face_in_frame, previous_face_count, detection_enabled
    cam = open_camera()  # Initialize the camera once
    if not cam.isOpened():
        print("Error: Camera could not be opened.")
        return  # Exit if the camera couldn't be opened

    while True:
        # Check distance every loop
        distance = get_distance()
        if distance is None:  # Check if distance measurement is valid
            print("Error: Distance measurement failed.")
            continue

        if distance > 100:
            # If distance is greater than 100, enter sleep mode and check every 10 seconds
            print("Distance greater than 100. Entering sleep mode...")
            #time.sleep(10)  # Sleep for 10 seconds before rechecking the distance
            cam.release()
            continue  # Skip processing the frame during sleep mode

        # Once the distance is within range, ensure the camera is open
        if not cam.isOpened():
            cam = open_camera()  # Reopen the camera if it's closed
            if not cam.isOpened():
                print("Error: Camera could not be opened.")
                return  # Exit if the camera couldn't be reopened

        # Capture a frame from the camera
        success, frame = cam.read()
        if not success:
            print("Error: Failed to capture frame.")
            break  # Exit if frame capture fails

        # Convert the frame to grayscale
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # Detect faces if detection is enabled
        if detection_enabled:
            faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

            for (x, y, w, h) in faces:
                cv2.rectangle(frame, (x, y), (x+w, y+h), (255, 0, 0), 2)

            face_count = len(faces)
            cv2.putText(frame, f"Faces detected: {face_count}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)

            if face_count > 0 and (not face_in_frame or face_count > previous_face_count):
                face_in_frame = True
                send_image_to_line(frame)
            elif face_count == 0:
                face_in_frame = False

            previous_face_count = face_count

        # Encode the frame in JPEG format
        _, buffer = cv2.imencode('.jpg', frame)
        frame = buffer.tobytes()

        # Yield the frame in the required format
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

        # Optional: Sleep for a short while to reduce CPU usage
        time.sleep(0.1)  # Adjust as needed


@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/toggle_detection', methods=['POST'])
def toggle_detection():
    global detection_enabled
    detection_enabled = not detection_enabled
    status = "enabled" if detection_enabled else "disabled"
    return jsonify({"status": status})

@app.route('/status')
def status():
    # Check for faces and return status
    face_count, _ = detect_faces()
    return jsonify({"active": face_count > 0})

@app.route('/is_face_detected')
def is_face_detected():
    face_count, _ = detect_faces()
    return jsonify({"face_detected": face_count > 0})

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

# Modify the app run logic here
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
