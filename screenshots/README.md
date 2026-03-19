# Web Dashboard Screenshots

This directory contains screenshots of the TTGO T-Beam web dashboard.

## Generating Screenshots

Use the automated screenshot capture tool:

```bash
# Install dependencies
pip install -r requirements-screenshots.txt

# Capture all pages
python3 capture_screenshots.py
```

See [../SCREENSHOTS.md](../SCREENSHOTS.md) for detailed instructions.

## Expected Files

After running the capture script, this directory will contain:

- `dashboard.png` - Main dashboard with sensor data and GPS map
- `settings.png` - System settings page
- `config.png` - LoRaWAN configuration page
- `payload-info.png` - Payload channel configuration
- `diagnostics.png` - System diagnostics and statistics
- `debug.png` - Live debug console
- `about.png` - About page with developer info

## Manual Screenshots

If you prefer to capture screenshots manually, see the [manual capture guide](../SCREENSHOTS.md#manual-screenshot-capture-alternative).

---

**Note:** Screenshots are not included in the repository by default. Run the capture script to generate them locally, or capture them manually using your browser.