# Resistor Calculator - Mobile Apps

Native Android and iOS apps using Capacitor.

## Prerequisites

- Node.js 18+
- For Android: Android Studio, Android SDK
- For iOS: Xcode 14+, macOS

## Setup

```bash
cd mobile

# Install dependencies
npm install

# Create www directory and copy web assets
mkdir -p www
npm run build

# Initialize platforms
npx cap add android
npx cap add ios

# Generate icons (requires librsvg2-bin or imagemagick)
npm run icons

# Sync web assets to native projects
npm run sync
```

## Building

### Android

```bash
# Open in Android Studio
npm run android

# Or build APK directly
npm run android:build
# APK: android/app/build/outputs/apk/release/app-release.apk
```

### iOS

```bash
# Open in Xcode (macOS only)
npm run ios

# Then build via Xcode:
# 1. Select target device
# 2. Product > Archive
# 3. Distribute App
```

## Development

After making changes to the web version:

```bash
npm run build   # Copy web files to www/
npm run sync    # Sync to native projects
```

## App Store Distribution

### Android (Google Play)

1. Generate signing key:
   ```bash
   keytool -genkey -v -keystore resistorcal.keystore -alias resistorcal -keyalg RSA -keysize 2048 -validity 10000
   ```

2. Configure signing in `android/app/build.gradle`

3. Build release APK/AAB:
   ```bash
   cd android
   ./gradlew bundleRelease
   ```

### iOS (App Store)

1. Open in Xcode
2. Configure signing with your Apple Developer account
3. Archive and upload to App Store Connect

## Updating Icons

Edit the SVG source at `../data/icons/resistorcal.svg`, then:

```bash
npm run icons
npm run sync
```
