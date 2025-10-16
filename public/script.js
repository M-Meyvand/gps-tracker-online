// سیستم ردیابی GPS آنلاین - رابط کاربری
class GPSTracker {
    constructor() {
        this.map = null;
        this.currentDevice = null;
        this.trackLayer = null;
        this.markerLayer = null;
        this.socket = null;
        this.isTracking = false;
        this.trackData = [];
        this.alerts = [];
        
        this.initializeElements();
        this.initializeMap();
        this.initializeSocket();
        this.bindEvents();
        this.loadDevices();
    }

    initializeElements() {
        this.deviceSelect = document.getElementById('deviceSelect');
        this.timeRange = document.getElementById('timeRange');
        this.refreshBtn = document.getElementById('refreshBtn');
        this.clearTrackBtn = document.getElementById('clearTrackBtn');
        this.centerBtn = document.getElementById('centerBtn');
        this.fullscreenBtn = document.getElementById('fullscreenBtn');
        
        this.serverStatus = document.getElementById('serverStatus');
        this.deviceStatus = document.getElementById('deviceStatus');
        this.gpsStatus = document.getElementById('gpsStatus');
        
        this.totalPoints = document.getElementById('totalPoints');
        this.avgSpeed = document.getElementById('avgSpeed');
        this.trackDuration = document.getElementById('trackDuration');
        this.totalDistance = document.getElementById('totalDistance');
        
        this.alertsList = document.getElementById('alertsList');
    }

    initializeMap() {
        // ایجاد نقشه با مرکز تهران
        this.map = L.map('map').setView([35.6892, 51.3890], 13);
        
        // اضافه کردن لایه نقشه
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© OpenStreetMap contributors',
            maxZoom: 19
        }).addTo(this.map);
        
        // ایجاد لایه‌های مسیر و نشانگر
        this.trackLayer = L.layerGroup().addTo(this.map);
        this.markerLayer = L.layerGroup().addTo(this.map);
        
        console.log('نقشه راه‌اندازی شد');
    }

    initializeSocket() {
        this.socket = io();
        
        this.socket.on('connect', () => {
            console.log('اتصال به سرور برقرار شد');
            this.updateStatus(this.serverStatus, true);
        });
        
        this.socket.on('disconnect', () => {
            console.log('اتصال به سرور قطع شد');
            this.updateStatus(this.serverStatus, false);
        });
        
        this.socket.on('location_update', (data) => {
            this.handleLocationUpdate(data);
        });
        
        this.socket.on('alert', (data) => {
            this.handleAlert(data);
        });
    }

    bindEvents() {
        this.deviceSelect.addEventListener('change', () => {
            this.currentDevice = this.deviceSelect.value;
            if (this.currentDevice) {
                this.loadTrackData();
                this.socket.emit('join_device', this.currentDevice);
            }
        });
        
        this.timeRange.addEventListener('change', () => {
            if (this.currentDevice) {
                this.loadTrackData();
            }
        });
        
        this.refreshBtn.addEventListener('click', () => {
            this.refreshData();
        });
        
        this.clearTrackBtn.addEventListener('click', () => {
            this.clearTrack();
        });
        
        this.centerBtn.addEventListener('click', () => {
            this.centerMap();
        });
        
        this.fullscreenBtn.addEventListener('click', () => {
            this.toggleFullscreen();
        });
    }

    async loadDevices() {
        try {
            const response = await fetch('/api/devices');
            const devices = await response.json();
            
            this.deviceSelect.innerHTML = '<option value="">انتخاب دستگاه...</option>';
            
            devices.forEach(device => {
                const option = document.createElement('option');
                option.value = device.device_id;
                option.textContent = `${device.device_name || device.device_id} (${device.is_active ? 'فعال' : 'غیرفعال'})`;
                this.deviceSelect.appendChild(option);
            });
            
            if (devices.length > 0) {
                this.currentDevice = devices[0].device_id;
                this.deviceSelect.value = this.currentDevice;
                this.loadTrackData();
                this.socket.emit('join_device', this.currentDevice);
            }
            
        } catch (error) {
            console.error('خطا در بارگذاری دستگاه‌ها:', error);
            this.showError('خطا در بارگذاری دستگاه‌ها');
        }
    }

    async loadTrackData() {
        if (!this.currentDevice) return;
        
        try {
            const hours = this.timeRange.value;
            const response = await fetch(`/api/track/${this.currentDevice}?hours=${hours}`);
            const trackData = await response.json();
            
            this.trackData = trackData;
            this.displayTrack();
            this.updateStats();
            
        } catch (error) {
            console.error('خطا در بارگذاری مسیر:', error);
            this.showError('خطا در بارگذاری مسیر');
        }
    }

    displayTrack() {
        // پاک کردن لایه‌های قبلی
        this.trackLayer.clearLayers();
        this.markerLayer.clearLayers();
        
        if (this.trackData.length === 0) {
            this.showMessage('داده‌ای برای نمایش وجود ندارد');
            return;
        }
        
        // ایجاد مسیر
        const trackPoints = this.trackData.map(point => [point.latitude, point.longitude]);
        const trackLine = L.polyline(trackPoints, {
            color: '#3498db',
            weight: 4,
            opacity: 0.8
        });
        
        this.trackLayer.addLayer(trackLine);
        
        // اضافه کردن نشانگر شروع
        if (this.trackData.length > 0) {
            const startPoint = this.trackData[0];
            const startMarker = L.marker([startPoint.latitude, startPoint.longitude], {
                icon: L.divIcon({
                    className: 'start-marker',
                    html: '<i class="fas fa-play-circle" style="color: #27ae60; font-size: 20px;"></i>',
                    iconSize: [20, 20]
                })
            }).bindPopup('نقطه شروع');
            
            this.markerLayer.addLayer(startMarker);
        }
        
        // اضافه کردن نشانگر پایان
        if (this.trackData.length > 1) {
            const endPoint = this.trackData[this.trackData.length - 1];
            const endMarker = L.marker([endPoint.latitude, endPoint.longitude], {
                icon: L.divIcon({
                    className: 'end-marker',
                    html: '<i class="fas fa-stop-circle" style="color: #e74c3c; font-size: 20px;"></i>',
                    iconSize: [20, 20]
                })
            }).bindPopup('نقطه پایان');
            
            this.markerLayer.addLayer(endMarker);
        }
        
        // اضافه کردن نشانگر موقعیت فعلی
        if (this.trackData.length > 0) {
            const currentPoint = this.trackData[this.trackData.length - 1];
            const currentMarker = L.marker([currentPoint.latitude, currentPoint.longitude], {
                icon: L.divIcon({
                    className: 'current-marker',
                    html: '<i class="fas fa-map-marker-alt" style="color: #3498db; font-size: 24px;"></i>',
                    iconSize: [24, 24]
                })
            }).bindPopup(`
                <strong>موقعیت فعلی</strong><br>
                عرض: ${currentPoint.latitude.toFixed(6)}<br>
                طول: ${currentPoint.longitude.toFixed(6)}<br>
                سرعت: ${currentPoint.speed ? currentPoint.speed.toFixed(1) + ' km/h' : 'نامشخص'}<br>
                زمان: ${new Date(currentPoint.timestamp).toLocaleString('fa-IR')}
            `);
            
            this.markerLayer.addLayer(currentMarker);
        }
        
        // تنظیم نمای نقشه
        if (trackPoints.length > 0) {
            this.map.fitBounds(trackLine.getBounds(), { padding: [20, 20] });
        }
        
        this.updateStatus(this.gpsStatus, true);
    }

    handleLocationUpdate(data) {
        if (data.device_id === this.currentDevice) {
            // اضافه کردن نقطه جدید به مسیر
            this.trackData.push({
                latitude: data.latitude,
                longitude: data.longitude,
                speed: data.speed,
                course: data.course,
                timestamp: data.timestamp
            });
            
            // به‌روزرسانی نمایش
            this.displayTrack();
            this.updateStats();
            
            // به‌روزرسانی وضعیت
            this.updateStatus(this.deviceStatus, true);
        }
    }

    handleAlert(data) {
        if (data.device_id === this.currentDevice) {
            this.alerts.unshift({
                ...data,
                time: new Date().toLocaleString('fa-IR')
            });
            
            this.displayAlerts();
            this.showNotification(`هشدار جدید: ${data.alert_type}`);
        }
    }

    displayAlerts() {
        if (this.alerts.length === 0) {
            this.alertsList.innerHTML = '<div class="no-alerts">هشدار جدیدی وجود ندارد</div>';
            return;
        }
        
        this.alertsList.innerHTML = '';
        
        this.alerts.slice(0, 10).forEach(alert => {
            const alertDiv = document.createElement('div');
            alertDiv.className = `alert-item ${alert.alert_type}`;
            
            alertDiv.innerHTML = `
                <div class="alert-content">
                    <div class="alert-type">${this.getAlertTypeText(alert.alert_type)}</div>
                    <div class="alert-details">
                        فاصله: ${alert.distance ? alert.distance.toFixed(1) + ' متر' : 'نامشخص'}
                        ${alert.latitude && alert.longitude ? 
                            ` - موقعیت: ${alert.latitude.toFixed(6)}, ${alert.longitude.toFixed(6)}` : ''}
                    </div>
                </div>
                <div class="alert-time">${alert.time}</div>
            `;
            
            this.alertsList.appendChild(alertDiv);
        });
    }

    getAlertTypeText(type) {
        const types = {
            'boundary_breach': 'خروج از محدوده مجاز',
            'gps_lost': 'قطع سیگنال GPS',
            'device_offline': 'قطع اتصال دستگاه'
        };
        return types[type] || type;
    }

    updateStats() {
        if (this.trackData.length === 0) {
            this.totalPoints.textContent = '-';
            this.avgSpeed.textContent = '-';
            this.trackDuration.textContent = '-';
            this.totalDistance.textContent = '-';
            return;
        }
        
        // تعداد نقاط
        this.totalPoints.textContent = this.trackData.length;
        
        // سرعت متوسط
        const speeds = this.trackData.filter(p => p.speed > 0).map(p => p.speed);
        const avgSpeed = speeds.length > 0 ? speeds.reduce((a, b) => a + b, 0) / speeds.length : 0;
        this.avgSpeed.textContent = avgSpeed.toFixed(1);
        
        // مدت ردیابی
        const startTime = this.trackData[0].timestamp;
        const endTime = this.trackData[this.trackData.length - 1].timestamp;
        const duration = (endTime - startTime) / 1000 / 60; // دقیقه
        this.trackDuration.textContent = this.formatDuration(duration);
        
        // مسافت کل
        let totalDistance = 0;
        for (let i = 1; i < this.trackData.length; i++) {
            const prev = this.trackData[i - 1];
            const curr = this.trackData[i];
            totalDistance += this.calculateDistance(prev.latitude, prev.longitude, curr.latitude, curr.longitude);
        }
        this.totalDistance.textContent = (totalDistance / 1000).toFixed(2);
    }

    calculateDistance(lat1, lng1, lat2, lng2) {
        const R = 6371000; // شعاع زمین بر حسب متر
        const dLat = this.toRadians(lat2 - lat1);
        const dLng = this.toRadians(lng2 - lng1);
        
        const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
                Math.cos(this.toRadians(lat1)) * Math.cos(this.toRadians(lat2)) *
                Math.sin(dLng/2) * Math.sin(dLng/2);
        
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
        return R * c;
    }

    toRadians(degrees) {
        return degrees * (Math.PI / 180);
    }

    formatDuration(minutes) {
        if (minutes < 60) {
            return `${Math.round(minutes)} دقیقه`;
        } else if (minutes < 1440) {
            return `${Math.round(minutes / 60)} ساعت`;
        } else {
            return `${Math.round(minutes / 1440)} روز`;
        }
    }

    updateStatus(element, connected) {
        element.classList.remove('connected', 'disconnected');
        element.classList.add(connected ? 'connected' : 'disconnected');
    }

    refreshData() {
        this.refreshBtn.innerHTML = '<div class="loading"></div> در حال بارگذاری...';
        this.loadTrackData().finally(() => {
            this.refreshBtn.innerHTML = '<i class="fas fa-sync-alt"></i> به‌روزرسانی';
        });
    }

    clearTrack() {
        this.trackLayer.clearLayers();
        this.markerLayer.clearLayers();
        this.trackData = [];
        this.updateStats();
        this.showMessage('مسیر پاک شد');
    }

    centerMap() {
        if (this.trackData.length > 0) {
            const trackPoints = this.trackData.map(point => [point.latitude, point.longitude]);
            const trackLine = L.polyline(trackPoints);
            this.map.fitBounds(trackLine.getBounds(), { padding: [20, 20] });
        } else {
            this.map.setView([35.6892, 51.3890], 13);
        }
    }

    toggleFullscreen() {
        const mapContainer = document.querySelector('.map-container');
        if (!document.fullscreenElement) {
            mapContainer.requestFullscreen();
        } else {
            document.exitFullscreen();
        }
    }

    showMessage(message) {
        // ایجاد پیام موقت
        const messageDiv = document.createElement('div');
        messageDiv.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: #3498db;
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
            z-index: 1000;
            font-weight: 500;
        `;
        messageDiv.textContent = message;
        document.body.appendChild(messageDiv);
        
        setTimeout(() => {
            document.body.removeChild(messageDiv);
        }, 3000);
    }

    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: #e74c3c;
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
            z-index: 1000;
            font-weight: 500;
        `;
        errorDiv.textContent = message;
        document.body.appendChild(errorDiv);
        
        setTimeout(() => {
            document.body.removeChild(errorDiv);
        }, 5000);
    }

    showNotification(message) {
        // ایجاد نوتیفیکیشن
        const notification = document.createElement('div');
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            left: 20px;
            background: #f39c12;
            color: white;
            padding: 15px 20px;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0,0,0,0.15);
            z-index: 1000;
            font-weight: 500;
            animation: slideIn 0.3s ease-out;
        `;
        notification.textContent = message;
        document.body.appendChild(notification);
        
        setTimeout(() => {
            notification.style.animation = 'slideOut 0.3s ease-in';
            setTimeout(() => {
                document.body.removeChild(notification);
            }, 300);
        }, 4000);
    }
}

// اضافه کردن استایل‌های انیمیشن
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from { transform: translateX(-100%); opacity: 0; }
        to { transform: translateX(0); opacity: 1; }
    }
    
    @keyframes slideOut {
        from { transform: translateX(0); opacity: 1; }
        to { transform: translateX(-100%); opacity: 0; }
    }
`;
document.head.appendChild(style);

// راه‌اندازی سیستم
document.addEventListener('DOMContentLoaded', () => {
    new GPSTracker();
});
