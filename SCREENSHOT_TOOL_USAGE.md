# Quick Start: Screenshot Capture Tool

## Installation (One-time setup)

```bash
# Install Python dependencies
pip install -r requirements-screenshots.txt
```

## Usage

### Step 1: Power on your TTGO T-Beam device
- Ensure device is connected to WiFi
- Note the IP address (shown on OLED or Serial Monitor)

### Step 2: Run the screenshot script

```bash
# Default IP (10.0.4.80) - Full-page screenshots
python3 capture_screenshots.py

# Custom IP
python3 capture_screenshots.py --ip 192.168.1.100

# Viewport-only (faster, but may cut off content)
python3 capture_screenshots.py --viewport-only

# Custom output directory
python3 capture_screenshots.py --output my-screenshots
```

**Note:** By default, the script captures **full-page screenshots** that include all content, even if it requires scrolling. This ensures you capture complete pages with all sections visible.

### Step 3: Review screenshots

Screenshots will be saved to `screenshots/` directory:
- `dashboard.png`
- `settings.png`
- `config.png`
- `payload-info.png`
- `diagnostics.png`
- `debug.png`
- `about.png`

## Troubleshooting

### ChromeDriver not found
```bash
# macOS
brew install chromedriver

# Or use automatic driver management
pip install webdriver-manager
```

### Cannot connect to device
- Verify device IP: `ping 10.0.4.80`
- Check WiFi connection
- Ensure device is powered on

## Next Steps

1. Review screenshots in `screenshots/` folder
2. Add to README.md (see SCREENSHOTS.md for template)
3. Commit to git:
   ```bash
   git add screenshots/ capture_screenshots.py requirements-screenshots.txt
   git commit -m "Add web UI screenshots and capture tool"
   git push
   ```

---

For detailed instructions, see [SCREENSHOTS.md](SCREENSHOTS.md)