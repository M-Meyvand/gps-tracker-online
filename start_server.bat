@echo off
chcp 65001 >nul
title سیستم ردیابی GPS آنلاین

echo.
echo ========================================
echo    سیستم ردیابی GPS آنلاین
echo ========================================
echo.

echo در حال بررسی Node.js...
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ❌ خطا: Node.js نصب نیست!
    echo لطفاً از سایت nodejs.org دانلود و نصب کنید
    pause
    exit /b 1
)

echo ✅ Node.js یافت شد
echo.

echo در حال بررسی وابستگی‌ها...
if not exist "node_modules" (
    echo 📦 نصب وابستگی‌ها...
    npm install
    if %errorlevel% neq 0 (
        echo ❌ خطا در نصب وابستگی‌ها
        pause
        exit /b 1
    )
    echo ✅ وابستگی‌ها نصب شدند
) else (
    echo ✅ وابستگی‌ها موجود هستند
)

echo.
echo 🚀 شروع سرور...
echo.
echo آدرس: http://localhost:3000
echo نقشه: http://localhost:3000/map
echo.
echo برای توقف سرور: Ctrl+C
echo.

npm start

pause
