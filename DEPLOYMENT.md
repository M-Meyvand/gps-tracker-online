# راهنمای Deploy سرور GPS Tracking

## روش 1: استفاده از Render (توصیه می‌شود)

### مرحله 1: آماده‌سازی
1. تمام فایل‌های پروژه را در یک پوشه قرار دهید
2. مطمئن شوید `package.json` و `gps_server.js` موجود هستند

### مرحله 2: آپلود به Render
1. به https://render.com بروید
2. ثبت‌نام کنید (رایگان)
3. "New +" کلیک کنید
4. "Web Service" انتخاب کنید
5. "Build and deploy from a Git repository" انتخاب کنید
6. پروژه خود را آپلود کنید

### مرحله 3: تنظیمات
- **Build Command**: `npm install`
- **Start Command**: `node gps_server.js`
- **Plan**: Free

### مرحله 4: دریافت آدرس
بعد از deploy، آدرسی مثل `https://your-app.onrender.com` دریافت می‌کنید.

### مرحله 5: به‌روزرسانی Arduino
در فایل `GPS_Tracker_HTTP.ino`:
```cpp
const String SERVER_URL = "https://your-app.onrender.com"; // آدرس Render
```

## روش 2: استفاده از Railway

### مرحله 1: آپلود به Railway
1. به https://railway.app بروید
2. ثبت‌نام کنید (رایگان)
3. "New Project" کلیک کنید
4. "Deploy from GitHub repo" انتخاب کنید
5. پروژه خود را آپلود کنید

### مرحله 2: دریافت آدرس
آدرسی مثل `https://your-project.railway.app` دریافت می‌کنید.

## روش 3: استفاده از Heroku

### مرحله 1: نصب Heroku CLI
```bash
npm install -g heroku
```

### مرحله 2: Login
```bash
heroku login
```

### مرحله 3: ایجاد App
```bash
heroku create your-app-name
```

### مرحله 4: Deploy
```bash
git add .
git commit -m "Initial commit"
git push heroku main
```

## تست
بعد از deploy، آدرس را در مرورگر باز کنید:
- `https://your-app.onrender.com` (Render)
- `https://your-project.railway.app` (Railway)
- `https://your-app-name.herokuapp.com` (Heroku)

## نکات مهم
- مطمئن شوید `package.json` شامل تمام dependencies است
- فایل `Procfile` برای Heroku ضروری است
- فایل `render.yaml` برای Render ضروری است
- فایل `railway.json` برای Railway ضروری است
