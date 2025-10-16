const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const cors = require('cors');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "*",
    methods: ["GET", "POST"]
  }
});

const PORT = 3001;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static('public'));

// راه‌اندازی دیتابیس SQLite
const db = new sqlite3.Database('./gps_tracking.db', (err) => {
  if (err) {
    console.error('خطا در اتصال به دیتابیس:', err.message);
  } else {
    console.log('✅ دیتابیس SQLite متصل شد');
    initializeDatabase();
  }
});

// ایجاد جداول دیتابیس
function initializeDatabase() {
  db.serialize(() => {
    // جدول موقعیت‌ها
    db.run(`CREATE TABLE IF NOT EXISTS locations (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      device_id TEXT NOT NULL,
      latitude REAL NOT NULL,
      longitude REAL NOT NULL,
      speed REAL DEFAULT 0,
      course REAL DEFAULT 0,
      timestamp INTEGER NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    // جدول هشدارها
    db.run(`CREATE TABLE IF NOT EXISTS alerts (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      device_id TEXT NOT NULL,
      alert_type TEXT NOT NULL,
      distance REAL,
      latitude REAL,
      longitude REAL,
      timestamp INTEGER NOT NULL,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    // جدول دستگاه‌ها
    db.run(`CREATE TABLE IF NOT EXISTS devices (
      device_id TEXT PRIMARY KEY,
      device_name TEXT,
      is_active BOOLEAN DEFAULT 1,
      last_seen DATETIME,
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    )`);

    console.log('✅ جداول دیتابیس ایجاد شدند');
  });
}

// ذخیره موقعیت جدید
app.post('/api/location', (req, res) => {
  const { device_id, latitude, longitude, speed, course, timestamp, valid } = req.body;
  
  if (!device_id || !latitude || !longitude) {
    return res.status(400).json({ error: 'داده‌های ناقص' });
  }

  const stmt = db.prepare(`
    INSERT INTO locations (device_id, latitude, longitude, speed, course, timestamp)
    VALUES (?, ?, ?, ?, ?, ?)
  `);

  stmt.run([device_id, latitude, longitude, speed || 0, course || 0, timestamp || Date.now()], function(err) {
    if (err) {
      console.error('خطا در ذخیره موقعیت:', err);
      return res.status(500).json({ error: 'خطا در ذخیره موقعیت' });
    }

    // به‌روزرسانی آخرین دیده شدن دستگاه
    updateDeviceLastSeen(device_id);

    // ارسال موقعیت به کلاینت‌های متصل
    io.emit('location_update', {
      device_id,
      latitude,
      longitude,
      speed,
      course,
      timestamp: timestamp || Date.now()
    });

    console.log(`📍 موقعیت ذخیره شد: ${device_id} - ${latitude}, ${longitude}`);
    res.json({ success: true, id: this.lastID });
  });

  stmt.finalize();
});

// ذخیره هشدار
app.post('/api/alert', (req, res) => {
  const { device_id, alert_type, distance, latitude, longitude, timestamp } = req.body;
  
  if (!device_id || !alert_type) {
    return res.status(400).json({ error: 'داده‌های ناقص' });
  }

  const stmt = db.prepare(`
    INSERT INTO alerts (device_id, alert_type, distance, latitude, longitude, timestamp)
    VALUES (?, ?, ?, ?, ?, ?)
  `);

  stmt.run([device_id, alert_type, distance || 0, latitude || 0, longitude || 0, timestamp || Date.now()], function(err) {
    if (err) {
      console.error('خطا در ذخیره هشدار:', err);
      return res.status(500).json({ error: 'خطا در ذخیره هشدار' });
    }

    // ارسال هشدار به کلاینت‌های متصل
    io.emit('alert', {
      device_id,
      alert_type,
      distance,
      latitude,
      longitude,
      timestamp: timestamp || Date.now()
    });

    console.log(`🚨 هشدار ذخیره شد: ${device_id} - ${alert_type}`);
    res.json({ success: true, id: this.lastID });
  });

  stmt.finalize();
});

// دریافت آخرین موقعیت دستگاه
app.get('/api/location/:device_id', (req, res) => {
  const deviceId = req.params.device_id;
  
  db.get(`
    SELECT * FROM locations 
    WHERE device_id = ? 
    ORDER BY timestamp DESC 
    LIMIT 1
  `, [deviceId], (err, row) => {
    if (err) {
      console.error('خطا در دریافت موقعیت:', err);
      return res.status(500).json({ error: 'خطا در دریافت موقعیت' });
    }

    if (!row) {
      return res.status(404).json({ error: 'موقعیت یافت نشد' });
    }

    res.json(row);
  });
});

// دریافت مسیر حرکت دستگاه
app.get('/api/track/:device_id', (req, res) => {
  const deviceId = req.params.device_id;
  const hours = req.query.hours || 24; // پیش‌فرض 24 ساعت گذشته
  
  const startTime = Date.now() - (hours * 60 * 60 * 1000);
  
  db.all(`
    SELECT latitude, longitude, speed, course, timestamp, created_at
    FROM locations 
    WHERE device_id = ? AND timestamp >= ?
    ORDER BY timestamp ASC
  `, [deviceId, startTime], (err, rows) => {
    if (err) {
      console.error('خطا در دریافت مسیر:', err);
      return res.status(500).json({ error: 'خطا در دریافت مسیر' });
    }

    res.json(rows);
  });
});

// دریافت لیست دستگاه‌ها
app.get('/api/devices', (req, res) => {
  db.all(`
    SELECT d.*, l.latitude, l.longitude, l.timestamp as last_location_time
    FROM devices d
    LEFT JOIN locations l ON d.device_id = l.device_id
    WHERE l.timestamp = (
      SELECT MAX(timestamp) FROM locations WHERE device_id = d.device_id
    )
    ORDER BY d.created_at DESC
  `, (err, rows) => {
    if (err) {
      console.error('خطا در دریافت دستگاه‌ها:', err);
      return res.status(500).json({ error: 'خطا در دریافت دستگاه‌ها' });
    }

    res.json(rows);
  });
});

// دریافت آمار
app.get('/api/stats/:device_id', (req, res) => {
  const deviceId = req.params.device_id;
  const hours = req.query.hours || 24;
  const startTime = Date.now() - (hours * 60 * 60 * 1000);
  
  db.get(`
    SELECT 
      COUNT(*) as total_points,
      MIN(timestamp) as start_time,
      MAX(timestamp) as end_time,
      AVG(speed) as avg_speed,
      MAX(speed) as max_speed
    FROM locations 
    WHERE device_id = ? AND timestamp >= ?
  `, [deviceId, startTime], (err, row) => {
    if (err) {
      console.error('خطا در دریافت آمار:', err);
      return res.status(500).json({ error: 'خطا در دریافت آمار' });
    }

    res.json(row);
  });
});

// به‌روزرسانی آخرین دیده شدن دستگاه
function updateDeviceLastSeen(deviceId) {
  db.run(`
    INSERT OR REPLACE INTO devices (device_id, last_seen)
    VALUES (?, CURRENT_TIMESTAMP)
  `, [deviceId], (err) => {
    if (err) {
      console.error('خطا در به‌روزرسانی دستگاه:', err);
    }
  });
}

// Socket.IO برای Real-time updates
io.on('connection', (socket) => {
  console.log('👤 کلاینت متصل شد:', socket.id);
  
  socket.on('join_device', (deviceId) => {
    socket.join(deviceId);
    console.log(`📱 کلاینت به دستگاه ${deviceId} پیوست`);
  });
  
  socket.on('disconnect', () => {
    console.log('👤 کلاینت قطع شد:', socket.id);
  });
});

// صفحه اصلی
app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// شروع سرور
server.listen(PORT, '0.0.0.0', () => {
  console.log(`🚀 سرور GPS Tracking در حال اجرا در پورت ${PORT}`);
  console.log(`🌐 آدرس محلی: http://localhost:${PORT}`);
  console.log(`🌐 آدرس عمومی: http://104.167.27.85:${PORT}`);
  console.log(`📡 API: http://104.167.27.85:${PORT}/api`);
  console.log(`🗺️ نقشه: http://104.167.27.85:${PORT}/map`);
});

// مدیریت خروج
process.on('SIGINT', () => {
  console.log('\n🛑 در حال بستن سرور...');
  db.close((err) => {
    if (err) {
      console.error('خطا در بستن دیتابیس:', err.message);
    } else {
      console.log('✅ دیتابیس بسته شد');
    }
    process.exit(0);
  });
});

module.exports = app;
