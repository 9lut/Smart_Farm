<h1 align="center">
  <br>
  <a><img src="https://github.com/user-attachments/assets/e796e970-87f9-4e10-8976-c0c652dc1ea0" alt="Markdownify" width="300"></a>
  <br>
  SmartFarm
  <br>
</h1>

<h4 align="center">ฟาร์มอัจฉริยะ  By Hayday Group </h4>

<p align="center">
  <a href="#about-us">About Us</a> •
    <a href="#tools">Tools</a> •
  <a href="#how-to-use">How To Use</a> •
  <a href="#function">Function</a> •
  <a href="#our-team">Our Team</a> 
</p>
<h4>วงจรต่อจริง</h4>

![CC](https://github.com/user-attachments/assets/165d0d68-226d-4c6e-8a11-d7a48c611c52)

<h4>Schematic diagram</h4>

![รูปภาพ1](https://github.com/user-attachments/assets/2029b8c5-16bc-40d6-9a02-1c93e562465d)




## About Us
ระบบฟาร์มอัจฉริยะ (SmartFarm) ซึ่งเป็นการนำเทคโนโลยีมาประยุกต์ใช้ในการทำการเกษตร โดยมีวัตถุประสงค์เพื่อลดต้นทุน และสร้างความยั่งยืนให้กับภาคการเกษตร โครงงานนี้จะมุ่งเน้นการพัฒนาระบบควบคุมและตรวจสอบสภาพแวดล้อมในฟาร์มแบบอัตโนมัติ โดยใช้เทคโนโลยี Internet of Things (IoT) เพื่อเก็บข้อมูลและวิเคราะห์ปัจจัยต่าง ๆ

## Tools
 1. Arduino IDE
 2. Node-red
 3. Flask
 4. OpenCV
 5. PostgreSQL
 6. Ngrok
 7. Line Notify

## How To Use
**ต้องติดตั้ง Node-red สำหรับการใช้เว็บแดชบอร์ด**

 - Node-red

*Install Node-red on Odroid-C4*
```bash
$ bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)
```
 - PostgreSQL

*install PostgreSQL on Odroid-C4*
```
$ sudo apt install postgresql postgresql-contrib
```
**Create Database & Table in PostgreSQL*
```
CREATE TABLE Sensor_Data (
    id BIGINT PRIMARY KEY,
    Humidity FLOAT, 
    Temperature FLOAT, 
    Soil_Moisture INT, 
    Water_Level INT, 
    Light_Sensor INT, 
    Date TEXT, 
    Time TEXT
);
```
## Sensor

 - DHT11 เซนเซอร์วัดอุณหภูมิและความชื้นสัมพัทธ์
 - Water level sensor เซนเซอร์วัดระดับน้ำ
 - Light sensor เซนเซอร์วัดความสว่าง
 - Soil moisture Sensor เซนเซอร์วัดความชื้นในดิน
## Function

> สามารถดูค่าจากเซนเซอร์ต่างๆ ได้แก่ อุณหภูมิ , ความชื้นสัมพัทธ์ , ความชื้นในดิน, ระดับน้ำ และความสว่าง
> 
> สามารถดูค่าและความคุมอุปกรณ์แบบ Real-time ผ่านแดชบอร์ด Node-red
> 
> สามารถดูข้อมูลย้อนหลังในแต่ละวันได้
> 
> สามารถรดน้ำ , เปิดปิดไฟ , เปิดปิดพัดลม อัตโนมัติ ถ้าถึงค่าที่กำหนดไว้
> 
> สามารถดูค่าต่างๆผ่าน LCD และสามารถกำหนดค่าต่างๆ


## Our Team

 - **Lutfee Dorloh** 
 - **Wannasak Nooplod**

